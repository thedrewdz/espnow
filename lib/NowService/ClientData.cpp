#include "ClientData.h"

ClientData::ClientData()
{
}

ClientData::ClientData(String name, String macAddress, int state)
    : name(name), macAddress(macAddress), state(state)
{
}
