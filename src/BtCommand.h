#ifndef BT_COMMAND_H
#define BT_COMMAND_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include "BtBase.h"

#define CMD_CONFIGURATION 4
#define CMD_DATA 5
#define CMD_MAC 6
#define BT_COMMAND_COMMAND_TYPE "command_type"
#define BT_COMMAND_DATA "data"

const String BT_COMMAND = "BtCommand";

struct BtCommand: public BtBase
{
    u_int commandType;
    String payload;

    BtCommand();

    BtCommand(u_int commandType, String payload);

    BtCommand(const JsonDocument& doc);

    void serialize(JsonDocument& doc) override;

    String getType() override;

    BtCommand& operator=(const BtCommand& other) 
    {
        if (this != &other)
        {
            commandType = other.commandType;
            payload = other.payload;
        }
        return *this;
    }
};

#endif // BT_COMMAND_H