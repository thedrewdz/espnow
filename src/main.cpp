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

bool server = false;
bool bound = false;

typedef struct
{
    uint16_t len;
    uint8_t data[230];
} rxMsg;

QueueHandle_t rxQ;

void setup()
{
    Serial.begin(115200);
    rxQ = xQueueCreate(10, sizeof(rxMsg));
    xTaskCreatePinnedToCore(serviceThread, "Worker Loop", 2048, NULL, 1, NULL, 0);
}

void loop()
{
    //  send a message
    if (bound)
    {
        if (!server)
        {
            String t = "This is a test";
            const uint8_t *msg = reinterpret_cast<const uint8_t *>(t.c_str());
            if (!service->sendData(msg, t.length()))
            {
                Serial.println("**** Failed to send data.");
            }
        }

        //  check for messages in the queue
        rxMsg msg;
        if (xQueueReceive(rxQ, &msg, 0) == pdTRUE)
        {
            char cc[msg.len];
            memcpy(cc, msg.data, msg.len);
            Serial.println("**** Data Received! (" + String(msg.len) + ") ****");
            Serial.println(String(cc));
            Serial.println();
        }
    }

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
    Serial.println("**** NOW BOUND: " + info);
    bound = true;
}

void onDataReceived(uint8_t *data, int length)
{
    if (length <= 0)
        return;
    rxMsg msg{};
    msg.len = (length > 230) ? 230 : length;
    memcpy(msg.data, data, msg.len);
    xQueueSend(rxQ, &msg, 0);
}