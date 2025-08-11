#pragma once

#include <Arduino.h>

class Helpers
{
public:
    static bool flagIsSet(int flag, int mask);
    static void setFlag(int flag, int &mask);
    static void unsetFlag(int flag, int &mask);
    static String macToString(const uint8_t *mac);
    static bool macEquals(const uint8_t *mac1, const uint8_t *mac2);
    static void parseMac(const String &s, uint8_t *mac);
    static void parseMac(const uint8_t *inMac, uint8_t *outMac);
    static bool macIsEmpty(const uint8_t *mac, int len);

private:
    Helpers() = delete;
};
