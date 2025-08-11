#include <ArduinoJson.h>
#include <Helpers.h>
#include "esp_now.h"

#include "NowServer.h"
#include "NowDebug.h"

NowServer::NowServer()
{
    role = ServiceRole::Server;
}

NowServer::~NowServer()
{
}

void NowServer::work(unsigned long now, unsigned long ticks)
{
    //  make sure we can let idle clients go
    if (boundMac && !Helpers::macIsEmpty(boundMac, sizeof(boundMac)))
    {
        unsigned long elapsed = now - clientLast;
        if (elapsed < clientTimeout) return;
        //  get our selected client data
        ClientData client;
        getClient(boundMac, client);
        //  client hasn't sent a heartbeat - unbind
        Helpers::unsetFlag(Bound, serviceMode);
        client.state = CLIENT_DATA_NEW;
        memset(boundMac, 0x0, 6);
    }
}

void NowServer::dataReceived(const uint8_t *mac, const uint8_t *incomingData, int len)
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
    if (!validateMsg(incomingData, len))
        return;
    const NowMsg *m = reinterpret_cast<const NowMsg *>(incomingData);

    //  TODO: do this better that with a long switch - declaritively - how in c++?
    uint16_t replyType = 0;
    if (m->datatype == NOW_DT_ADVERTISE)
    {
        printDebug("    (dataReceived-0) Client advertisement received.", 1);
        if (boundMac && !Helpers::macIsEmpty(boundMac, sizeof(boundMac)))
        {
            printDebug("    (dataReceived-0) Already bound to a client. Ignore.", 1);
            return;
        }
        // name came in payload (not NUL-terminated). Copy safely:
        char nameBuf[231];
        uint16_t n = m->length;
        if (n > 230)
            n = 230;
        memcpy(nameBuf, m->payload, n);
        nameBuf[n] = '\0';
        addClient(String(nameBuf), Helpers::macToString(m->fromMac), CLIENT_DATA_NEW);
        //  send connect data
        replyType = NOW_DT_CONNECT;
    }
    else if (m->datatype == NOW_DT_HANDSHAKE)
    {
        printDebug("    (dataReceived-2) Client handshake received.", 1);
        if (boundMac && !Helpers::macIsEmpty(boundMac, sizeof(boundMac)) && !Helpers::macEquals(m->fromMac, boundMac))
        {
            printDebug("    (dataReceived-2) Already bound to a client (" + Helpers::macToString(boundMac) + "). Ignore (" + Helpers::macToString(m->fromMac) + ")", 1);
            return;
        }
        clientLast = millis();
        //  update the client data
        addClient("", Helpers::macToString(m->fromMac), CLIENT_DATA_CONFIRM);
        replyType = NOW_DT_ACK;
        //  we're bound now
        Helpers::setFlag(Bound, serviceMode);
        if (onPeerBound) onPeerBound(Helpers::macToString(boundMac));
    }
    else if (m->datatype == NOW_DT_HEARTBEAT)
    {
        //  make sure the heartbeat is from our bound client
        if (!boundMac || Helpers::macIsEmpty(boundMac, sizeof(boundMac)))
        {
            printDebug("    (dataReceived-4) Not bound to a client. Ignore.", 1);
            return;
        }
        if (!Helpers::macEquals(boundMac, m->fromMac))
        {
            printDebug("    (dataReceived-4) Heartbeat request received from unbound client. Ignore, client will reset to advertise.", 1);
            return;
        }
        clientLast = millis();
        printDebug("    (dataReceived-4) Client heartbeat request.", 1);
        sendHeartbeat(m->fromMac);
        return;
    }
    else if (m->datatype == NOW_DT_DATA)
    {
        //  make sure the heartbeat is from our bound client
        if (!boundMac || Helpers::macIsEmpty(boundMac, sizeof(boundMac)))
        {
            printDebug("    (dataReceived-5) Not bound to a client. Ignore.", 1);
            return;
        }
        if (!Helpers::macEquals(boundMac, m->fromMac))
        {
            printDebug("    (dataReceived-5) Incoming data from unbound client. Ignore.", 1);
            return;
        }
        clientLast = millis();
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
    //  TODO: refactor this
    //  send response
    NowMsg out{};
    if (buildMsg(out, replyType, macAddress, m->fromMac, nullptr, 0, millis()))
    {
        sendMsg(mac, out);
    }
}

void NowServer::initialize()
{
    memset(boundMac, 0x0, 6);
    clientLast = 0;
    printDebug("(initialize) Server Ready!", 0);
}

void NowServer::addClient(String name, String address, int state)
{
    ("(addClient) Preparing to add client: " + name + ", " + address, 0);
    //  add client as source - duplicates won't be added
    uint8_t mac[6];
    Helpers::parseMac(address, mac);
    addSourceMac((uint8_t *)mac);
    //  don't add duplicates
    for (ClientData &client : clients)
    {
        if (client.macAddress == address)
        {
            if (client.state == state)
            {
                printDebug("    (addClient) Duplicate client received: " + address, 1);
                return;
            }
            else
            {
                printDebug("    (addClient) Updating client state: " + String(state) + " (" + String(client.state) + ")", 1);
                client.state = state;
                return;
            }
        }
    }
    //  add to the list
    if (name.isEmpty())
    {
        printDebug("    (addClient) Unable to add client without name.", 1);
        return;
    }
    ClientData client(name, address, state);
    clients.push_back(client);
    Helpers::parseMac(mac, boundMac);
}

void NowServer::getClient(const uint8_t *mac, ClientData &outClient)
{
    if (clients.size() == 0) return;
    for (ClientData &client : clients)
    {
        uint8_t cmac[6];
        Helpers::parseMac(client.macAddress, cmac);
        if (Helpers::macEquals(cmac, mac)) 
        {
            outClient = client;
            return;
        }
    }
}
