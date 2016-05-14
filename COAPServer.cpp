#include "COAPServer.h"
#include <cstdio>
#include "String.h"
#include "log.h"
#include "COAPObserver.h"
#include <cstddef>

COAPServer::COAPServer(COAPSend sender):
    m_sender(sender),
    m_tick(0)
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

        if (m_responseHandlers.has(messageId) ){
            COAPResponseHandler handler = m_responseHandlers.get(messageId);
            handler(p);


            if (messageId != 0){//0 is reserverd mid or discovery
                log("Remove response handler %d, remanin handlers=%d\n", messageId, m_responseHandlers.size());

                m_responseHandlers.remove(messageId);
                m_timestamps.remove(messageId);
                delete m_packets.get(messageId);
                m_packets.remove(messageId);
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
            log("handle request %s\n", uri.c_str());

            COAPOption* observeOption = p->getOption(COAP_OPTION_OBSERVE);
            if (observeOption)
            {
                uint8_t observe = 0;
                if (observeOption->getData()->size() > 0)
                    observe = observeOption->getData()->at(0);

                if (observe == 0){
                    COAPObserver* obs = new COAPObserver(p->getAddress(), p->getUri(), *p->getToken());
                    m_observers.append(obs);
                    List<uint8_t> d;
                    d.append(obs->getNumber());
                    response->addOption(new COAPOption(COAP_OPTION_OBSERVE, d));
                }
                else if (observe == 1){
                    for(uint16_t i=0; i<m_observers.size(); i++){
                        COAPObserver* o = m_observers.at(i);
                        if (o->getToken() == *p->getToken()){
                           delete o;
                           m_observers.remove(i);
                           log("remove observer");
                           break;
                        }
                    }
                }
                else{
                    for(COAPObserver* o: m_observers) {
                        if (o->getToken() == *p->getToken()){
                            if (o->getHref() == p->getUri()){
                                o->handle(p);
                                log("notify from server received %s %s\n", o->getAddress().c_str(), o->getHref().c_str());
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
    log("sendPacket \n");
    if (!p->isValidMessageId()){
        m_id++;
        if (m_id == 0) m_id = 1;
        p->setMessageId(m_id);
    }
    if (handler != nullptr){
        log("add handlers %d\n", p->getMessageId());
        m_responseHandlers.insert(p->getMessageId(), handler);
        m_packets.insert(p->getMessageId(), p);
        m_timestamps.insert(p->getMessageId(), m_tick);
    }
    COAPOption* observeOption = p->getOption(COAP_OPTION_OBSERVE);

    if (observeOption !=0){
        uint8_t observe = 0;
        if (observeOption->getData()->size() > 0)
            observe = observeOption->getData()->at(0);

        if (observe == 0){
            COAPObserver* obs = new COAPObserver(p->getAddress(), p->getUri(), *p->getToken(), handler);
            m_observers.append(obs);
        }
    }


    log("send");
    m_sender(p);
    log(" sent\n");
    if (handler == nullptr && !keepPacket){
        delete p;
        log("delete packet\n");
    }
}

void COAPServer::addResource(String url, COAPCallback callback){
    m_callbacks.insert(url, callback);
}


void COAPServer::tick(){
    m_tick++;

    for(uint16_t messageId: m_responseHandlers){

        if (messageId == 0) continue;
        uint32_t tick = m_timestamps.get(messageId);
        COAPPacket* p = m_packets.get(messageId);
        if (m_tick-tick > 1){
            log("Resend packet\n");
            sendPacket(p, nullptr, true);
        }
        if (m_tick - tick > 10){
            log("timeout remove handlers %d\n", p->getMessageId());
            // abort sending
            COAPResponseHandler handler = m_responseHandlers.get(p->getMessageId());
            if (handler != nullptr)
                handler(0);

            m_responseHandlers.remove(p->getMessageId());
            delete m_packets.get(p->getMessageId());
            m_packets.remove(p->getMessageId());
            m_timestamps.remove(p->getMessageId());

        }
    }
}


void COAPServer::notify(String href, List<uint8_t>* data){
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


            sendPacket(p, [=](COAPPacket* pa){
                if (pa){
                    log("Notify acked\n");
                }else{
                    log("Notify timeout, remove observer\n");
                    delete o;
                    m_observers.remove(i);
                }
            });
        }
    }
}



