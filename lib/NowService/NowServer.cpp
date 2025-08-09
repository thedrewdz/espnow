#include <ArduinoJson.h>
#include <Helpers.h>
#include "esp_now.h"

#include "NowServer.h"

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
    if (selectedClient)
    {
        unsigned long elapsed = now - clientLast;
        if (elapsed < clientTimeout)
            return;
        //  client hasn't sent a heartbeat - unbind
        Helpers::unsetFlag(Bound, serviceMode);
        selectedClient->state = CLIENT_DATA_NEW;
        selectedClient = nullptr;
    }
}

void NowServer::dataReceived(const uint8_t *mac, const uint8_t *incomingData, int len)
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
    if (!validateMsg(incomingData, len))
        return;
    const NowMsg *m = reinterpret_cast<const NowMsg *>(incomingData);

    //  TODO: do this better that with a long switch - declaritively - how in c++?
    uint16_t replyType = 0;
    if (m->datatype == NOW_DT_ADVERTISE)
    {
        Serial.println("    (dataReceived-0) Client advertisement received.");
        if (selectedClient)
        {
            Serial.println("    (dataReceived-0) Already bound to a client. Ignore.");
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
        Serial.println("    (dataReceived-2) Client handshake received.");
        if (selectedClient)
        {
            Serial.println("    (dataReceived-2) Already bound to a client. Ignore.");
            return;
        }
        clientLast = millis();
        //  update the client data
        addClient("", Helpers::macToString(m->fromMac), CLIENT_DATA_CONFIRM);
        replyType = NOW_DT_ACK;
        //  we're bound now
        Helpers::setFlag(Bound, serviceMode);
    }
    else if (m->datatype == NOW_DT_HEARTBEAT)
    {
        //  make sure the heartbeat is from our bound client
        if (!selectedClient)
        {
            Serial.println("    (dataReceived-4) Not bound to a client. Ignore.");
            return;
        }
        if (!selectedClient->macAddress.equals(Helpers::macToString(m->fromMac)))
        {
            Serial.println("    (dataReceived-4) Heartbeat request received from unbound client. Ignore, client will reset to advertise.");
            return;
        }
        clientLast = millis();
        Serial.println("    (dataReceived-4) Client heartbeat request.");
        sendHeartbeat(m->fromMac);
        return;
    }
    else if (m->datatype == NOW_DT_DATA)
    {
        //  make sure the heartbeat is from our bound client
        if (!selectedClient)
        {
            Serial.println("    (dataReceived-5) Not bound to a client. Ignore.");
            return;
        }
        if (!selectedClient->macAddress.equals(Helpers::macToString(m->fromMac)))
        {
            Serial.println("    (dataReceived-5) Incoming data from unbound client. Ignore.");
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
    selectedClient = nullptr;
    clientLast = 0;
    Serial.println("    (initialize) Server Ready!");
}

void NowServer::addClient(String name, String address, int state)
{
    Serial.println("    (addClient) Preparing to add client: " + name + ", " + address);
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
                Serial.println("    (addClient) Duplicate client received: " + address);
                return;
            }
            else
            {
                Serial.println("    (addClient) Updating client state: " + String(state) + " (" + String(client.state) + ")");
                client.state = state;
                return;
            }
        }
    }
    //  add to the list
    if (name.isEmpty())
    {
        Serial.println("    (addClient) Unable to add client without name.");
        return;
    }
    ClientData client(name, address, state);
    selectedClient = &client;
    clients.push_back(client);
}