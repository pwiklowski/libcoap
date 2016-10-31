#include "COAPServer.h"
#include <cstdio>
#include "log.h"
#include "COAPObserver.h"
#include <cstddef>

#ifdef ESP8266
    uint64_t get_current_ms(){
        return xTaskGetTickCount();
    }

#else
    #include <sys/time.h>
    uint64_t get_current_ms(){
        struct timeval te;
        gettimeofday(&te, NULL); // get current time
        long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // caculate milliseconds
        return milliseconds;
    }

#endif



#define cs_log(line, ...) //log(line, ## __VA_ARGS__ )

COAPServer::COAPServer(COAPSend sender):
    m_sender(sender)
{
}

COAPServer::~COAPServer(){
    for (uint16_t i=0; i<m_observers.size();i++)
        delete m_observers.at(i);

}



void COAPServer::handleMessage(COAPPacket* p){

    if(p->getType() == COAP_TYPE_ACK)
    {
        uint16_t messageId = p->getMessageId();
        cs_log("ack mid=%d\n", messageId);
        if (m_packets.has(messageId) ){
            COAPPacket* packet = m_packets.get(messageId);
            packet->callHandler(p);

            if (messageId != 0){//0 is reserverd mid or discovery
                delete m_packets.get(messageId);
                m_packets.remove(messageId);
                cs_log("Remove response handler %d, remanin handlers=%d\n", messageId, m_responseHandlers.size());
            }
        }
    }
    else if (p->getType() == COAP_TYPE_CON)
    {
        COAPPacket* response = new COAPPacket();
        response->setAddress(p->getAddress());
        response->setMessageId(p->getMessageId());

        List<uint8_t>* t = p->getToken();

        if (t->size() == 2){
            uint16_t token = t->at(1) << 8 | t->at(0);
            response->setToken(token);
        }

        String uri = p->getUri();
        if (uri.size() >0){
            cs_log("handle request %s %s\n", uri.c_str(), p->getAddress().c_str());

            COAPOption* observeOption = p->getOption(COAP_OPTION_OBSERVE);
            if (observeOption)
            {
                uint8_t observe = 0;
                if (observeOption->getData()->size() > 0)
                    observe = observeOption->getData()->at(0);

                if (observe == 0){
                    cs_log("add observer \n");
                    COAPObserver* obs = new COAPObserver(p->getAddress(), p->getUri(), *p->getToken());
                    m_observers.append(obs);
                    List<uint8_t> d;
                    d.append(obs->getNumber());
                    response->addOption(new COAPOption(COAP_OPTION_OBSERVE, d));
                }
                else if (observe == 1){
                    cs_log("remove observer \n");
                    for(uint16_t i=0; i<m_observers.size(); i++){
                        COAPObserver* o = m_observers.at(i);
                        if (o->getToken() == *p->getToken()){
                           cs_log("remove observer %d %s %s\n", o->getNumber(), o->getAddress(),o->getHref());
                           delete o;
                           m_observers.remove(i);
                           break;
                        }
                    }
                }
                else{
                    for(COAPObserver* o: m_observers) {
                        if (o->getToken() == *p->getToken()){
                            if (o->getHref() == p->getUri()){
                                cs_log("notify from server received %dl %s %s\n", o->getNumber(), o->getAddress().c_str(), o->getHref().c_str());
                                o->handle(p);
                                sendPacket(response, nullptr);
                                return;
                            }
                        }
                    }
                    response->setResonseCode(COAP_RSPCODE_NOT_FOUND);
                    sendPacket(response, nullptr);
                    return;
                }
            }

            if (m_callbacks.has(uri)){
                COAPCallback callback = m_callbacks.get(uri);
                bool success = callback(this, p, response);
                if (!success){
                    response->setResonseCode(COAP_RSPCODE_FORBIDDEN);
                }
            }else{
                response->setResonseCode(COAP_RSPCODE_NOT_FOUND);
            }
        }
        sendPacket(response, nullptr);
    }
}

void COAPServer::sendPacket(COAPPacket* p, COAPResponseHandler handler, bool keepPacket){
    if (!p->isValidMessageId()){
        m_id++;
        if (m_id == 0) m_id = 1;
        p->setMessageId(m_id);
        cs_log("sendPacket assign new message id\n");
    }
    cs_log("sendPacket mid=%d\n", p->getMessageId());
    if (handler != nullptr){
        cs_log("add handler %d\n", p->getMessageId());

        p->setHandler(handler);
        p->setSendTimestamp(get_current_ms());
        p->setResentTimestamp(get_current_ms());

        m_packets.insert(p->getMessageId(), p);
    }
    COAPOption* observeOption = p->getOption(COAP_OPTION_OBSERVE);

    if (observeOption !=0){
        uint8_t observe = 0;
        if (observeOption->getData()->size() > 0)
            observe = observeOption->getData()->at(0);

        if (observe == 0){
            cs_log("add new observer\n");

            //TODO check for duplicated observers
            COAPObserver* obs = new COAPObserver(p->getAddress(), p->getUri(), *p->getToken(), handler);
            m_observers.append(obs);
        }
    }

    m_sender(p);
    if (handler == nullptr && !keepPacket){
        delete p;
    }else{
        cs_log("do not delete packet %d\n", p->getMessageId());
    }
}

void COAPServer::queuePacket(COAPPacket* packet, COAPResponseHandler handler){
    packet->setHandler(handler);
    m_packetQueue.append(packet);
}


void COAPServer::sendQueuedPackets(){
    for(size_t i=0; i<m_packetQueue.size(); i++){
        COAPPacket* p = m_packetQueue.at(i);
        sendPacket(p, p->getHandler());
    }
    m_packetQueue.clear();
}

void COAPServer::addResource(String url, COAPCallback callback){
    m_callbacks.insert(url, callback);
}

void COAPServer::checkPackets(){
    for(uint16_t messageId: m_packets){

        if (messageId == 0) continue;
        COAPPacket* packet = m_packets.get(messageId);

        uint64_t tick = packet->getTimestamp();
        uint64_t tick_diff = get_current_ms()- tick;
        uint64_t waiting = get_current_ms() - packet->getResentTimestamp();

        if (tick_diff > 1000){
            log("timeout remove handler timediff=%d id=%d\n", tick_diff,  messageId);

            // TODO: clean all handlers for that destination ?
            // abort sending

            cs_log("timeout remove handler - get handler %d\n", m_responseHandlers.has(messageId));

            packet->callHandler(0);

            delete m_packets.get(messageId);
            m_packets.remove(messageId);
        }else if (waiting > 100){
            log("Resend packet %d\n", messageId);
            packet->setResentTimestamp(get_current_ms());
            sendPacket(m_packets.get(messageId), nullptr, true);
        }
    }
}


void COAPServer::deleteObserver(String address, String href){
    cs_log("delete observer %s %s count=%d\n", address.c_str(), href.c_str(), m_observers.size());
    for(uint16_t i=0; i<m_observers.size(); i++){
        COAPObserver* o = m_observers.at(i);

        if (o->getAddress() == address && o->getHref() == href){
            delete o;
            m_observers.remove(i);
            cs_log("delete observer deleted\n");
            return;
        }
    }
    cs_log("delete observer not found\n");

}


void COAPServer::notify(String href, List<uint8_t>* data){
    cs_log("nofity %d\n", m_observers.size());
    for(uint16_t i=0; i<m_observers.size(); i++){
        COAPObserver* o = m_observers.at(i);

        if (href == o->getHref()){
            COAPPacket* p = new COAPPacket();
            p->setType(COAP_TYPE_CON);
            p->setAddress(o->getAddress());
            p->setResonseCode(COAP_RSPCODE_CONTENT);
            p->setToken(o->getToken());

            List<uint8_t> obs;
            obs.append(o->getNumber());
            p->addOption(new COAPOption(COAP_OPTION_OBSERVE, obs));

            List<uint8_t> part;
            for(uint16_t j=1; j<href.size();j++){
                if (href.at(j) == '/'){
                    p->addOption(new COAPOption(COAP_OPTION_URI_PATH, part));
                    part.clear();
                }else{
                    part.append(href.at(j));
                }
            }
            p->addOption(new COAPOption(COAP_OPTION_URI_PATH, part));

            List<uint8_t> content_type;
            content_type.append(((uint16_t)COAP_CONTENTTYPE_CBOR & 0xFF00) >> 8);
            content_type.append(((uint16_t)COAP_CONTENTTYPE_CBOR & 0xFF));

            p->addOption(new COAPOption(COAP_OPTION_CONTENT_FORMAT, content_type));
            p->addPayload(*data);

            cs_log("notify %s\n", o->getAddress().c_str());

            String a = o->getAddress();
            String h = o->getHref();
            sendPacket(p, [=](COAPPacket* pa){
                if (pa){
                    cs_log("Notify acked\n");
                }else{
                    cs_log("Notify timeout, remove observer %s\n", a);
                    deleteObserver(a, h);
                }
            });
        }
    }
}



