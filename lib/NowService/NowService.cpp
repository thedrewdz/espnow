#include <WiFi.h>
#include <iostream>
#include <cstring>
#include <Helpers.h>
#include "esp_wifi.h"
#include "esp_now.h"

#include "NowService.h"

NowService *instance;
int serviceModePrev = None;

unsigned long lastTick = 0;

#pragma region NowService interface

#pragma region Prototypes

void worker(void *pvParameters);
void onSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void onReceived(const uint8_t *mac, const uint8_t *incomingData, int len);

#pragma endregion Prototypes

NowService::NowService()
{
    instance = this;
}

NowService::~NowService()
{
    serviceMode = Terminate;
    delete (instance);
}

void NowService::initialize(PeerFoundCallback peerFound, DataReceivedCallback dataRecevied)
{
    Serial.println("(initialize) Initializing...");

    onPeerFound = peerFound;
    onDataReceived = dataRecevied;

    //  initialize wifi first
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("    (initialize) Error initializing ESP-NOW");
        return;
    }
    //  register callbacks
    esp_now_register_send_cb(onSent);
    esp_now_register_recv_cb(esp_now_recv_cb_t(onReceived));
    readMacAddress();

    //  add omni channel
    Serial.println("    (initialize) Register to receive data from omni channel");
    addSourceMac(broadcastMac);

    //  do specific initialization
    initialize();

    //  start the task
    Serial.println("    (initialize) Starting loop...");
    Helpers::setFlag(Initialized, serviceMode);
    worker();
}

bool NowService::sendData(const uint8_t *mac, uint8_t *data, int length)
{
    Serial.println("(sendData) Preparing to send data, To: " + Helpers::macToString(mac) + ", length: " + String(length));
    //  ensure that the data length is <= 230 bytes
    if (length > 230)
    {
        Serial.println("    (sendData) Unable to send more than 230 bytes for now.");
        return;
    }
    NowMsg out{};
    if (!buildMsg(out, NOW_DT_DATA, macAddress, mac, data, length, millis()))
    {
        Serial.println("    (sendData) Unable to build message.");
        return false;
    }
    if (!sendMsg(mac, out)) 
    {
        Serial.println("    (sendData) Unable to send message.");
        return false;
    }
    return true;
}

bool NowService::sendMsg(const uint8_t* mac, const NowMsg& m) 
{
    int length = sizeof(NowMsg);
    esp_err_t result = esp_now_send(mac, (uint8_t*)&m, length);
    Serial.println("    (sendData) sending data result: " + String(result) + ", length: " + String(length));
    return (result == ESP_OK) ? true : false;
}

void NowService::sendHeartbeat(const uint8_t *mac)
{
    Serial.println("(sendHeartbeat) Sending heartbeat");
    NowMsg m{};
    if (!buildMsg(m, NOW_DT_HEARTBEAT, macAddress, mac, nullptr, 0, millis())) return;
    sendMsg(mac, m);
}

#pragma endregion NowService interface

#pragma region Helpers

void NowService::readMacAddress()
{
    Serial.println("    (readMacAddress) Reading own MAC Address...");
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, macAddress);
    if (ret == ESP_OK)
    {
        Serial.println("    (readMacAddress) Success: " + Helpers::macToString(macAddress));
    }
    else
    {
        Serial.println("    (readMacAddress) Failed to read own MAC address");
    }
}

void NowService::addSourceMac(const uint8_t *sourceMac)
{
    if (esp_now_is_peer_exist(sourceMac)) return;

    Serial.println("    (addSourceMac) adding peer: " + Helpers::macToString(sourceMac));
    const uint8_t m[6] = {sourceMac[0], sourceMac[1], sourceMac[2], sourceMac[3], sourceMac[4], sourceMac[5]};
    esp_now_peer_info peer;
    memset(&peer, 0, sizeof(esp_now_peer_info_t));
    peer.channel = 0;
    peer.encrypt = false;
    memcpy(peer.peer_addr, m, 6);
    if (esp_now_add_peer(&peer) != ESP_OK)
    {
        Serial.println("    (addSourceMac) Failed to add peer");
    }
}

void NowService::removeSourceMac(const uint8_t *sourceMac)
{
    Serial.println("    (removeSourceMac) Removing source: " + Helpers::macToString(sourceMac));
    if (!esp_now_is_peer_exist(sourceMac)) return;

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, sourceMac, 6);
    peerInfo.channel = 0; // 0 = current channel
    peerInfo.encrypt = false;

    if (esp_now_del_peer(sourceMac) != ESP_OK)
    {
        Serial.println("    (removeSourceMac) Failed to remove source: " + Helpers::macToString(sourceMac));
    }
    else
    {
        Serial.println("    (removeSourceMac) Source successfully removed: " + Helpers::macToString(sourceMac));
    }
}

#pragma endregion Helpers

#pragma region Worker Loop

void NowService::worker()
{
    while (!Helpers::flagIsSet(Terminate, serviceMode))
    {
        if (serviceMode != serviceModePrev)
        {
            Serial.println("    (worker) service mode changed: " + String(serviceMode) + " (" + String(serviceModePrev) + ")");
            serviceModePrev = serviceMode;
        }

        unsigned long now = millis();
        unsigned long ticks = now - lastTick;
        lastTick = now;

        work(now, ticks);

        //  give back to the processor
        vTaskDelay(1000);
    }
    Serial.println("    (worker) The End!");
}

#pragma endregion Worker Loop

#pragma region Virtuals

void NowService::initialize()
{
    Serial.println("*** (virtual intialize) This shouldn't happen");
}

void NowService::work(unsigned long now, unsigned long ticks)
{
    Serial.println("*** (virtual work) This shouldn't happen");
}
void NowService::dataReceived(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    Serial.println("*** (virtual dataReceived) This shouldn't happen");
}

#pragma endregion Virtuals

#pragma region Callbacks

void onSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    Serial.println("    (onSent) data send to: " + Helpers::macToString(mac_addr) + ", status: " + String(status));
    if (status != ESP_OK)
    {
        Serial.print("*** Data sending failed with the following error: ");
        Serial.println(status);
    }
}

void onReceived(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    instance->dataReceived(mac, incomingData, len);
}

#pragma endregion Callbacks
