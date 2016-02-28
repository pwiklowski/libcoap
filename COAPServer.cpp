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



void COAPServer::handleMessage(COAPPacket* p){

    if(p->getType() == COAP_TYPE_ACK)
    {
        uint16_t messageId = p->getMessageId();

        if (m_responseHandlers.has(messageId) ){
            COAPResponseHandler handler = m_responseHandlers.get(messageId);
            handler(p);

            COAPOption* observeOption = p->getOption(COAP_OPTION_OBSERVE);

            if (observeOption !=0){
                COAPObserver* obs = new COAPObserver(p->getAddress(), p->getUri(), *p->getToken(), handler);
                m_observers.append(obs);
            }
            if (messageId != 0){//0 is reserverd mid or discovery
                log("Remove response handler %d, remanin handlers=%d\n", messageId, m_responseHandlers.size());

                m_responseHandlers.remove(messageId);
                m_timestamps.remove(messageId);
                m_packets.remove(messageId);
            }
        }
    }
    else if (p->getType() == COAP_TYPE_CON)
    {
        String uri = p->getUri();
        log("handle request %s\n", uri.c_str());

        COAPPacket* response = new COAPPacket();
        COAPOption* observeOption = p->getOption(COAP_OPTION_OBSERVE);

        response->setAddress(p->getAddress());
        response->setMessageId(p->getMessageId());

        List<uint8_t>* t = p->getToken();

        if (t->size() == 2){
            uint16_t token = t->at(1) << 8 | t->at(0);
            response->setToken(token);
        }

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
                log("TODO remove observers");

            }
            else{
                for(COAPObserver* o: m_observers) {
                    if (o->getToken() == *p->getToken()){
                        o->handle(p);
                        log("notify from server received %s %s\n", o->getAddress().c_str(), o->getHref().c_str());
                        sendPacket(response, nullptr);
                        return;
                    }
                }
                response->setResonseCode(COAP_RSPCODE_NOT_FOUND);
                sendPacket(response, nullptr);
                return;
            }
        }

        COAPCallback callback = m_callbacks.get(uri);
        if (callback != nullptr){
            bool success = callback(this, p, response);
            if (!success){
                response->setResonseCode(COAP_RSPCODE_FORBIDDEN);
            }
        }else{
            response->setResonseCode(COAP_RSPCODE_NOT_FOUND);
        }

        sendPacket(response, nullptr);
    }
}

void COAPServer::sendPacket(COAPPacket* p, COAPResponseHandler handler){
    if (!p->isValidMessageId()){
        m_id++;
        if (m_id == 0) m_id = 1;
        p->setMessageId(m_id);
    }

    if (handler != nullptr){
        m_responseHandlers.insert(p->getMessageId(), handler);
        m_packets.insert(p->getMessageId(), p);
        m_timestamps.insert(p->getMessageId(), m_tick);
    }

    m_sender(p);
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
            sendPacket(p, nullptr);
            m_timestamps.insert(messageId, m_tick);
        }
    }
}


void COAPServer::notify(String href, List<uint8_t> data){
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

            List<uint8_t> content_type;
            content_type.append(((uint16_t)COAP_CONTENTTYPE_CBOR & 0xFF00) >> 8);
            content_type.append(((uint16_t)COAP_CONTENTTYPE_CBOR & 0xFF));

            p->addOption(new COAPOption(COAP_OPTION_CONTENT_FORMAT, content_type));
            p->addPayload(data);
            sendPacket(p, [=](COAPPacket* p){
                log("notify acked");
            });
        }
    }
}



