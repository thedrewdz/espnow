#include "BtBase.h"

BtBase::BtBase()
{
}

BtBase::BtBase(const JsonDocument& doc)
{
}

void BtBase::serialize(JsonDocument& doc)
{
}

String BtBase::getType()
{
    return "Base";
}
