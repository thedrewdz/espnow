#include "Helpers.h"

bool Helpers::flagIsSet(int flag, int mask)
{
    return ((mask & flag) == flag);
}

void Helpers::setFlag(int flag, int &mask)
{
    mask |= flag;
}

void Helpers::unsetFlag(int flag, int &mask)
{
    mask ^= flag;
}

String Helpers::macToString(const uint8_t *mac)
{
    char buffer[18];

    snprintf(buffer,
             sizeof(buffer),
             "%02x:%02x:%02x:%02x:%02x:%02x\n",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return String(buffer);
}

bool Helpers::macEquals(const uint8_t *mac1, const uint8_t *mac2)
{
    return memcmp(mac1, mac2, 6) == 0;
}

void Helpers::parseMac(const String &s, uint8_t *mac)
{
    if (s.isEmpty())
    {
        return;
    }
    //  assuming mac is in the format "xx:xx:xx:xx:xx:xx"
    for (int i = 0; i < 6; i++)
    {
        String ss = s.substring(i * 3, i * 3 + 2);
        mac[i] = strtoul(ss.c_str(), NULL, 16);
    }
}

void Helpers::parseMac(const uint8_t *inMac, uint8_t *outMac)
{
    for (int i = 0; i < 6; i++)
    {
        outMac[i] = inMac[i];
    }
}

bool Helpers::macIsEmpty(const uint8_t *mac, int len)
{
    if (!mac || (len < 6)) return true;
    for (int i = 0; i < 6; i++)
    {
        if (mac[i] != 0) return false;
    }
    return true;
}
