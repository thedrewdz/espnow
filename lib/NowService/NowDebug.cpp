#include "NowDebug.h"

const int DEBUG_LEVEL = 0;

void printDebug(const String &info, int level)
{
    if (level > DEBUG_LEVEL) return;
    Serial.println(info);
}
