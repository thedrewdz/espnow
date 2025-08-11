#include <WiFi.h>
#include <iostream>
#include <cstring>
#include <Helpers.h>
#include "esp_wifi.h"
#include "esp_now.h"

#include "NowService.h"
#include "NowDebug.h"

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

void NowService::initialize(BoundCallback peerBound, DataReceivedCallback dataRecevied)
{
    printDebug("(initialize) Initializing...", 0);

    onPeerBound = peerBound;
    onDataReceived = dataRecevied;

    //  initialize wifi first
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
    {
        printDebug("    (initialize) Error initializing ESP-NOW", 1);
        return;
    }
    //  register callbacks
    esp_now_register_send_cb(onSent);
    esp_now_register_recv_cb(esp_now_recv_cb_t(onReceived));
    readMacAddress();

    //  add omni channel
    printDebug("    (initialize) Register to receive data from omni channel", 1);
    addSourceMac(broadcastMac);

    //  do specific initialization
    memset(boundMac, 0x0, 6);
    initialize();

    //  start the task
    printDebug("    (initialize) Starting loop...", 1);
    Helpers::setFlag(Initialized, serviceMode);
    worker();
}

bool NowService::sendData(const uint8_t *data, int length)
{
    printDebug("(sendData) Preparing to send data, To: " + Helpers::macToString(boundMac) + ", length: " + String(length), 0);
    //  ensure that the data length is <= 230 bytes
    if (length > 230)
    {
        printDebug("    (sendData) Unable to send more than 230 bytes for now.", 1);
        return false;
    }
    NowMsg out{};
    if (!buildMsg(out, NOW_DT_DATA, macAddress, boundMac, data, length, millis()))
    {
        printDebug("    (sendData) Unable to build message.", 1);
        return false;
    }
    if (!sendMsg(boundMac, out)) 
    {
        printDebug("    (sendData) Unable to send message.", 1);
        return false;
    }
    return true;
}

bool NowService::sendMsg(const uint8_t* mac, const NowMsg& m) 
{
    int length = sizeof(NowMsg);
    esp_err_t result = esp_now_send(mac, (uint8_t*)&m, length);
    printDebug("(sendData) sending data result: " + String(result) + ", length: " + String(length), 0);
    return (result == ESP_OK) ? true : false;
}

void NowService::sendHeartbeat(const uint8_t *mac)
{
    printDebug("(sendHeartbeat) Sending heartbeat", 0);
    NowMsg m{};
    if (!buildMsg(m, NOW_DT_HEARTBEAT, macAddress, mac, nullptr, 0, millis())) return;
    sendMsg(mac, m);
}

#pragma endregion NowService interface

#pragma region Helpers

void NowService::readMacAddress()
{
    printDebug("(readMacAddress) Reading own MAC Address...", 0);
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, macAddress);
    if (ret == ESP_OK)
    {
        printDebug("    (readMacAddress) Success: " + Helpers::macToString(macAddress), 1);
    }
    else
    {
        printDebug("    (readMacAddress) Failed to read own MAC address", 1);
    }
}

void NowService::addSourceMac(const uint8_t *sourceMac)
{
    if (esp_now_is_peer_exist(sourceMac)) return;

    printDebug("(addSourceMac) adding peer: " + Helpers::macToString(sourceMac), 0);
    const uint8_t m[6] = {sourceMac[0], sourceMac[1], sourceMac[2], sourceMac[3], sourceMac[4], sourceMac[5]};
    esp_now_peer_info peer;
    memset(&peer, 0, sizeof(esp_now_peer_info_t));
    peer.channel = 0;
    peer.encrypt = false;
    memcpy(peer.peer_addr, m, 6);
    if (esp_now_add_peer(&peer) != ESP_OK)
    {
        printDebug("    (addSourceMac) Failed to add peer", 1);
    }
}

void NowService::removeSourceMac(const uint8_t *sourceMac)
{
    printDebug("(removeSourceMac) Removing source: " + Helpers::macToString(sourceMac), 0);
    if (!esp_now_is_peer_exist(sourceMac)) return;

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, sourceMac, 6);
    peerInfo.channel = 0; // 0 = current channel
    peerInfo.encrypt = false;

    if (esp_now_del_peer(sourceMac) != ESP_OK)
    {
        printDebug("    (removeSourceMac) Failed to remove source: " + Helpers::macToString(sourceMac), 1);
    }
    else
    {
        printDebug("    (removeSourceMac) Source successfully removed: " + Helpers::macToString(sourceMac), 1);
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
            printDebug("(worker) service mode changed: " + String(serviceMode) + " (" + String(serviceModePrev) + ")", 0);
            serviceModePrev = serviceMode;
        }

        unsigned long now = millis();
        unsigned long ticks = now - lastTick;
        lastTick = now;

        work(now, ticks);

        //  give back to the processor
        vTaskDelay(1000);
    }
    printDebug("    (worker) The End!", 1);
}

#pragma endregion Worker Loop

#pragma region Virtuals

void NowService::initialize()
{
    printDebug("*** (virtual intialize) This shouldn't happen", 1);
}

void NowService::work(unsigned long now, unsigned long ticks)
{
    printDebug("*** (virtual work) This shouldn't happen", 1);
}
void NowService::dataReceived(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    printDebug("*** (virtual dataReceived) This shouldn't happen", 1);
}

#pragma endregion Virtuals

#pragma region Callbacks

void onSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    printDebug("(onSent) data send to: " + Helpers::macToString(mac_addr) + ", status: " + String(status), 0);
    if (status != ESP_OK)
    {
        printDebug("*** Data sending failed with the following error: " + String(status), 1);
    }
}

void onReceived(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    instance->dataReceived(mac, incomingData, len);
}

#pragma endregion Callbacks
