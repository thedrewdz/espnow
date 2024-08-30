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
void onSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void onReceived(const uint8_t *mac, const uint8_t *incomingData, int len);
String macToString(uint8_t *mac);

#pragma endregion Prototypes

NowService::NowService() 
{
    instance = this;
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
    esp_now_register_send_cb(onSent);
    esp_now_register_recv_cb(esp_now_recv_cb_t(onReceived));

    //  start the task
    xTaskCreatePinnedToCore(worker, "Worker Loop", 2048, NULL, 1, NULL, 0);

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
    if (!esp_now_is_peer_exist(mac)) 
    {
        const uint8_t m[6] = { mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] };
        esp_now_peer_info peer;
        memset(&peer, 0, sizeof(esp_now_peer_info_t));
        peer.channel = 0;
        peer.encrypt = false;
        memcpy(peer.peer_addr, m, 6);
        Serial.print("\t\tAdding peer:" ); Serial.println(macToString(mac));
        esp_now_add_peer(&peer);
    }

    esp_err_t result = esp_now_send(mac, data, length);
    Serial.print("Data sent: "); Serial.println(result);
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
    bool success = instance->sendData((uint8_t *)broadcastMac, (uint8_t *)&info, sizeof(info));
    Serial.print("Advertisement sent "); Serial.print(success); Serial.print(", Mode: "); Serial.println(serviceMode);
}


bool macEquals(const uint8_t *mac1, const uint8_t *mac2) 
{
    return memcmp(mac1, mac2, 6) == 0;
}

String NowService::macToString(uint8_t *mac)
{
    char buffer[18];

    snprintf(buffer, 
        sizeof(buffer), 
        "%02x:%02x:%02x:%02x:%02x:%02x\n", 
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return String(buffer);
}

#pragma endregion Helpers

#pragma region Worker Loop

//  this method should be executed on core 1 so as not to interfere with WIFI and ESP-NOW functions
void worker(void *pvParameters)
{
    Serial.println("Starting worker loop");
    while(serviceMode != Terminate)
    {
        unsigned long now = millis();
        unsigned long ticks = now - lastTick;
        lastTick = now;

        //  must we advertise
        Serial.print("*** Service Mode: "); Serial.println(serviceMode);
        if ((serviceMode & Advertise) == Advertise)
        {
            Serial.println("\tAdvertise: true");
            advertiseTicks += ticks;
            if (advertiseTicks > advertiseInterval)
            {
                Serial.print("\t\tInterval: "); Serial.println(advertiseTicks);
                advertiseTicks = 0;
                advertise();
            }
            //  stop advertising
            if ((serviceMode & Advertise == Advertise) && (now - advertiseStart >= advertisePeriod)) serviceMode ^= Advertise;
        }
        if ((serviceMode & Discovery) == Discovery)
        {
            Serial.println("\tDiscovery: true");
            //  check if we need to stop discovery
            if ((discoveryPeriod > 0) && (now - discoveryStart >= discoveryPeriod))
            {
                Serial.println("\t\tTime to stop discovery");
                serviceMode ^= Discovery;
            }
        }
        //  give back to the processor
        vTaskDelay(1000);
    }
    Serial.println("Worker loop teminated");
}

#pragma endregion Worker Loop

#pragma region Callbacks

void onSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    Serial.print("Data send complete with status: "); Serial.println(status);
    if (status == ESP_OK) return;
    Serial.print("*** Data sending failed with the following error: "); Serial.println(status);
}

void onReceived(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    Serial.print("Data received: "); Serial.println(len);
    //  do we don't receive our own data
    if (macEquals(macAddress, mac))
    {
        Serial.println("*** We received our own data :(");
        return;
    }

    if (serviceMode & Discovery == Discovery)
    {
        if ((instance->onPeerFound == nullptr) || (len != sizeof(DiscoveryInfo))) 
        {
            Serial.println("Data does not appear to be DiscoveryInfo data!");
            return;
        }
        //  receive the data
        DiscoveryInfo info;
        memcpy(&info, incomingData, sizeof(info));
        //  notify any listeners
        instance->onPeerFound(info);
    }
    else 
    {
        if (instance->onDataReceived == nullptr) return;
        instance->onDataReceived((uint8_t*)incomingData);
    }
}

#pragma endregion Callbacks

