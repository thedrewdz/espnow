#include <Arduino.h>
#include <NowService.h>
#include <WiFi.h>
#include <esp_wifi.h>

NowService service;
String _macAddress;
uint8_t _macPointer[6];

void readMacAddress();
String macToString(uint8_t *mac);
void onPeerFound(DiscoveryInfo info);

void setup() 
{
    Serial.begin(115200);
    //  initialize WIFI so we can get our own MAC
    WiFi.mode(WIFI_STA);
    readMacAddress();
    Serial.print("Own MAC is: "); Serial.println(_macAddress);
    //  initialize the ESP-NOW service and start advertising
    service.initialize(onPeerFound, false);
    service.beginAdverise(_macPointer, 1000);
    service.beginDiscovery();
}

void loop()
{
    delay(10);
}

void readMacAddress()
{
    Serial.println("Reading MAC Address...");
    // uint8_t baseMac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, _macPointer);
    if (ret == ESP_OK)
    {
        _macAddress = macToString(_macPointer);
    }
    else
    {
        Serial.println("Failed to read MAC address");
    }
}

String macToString(uint8_t *mac)
{
    char buffer[18];

    snprintf(buffer, 
        sizeof(buffer), 
        "%02x:%02x:%02x:%02x:%02x:%02x\n", 
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return String(buffer);
}

void onPeerFound(DiscoveryInfo info)
{
    Serial.print("Found Peer: "); Serial.println(macToString(info.macAddress));
}