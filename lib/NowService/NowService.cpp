#include <WiFi.h>
#include <iostream>
#include <cstring>

#include "esp_wifi.h"
#include "esp_now.h"
#include "NowService.h"

const uint8_t broadcastMac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

NowService *instance;
uint8_t *macAddress;
int serviceMode = None;

unsigned int advertiseInterval = 1000;
unsigned long advertiseTicks = 0;
unsigned long advertisePeriod = 300000;     //  default 5 mins
unsigned long advertiseStart = 0;

unsigned long discoveryStart = 0;
unsigned long discoveryPeriod = 0;          //  default to 0 = indefinite

unsigned long lastTick = 0;

#pragma region NowService interface

#pragma region Prototypes

void worker(void *pvParameters);
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);

#pragma endregion Prototypes

NowService::NowService() 
{
    instance = this;
    xTaskCreatePinnedToCore(worker, "Worker Loop", 2048, NULL, 1, NULL, 1); // Pinned to core 1
}

NowService::~NowService()
{
    delete(instance);
}

void NowService::initialize(PeerFoundCallback callback, bool isServer) 
{
    serviceMode |= (isServer)? Server : Client;
    //  initialize wifi first
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    //  register callbacks
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(esp_now_recv_cb_t(onDataReceived));

    serviceMode = Initialized;
}

void NowService::beginAdverise(uint8_t *mac, int interval, unsigned long period)
{
    advertiseInterval = (interval > 0)? interval : advertiseInterval;
    macAddress = mac;
    serviceMode |= Advertise;
    advertisePeriod = period;
    advertiseStart = millis();
}

void NowService::endAdvertise() 
{
    serviceMode ^= Advertise;
}

void NowService::beginDiscovery(unsigned long period)
{
    discoveryStart = millis();
    discoveryPeriod = period;
    serviceMode |= Discovery;
}

void NowService::endDiscovery()
{
    serviceMode ^= Discovery;
}

bool NowService::sendData(uint8_t *mac, uint8_t *data, int length)
{
    esp_err_t result = esp_now_send(mac, data, length);
    return (result == ESP_OK)? true : false;
}

void NowService::broadcastData(uint8_t *data, int length)
{
    instance->sendData((uint8_t*)broadcastMac, data, length);
}

#pragma endregion NowService interface

#pragma region Helpers

void advertise() 
{
    DiscoveryInfo info;
    info.time = millis();
    memcpy(info.macAddress, macAddress, 6);
    instance->sendData((uint8_t *)broadcastMac, (uint8_t *)&info, sizeof(info));
}


bool macEquals(const uint8_t *mac1, const uint8_t *mac2) 
{
    return memcmp(mac1, mac2, 6) == 0;
}

#pragma endregion Helpers

#pragma region Worker Loop

//  this method should be executed on core 1 so as not to interfere with WIFI and ESP-NOW functions
void worker(void *pvParameters)
{
    while(serviceMode != ServiceMode::Terminate)
    {
        unsigned long now = millis();
        unsigned long ticks = now - lastTick;
        lastTick = now;
        //  must we advertise
        if (serviceMode & Advertise == Advertise)
        {
            advertiseTicks += ticks;
            if (advertiseTicks > advertiseInterval)
            {
                advertiseTicks = 0;
                advertise();
            }
            //  stop advertising
            if (now - advertiseStart >= advertisePeriod) serviceMode ^= Advertise;
        }
        if (serviceMode & Discovery == Discovery)
        {
            //  check if we need to stop discovery
            if ((discoveryPeriod > 0) && (now - discoveryStart >= discoveryPeriod))
            {
                serviceMode ^= Discovery;
            }
        }
        //  give back to the processor
        vTaskDelay(10);
    }
}

#pragma endregion Worker Loop

#pragma region Callbacks

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status == ESP_OK) return;
    Serial.print("*** Data sending failed with the following error: "); Serial.println(status);
}

void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    //  do we don't receive our own data
    if (macEquals(macAddress, mac))
    {
        Serial.println("*** We received our own data :(");
        return;
    }

    if (serviceMode & Discovery == Discovery)
    {
        if (instance->onPeerFound == nullptr) return;
        //  receive the data
        DiscoveryInfo info;
        memcpy(&info, incomingData, sizeof(info));
        //  notify any listeners
        instance->onPeerFound(info);
    }
    else 
    {

    }
}

#pragma endregion Callbacks
