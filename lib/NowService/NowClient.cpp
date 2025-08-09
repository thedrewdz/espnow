#include "esp_now.h"

#include <Helpers.h>
#include "NowClient.h"
#include "NowMsg.h"

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
        Serial.println("    (advertise) Preparing to advertise...");
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
    Serial.println("    (dataReceived) Received data from: " + Helpers::macToString(mac) + ", length: " + String(len));

    String myMac = Helpers::macToString(macAddress);
    //  check that we didn't receive our own data
    if (Helpers::macEquals(macAddress, mac))
    {
        Serial.println("*** (dataReceived) We received our own data - " + myMac + ", " + Helpers::macToString(mac));
        return;
    }

    //  deserialize incoming data
    if (!validateMsg(incomingData, len)) return;
    receiveLast = millis();
    const NowMsg* m = reinterpret_cast<const NowMsg*>(incomingData);

    if (m->datatype == NOW_DT_CONNECT)
    {
        Serial.println("    (dataReceived-1) Accepting CONNECT from server");
        // ensure message aimed at us
        if (!Helpers::macEquals(macAddress, m->toMac)) return;
        addSourceMac(m->fromMac);
        // send HANDSHAKE back
        Serial.println("    (dataReceived-1) Initiate Handshake");
        NowMsg out{};
        if (buildMsg(out, NOW_DT_HANDSHAKE, macAddress, m->fromMac, nullptr, 0, millis()))
          sendMsg(mac, out);
        //  stop advertising
        Serial.println("    (dataReceived-1) Stop advertising");
        endAdvertise();
    }
    else if (m->datatype == NOW_DT_ACK)
    {
        receiveLast = millis();
        Serial.println("    (dataReceieved-3) Handshake complete. Stop receiving on omni channel");
        //  unsubscribe from omni channel
        removeSourceMac(broadcastMac);
        //  we're now up and running
        Helpers::setFlag(Running, serviceMode);
        serverMac = Helpers::macToString(m->fromMac);
        Serial.println("    (dataReceived-3) Now connected to server: " + serverMac);
    }
    else if (m->datatype == NOW_DT_HEARTBEAT)
    {
        Serial.println("    (dataReceived-4) Heartbeat received from server. Timeout reset.");
        countHb = 0;
    }
}

void NowClient::initialize()
{
    //  begin advertising
    Helpers::setFlag(Advertise, serviceMode);
    Serial.println("    (initialize) Starting client, advertise interval: " + String(advertiseInterval));
    beginAdverise();
    Serial.println("    (initialize) Client Ready!");
}

void NowClient::checkTimeout(unsigned long now)
{
    if (!Helpers::flagIsSet(Running, serviceMode)) return;

    unsigned long elapsed = now - receiveCheckLast;
    if (elapsed <= receiveCheckInterval) return;
    receiveCheckLast = now;

    // Serial.println("(checkTimeout) Checking receive timeout...");
    elapsed = now - receiveLast;
    //  should we request a heartbeat?
    if (elapsed <= receiveTimeout) return;
    Serial.println("(checkTimeout) Requesting heartbeat after " + String(elapsed) + "ms.");
    uint8_t sMac[6];
    Helpers::parseMac(serverMac, sMac);
    sendHeartbeat(sMac);
    countHb++;

    if (countHb < 3) return;
    Serial.println("    (checkTimeout) We haven't received anything for " + String(elapsed) + "ms, returning advertising");
    //  we're not running anymore
    serverMac = "";
    Helpers::unsetFlag(Running, serviceMode);
    //  read omni channel
    addSourceMac(broadcastMac);
    //  start advertising
    beginAdverise();
}
