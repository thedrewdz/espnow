#ifndef NOW_SERVICE_H
#define NOW_SERVICE_H

#include <Arduino.h>

#include "NowMsg.h"

enum ServiceMode : int
{
    None = 0,
    Initialized = 1,
    Advertise = 2,
    Discovery = 4,
    Broadcast = 8,
    Terminate = 16,
    Running = 32,
    Bound = 64
};

enum ServiceRole
{
    Client,
    Server
};

class NowService
{
protected:
    using BoundCallback = std::function<void(String)>;
    BoundCallback onPeerBound;

    using DataReceivedCallback = std::function<void(uint8_t*, int length)>;
    DataReceivedCallback onDataReceived;

    const uint8_t broadcastMac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    ServiceRole role = ServiceRole::Client;

    uint8_t macAddress[6];
    uint8_t boundMac[6];
    int serviceMode = None;

    void readMacAddress();
    void worker();
    virtual void work(unsigned long now, unsigned long ticks);    
    virtual void initialize();
    bool sendMsg(const uint8_t* mac, const NowMsg& m);
    void sendHeartbeat(const uint8_t *mac);
    void addSourceMac(const uint8_t *sourceMac);
    void removeSourceMac(const uint8_t *sourceMac);

public:
    NowService();

    ~NowService();

    void initialize(BoundCallback peerBound, DataReceivedCallback dataRecevied);
    bool sendData(const uint8_t *data, int length);
    virtual void dataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);
};

#endif // NOW_SERVICE_H