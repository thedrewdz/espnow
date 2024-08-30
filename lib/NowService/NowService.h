#ifndef NOW_SERVICE_H
#define NOW_SERVICE_H

#include <Arduino.h>
#include <vector>

#include "DiscoveryInfo.h"

enum ServiceMode : int
{
    None = 0,
    Initialized = 1,
    Advertise = 2,
    Discovery = 4,
    Broadcast = 8,
    Server = 16,
    Client = 32,
    Terminate = 64
};

class NowService
{
public:
    using PeerFoundCallback = std::function<void(DiscoveryInfo)>;
    PeerFoundCallback onPeerFound;

    using DataReceivedCallback = std::function<void(uint8_t*)>;
    DataReceivedCallback onDataReceived;

    NowService();

    ~NowService();

    void initialize(PeerFoundCallback peerFound, DataReceivedCallback dataRecevied, bool isServer = false);
    void beginAdverise(uint8_t *mac, int interval = 1000, unsigned long period = 300000);
    void endAdvertise();
    void beginDiscovery(unsigned long period = 0);
    void endDiscovery();

    bool sendData(uint8_t *mac, uint8_t *data, int length);
    void broadcastData(uint8_t *data, int length);

    String macToString(uint8_t *mac);
};

#endif // NOW_SERVICE_H