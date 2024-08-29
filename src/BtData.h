#ifndef BT_DATA_H
#define BT_DATA_H

#define BT_DATA_MAC_ADDRESS "macAddress"
#define BT_DATA_PERIPHERAL "peripheral"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "BtBase.h"

const String BT_DATA = "BtData";

struct BtData : public BtBase
{
    String macAddress;
    u_int peripheral;

    BtData();

    BtData(String macAddress, u_int peripheral);

    BtData(const JsonDocument& doc);

    void serialize(JsonDocument& doc) override;

    String getType() override;

    BtData& operator=(const BtData& other) 
    {
        if (this != &other)
        {
            macAddress = other.macAddress;
            peripheral = other.peripheral;
        }
        return *this;
    }
};

#endif // BT_DATA_H