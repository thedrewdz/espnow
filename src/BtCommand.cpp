#include "BtCommand.h"
#include "ArduinoJson.h"

BtCommand::BtCommand(u_int commandType, String payload) :
    commandType(commandType), payload(payload)
{
}

BtCommand::BtCommand(const JsonDocument& doc) : BtBase(doc)
{
    commandType = doc[BT_COMMAND_COMMAND_TYPE];
    String s = doc[BT_COMMAND_DATA];
    payload = s;
}

void BtCommand::serialize(JsonDocument& doc) 
{
    doc[BT_COMMAND_COMMAND_TYPE] = commandType;
    doc[BT_COMMAND_DATA] = payload;
}

String BtCommand::getType()
{
    return BT_COMMAND;
}
