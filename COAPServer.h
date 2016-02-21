#ifndef COAPServer_H
#define COAPServer_H

#include "String.h"
#include "COAPPacket.h"
#include "List.h"
#include "Map.h"
#include "FastFunc.hpp"
#include <iostream>




class COAPServer;
class COAPObserver;

typedef ssvu::FastFunc< void(COAPPacket*)> COAPResponseHandler;
typedef ssvu::FastFunc< bool(COAPServer*, COAPPacket*, COAPPacket*)> COAPCallback;
typedef ssvu::FastFunc< void(COAPPacket* packet, COAPResponseHandler func)> COAPSend;


class COAPServer
{
public:
    COAPServer(COAPSend sender);

    void handleMessage(COAPPacket* p);
    void addResource(String url, COAPCallback callback);
    void addResponseHandler(uint16_t messageId, COAPResponseHandler handler);

    void setIp(String ip){ m_ip = ip; }
    String getIp(){ return m_ip; }


    void setInterface(String interface){ m_interface = interface; }
    String getInterface(){ return m_interface; }

    void notify(String href, List<uint8_t> data);

    void setDiscoveryResponseHandler(uint16_t h){m_discoveryResponseHandlerId = h;}
private:
    COAPSend m_sender;
    Map<String, COAPCallback> m_callbacks;
    Map<uint16_t, COAPResponseHandler> m_responseHandlers;
    uint16_t m_discoveryResponseHandlerId;

    String m_ip;
    String m_interface;
    uint16_t m_port;

    List<COAPObserver*> m_observers;
};

#endif // COAPPacket_H
