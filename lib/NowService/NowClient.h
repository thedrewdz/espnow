#pragma once

#include "NowClient.h"
#include "NowService.h"

class NowClient : public NowService
{
private:
    unsigned long advertiseInterval = 1000;
    unsigned long advertiseLast = 0;
    unsigned long receiveTimeout = 60000;
    unsigned long receiveLast = 0;
    unsigned long receiveCheckInterval = 5000;  //  check every second / 5 seconds
    unsigned long receiveCheckLast = 0;
    int countHb = 0;
    
    String serverMac;

    void beginAdverise();
    void advertise(unsigned long now, unsigned long ticks);
    void endAdvertise();
    void checkTimeout(unsigned long now);

protected:
    void work(unsigned long now, unsigned long ticks) override;
    void initialize() override;

public:
    String name = "";

    NowClient(String name);
    ~NowClient();

    void dataReceived(const uint8_t *mac, const uint8_t *incomingData, int len) override;
};
