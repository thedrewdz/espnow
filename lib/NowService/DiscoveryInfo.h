#ifndef DISCOVERY_INFO_H
#define DISCOVERY_INFO_H

#include <Arduino.h>

struct DiscoveryInfo
{
    unsigned long time;
    uint8_t macAddress[6];
};

#endif // DISCOVERY_INFO_H