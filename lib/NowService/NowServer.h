#pragma once

#include <Arduino.h>
#include <vector>

#include "NowService.h"
#include "ClientData.h"

class NowServer : public NowService
{
private:
    std::vector<ClientData> clients;
    unsigned long clientTimeout = 300000;
    unsigned long clientLast = 0;

    void addClient(String name, String address, int state);
    void getClient(const uint8_t *mac, ClientData &outClient);

protected:
    void work(unsigned long now, unsigned long ticks) override;
    void initialize() override;

public:
    NowServer();
    ~NowServer();

    void dataReceived(const uint8_t *mac, const uint8_t *incomingData, int len) override;
};
