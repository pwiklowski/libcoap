#include "COAPServer.h"
#include <cstdio>
#include "String.h"
#include "log.h"
#include "COAPObserver.h"
#include <cstddef>

COAPServer::COAPServer(COAPSend sender):
    m_sender(sender)
{
}



void COAPServer::handleMessage(COAPPacket* p){

    if(p->getType() == COAP_TYPE_ACK)
    {
        uint16_t messageId = p->getMessageId();

        COAPResponseHandler handler = m_responseHandlers.get(messageId);
        if (handler != nullptr ){
            handler(p);

            COAPOption* observeOption = p->getOption(COAP_OPTION_OBSERVE);

            if (observeOption !=0){
                COAPObserver* obs = new COAPObserver(p->getAddress(), p->getUri(), *p->getToken(), handler);
                m_observers.append(obs);
            }
            log("Remove response handler %d, remanin handlers=%d\n", messageId, m_responseHandlers.size());
            if (messageId != 0) //0 is reserverd mid or discovery
                m_responseHandlers.remove(messageId);
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
                        m_sender(response, nullptr);
                        return;
                    }
                }
                response->setResonseCode(COAP_RSPCODE_NOT_FOUND);
                m_sender(response, nullptr);
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

        m_sender(response, nullptr);
    }
}


void COAPServer::addResource(String url, COAPCallback callback){
    m_callbacks.insert(url, callback);
}

void COAPServer::addResponseHandler(uint16_t messageId, COAPResponseHandler handler){
    m_responseHandlers.insert(messageId, handler);
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
            m_sender(p, nullptr);
        }
    }
}



