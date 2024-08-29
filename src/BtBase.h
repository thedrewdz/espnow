#ifndef BT_BASE_H
#define BT_BASE_H

#include <Arduino.h>
#include <ArduinoJson.h>

struct BtBase
{
protected:
    BtBase();

    BtBase(const JsonDocument& doc);

public:
    virtual void serialize(JsonDocument& doc);

    virtual String getType();
};

#endif // BT_BASE_H
