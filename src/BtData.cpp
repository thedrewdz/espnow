#include "BtData.h"
#include "ArduinoJson.h"

BtData::BtData(String mac, u_int peripheral) 
    : macAddress(mac), peripheral(peripheral)
{
}

BtData::BtData(const JsonDocument& doc) : BtBase(doc)
{
    macAddress = doc[BT_DATA_MAC_ADDRESS].as<String>();
    peripheral = doc[BT_DATA_PERIPHERAL];
}

void BtData::serialize(JsonDocument& doc) 
{
    doc[BT_DATA_MAC_ADDRESS] = macAddress;
    doc[BT_DATA_PERIPHERAL] = peripheral;
}

String BtData::getType()
{
    return BT_DATA;
}
