#pragma once

#include <Arduino.h>

#define CLIENT_DATA_NEW 0
#define CLIENT_DATA_CONFIRM 1

struct ClientData
{
    String macAddress;
    String name;
    int state = CLIENT_DATA_NEW;

    ClientData();
    ClientData(String name, String macAddress, int state);
};
