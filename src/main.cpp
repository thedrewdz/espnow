#include <Arduino.h>
#include <NowService.h>
#include <NowServer.h>
#include <NowClient.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>

#include <Helpers.cpp>

NowService *service;
String _macAddress;
uint8_t _macPointer[6];
std::vector<uint8_t *> peers;

void onPeerFound(String info);
void onDataReceived(uint8_t *data, int length);
void serviceThread(void *pvParameters);

bool server = true;

void setup()
{
    Serial.begin(115200);
    //  initialize WIFI so we can get our own MAC
    //  initialize the ESP-NOW service and start advertising
    xTaskCreatePinnedToCore(serviceThread, "Worker Loop", 2048, NULL, 1, NULL, 0);
}

void loop()
{
    delay(1000);
}

void serviceThread(void *pvParameters)
{
    if (server) 
    {
        service = new NowServer();
    }
    else 
    {
        service = new NowClient("CLIENT");
    }
    service->initialize(onPeerFound, onDataReceived);
}

void onPeerFound(String info)
{
}

void onDataReceived(uint8_t *data, int length)
{
}