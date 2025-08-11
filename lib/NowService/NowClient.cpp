#include "esp_now.h"

#include <Helpers.h>
#include "NowClient.h"
#include "NowMsg.h"
#include "NowDebug.h"

NowClient::NowClient(String name)
    : name(name)
{
    role = ServiceRole::Client;
}

NowClient::~NowClient()
{
}

void NowClient::beginAdverise()
{
    Helpers::setFlag(Advertise, serviceMode);
    advertiseLast = 0;     //  advertise immediately
}

void NowClient::endAdvertise()
{
    Helpers::unsetFlag(Advertise, serviceMode);
}

void NowClient::advertise(unsigned long now, unsigned long ticks)
{
    if (!Helpers::flagIsSet(Advertise, serviceMode)) return;
    
    receiveLast = millis();
    unsigned long elapsed = now - advertiseLast;
    if (elapsed > advertiseInterval)
    {
        advertiseLast = now;
        printDebug("(advertise) Preparing to advertise...", 0);
        // payload = client name as bytes (no NUL needed)
        NowMsg msg{};
        const uint8_t* p = reinterpret_cast<const uint8_t*>(name.c_str());
        uint16_t n = (uint16_t)name.length();  // cap to 230 if you want
        if (!buildMsg(msg, NOW_DT_ADVERTISE, macAddress, broadcastMac, p, n, millis())) return;
        sendMsg(broadcastMac, msg);    }
}

void NowClient::work(unsigned long now, unsigned long ticks) 
{
    //  must we advertise
    advertise(now, ticks);
    //  check idle timeout for re-advertise
    checkTimeout(now);
}

void NowClient::dataReceived(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    printDebug("(dataReceived) Received data from: " + Helpers::macToString(mac) + ", length: " + String(len), 0);

    String myMac = Helpers::macToString(macAddress);
    //  check that we didn't receive our own data
    if (Helpers::macEquals(macAddress, mac))
    {
        printDebug("*** (dataReceived) We received our own data - " + myMac + ", " + Helpers::macToString(mac), 0);
        return;
    }

    //  deserialize incoming data
    if (!validateMsg(incomingData, len)) return;
    receiveLast = millis();
    const NowMsg* m = reinterpret_cast<const NowMsg*>(incomingData);

    //  only our bound server can send us anything except a connect message
    if ((m->datatype > NOW_DT_CONNECT) && !Helpers::macEquals(m->fromMac, boundMac))
    {
        printDebug("    (dataReceived) *** Received data from a different source: " + Helpers::macToString(m->fromMac) + ". Ignoring (" + Helpers::macToString(boundMac) + ")", 1);
        return;
    }

    if (m->datatype == NOW_DT_CONNECT)
    {
        printDebug("    (dataReceived-1) Accepting CONNECT from server", 1);
        // ensure message aimed at us
        if (!Helpers::macEquals(macAddress, m->toMac)) return;
        addSourceMac(m->fromMac);
        Helpers::parseMac(m->fromMac, boundMac);
        // send HANDSHAKE back
        printDebug("    (dataReceived-1) Initiate Handshake", 1);
        NowMsg out{};
        if (buildMsg(out, NOW_DT_HANDSHAKE, macAddress, m->fromMac, nullptr, 0, millis()))
          sendMsg(mac, out);
        //  stop advertising
        printDebug("    (dataReceived-1) Stop advertising", 1);
        endAdvertise();
    }
    else if (m->datatype == NOW_DT_ACK)
    {
        receiveLast = millis();
        printDebug("    (dataReceieved-3) Handshake complete. Stop receiving on omni channel", 1);
        //  unsubscribe from omni channel
        removeSourceMac(broadcastMac);
        //  we're now up and running
        Helpers::setFlag(Running, serviceMode);
        Helpers::setFlag(Bound, serviceMode);
        serverMac = Helpers::macToString(m->fromMac);
        Helpers::parseMac(m->fromMac, boundMac);
        printDebug("    (dataReceived-3) Now connected to server: " + serverMac + " (" + Helpers::macToString(boundMac) + ")", 1);
        if (onPeerBound) onPeerBound(Helpers::macToString(boundMac));
    }
    else if (m->datatype == NOW_DT_HEARTBEAT)
    {
        receiveLast = millis();
        printDebug("    (dataReceived-4) Heartbeat received from server. Timeout reset.", 1);
        countHb = 0;
    }
    else if (m->datatype == NOW_DT_DATA)
    {
        receiveLast = millis();
        //  make received data available to the consumer
        uint16_t n = m->length;
        if (!onDataReceived || (n == 0) || (n > sizeof(m->payload))) return;
        uint8_t *copy = static_cast<uint8_t *>(malloc(n));
        if (!copy) return;
        memcpy(copy, m->payload, n);
        onDataReceived(copy, static_cast<int>(n));
        free(copy);
        return;
    }
}

void NowClient::initialize()
{
    //  begin advertising
    Helpers::setFlag(Advertise, serviceMode);
    printDebug("(initialize) Starting client, advertise interval: " + String(advertiseInterval), 0);
    beginAdverise();
    printDebug("(initialize) Client Ready!", 0);
}

void NowClient::checkTimeout(unsigned long now)
{
    if (!Helpers::flagIsSet(Running, serviceMode)) return;

    unsigned long elapsed = now - receiveCheckLast;
    if (elapsed <= receiveCheckInterval) return;
    receiveCheckLast = now;

    elapsed = now - receiveLast;
    //  should we request a heartbeat?
    if (elapsed <= receiveTimeout) return;
    printDebug("(checkTimeout) Requesting heartbeat after " + String(elapsed) + "ms.", 0);
    uint8_t sMac[6];
    Helpers::parseMac(serverMac, sMac);
    sendHeartbeat(sMac);
    countHb++;

    if (countHb < 3) return;
    printDebug("    (checkTimeout) We haven't received anything for " + String(elapsed) + "ms, returning advertising", 1);
    //  we're not running anymore
    serverMac = "";
    memset(boundMac, 0x0, 6);
    Helpers::unsetFlag(Running, serviceMode);
    Helpers::unsetFlag(Bound, serviceMode);
    //  read omni channel
    addSourceMac(broadcastMac);
    //  start advertising
    beginAdverise();
}
