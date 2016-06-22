#include "COAPServer.h"
#include <cstdio>
#include "String.h"
#include "log.h"
#include "COAPObserver.h"
#include <cstddef>

extern uint64_t get_current_ms();



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

        if (m_responseHandlers.has(messageId) ){
            COAPResponseHandler handler = m_responseHandlers.get(messageId);
            handler(p);


            if (messageId != 0){//0 is reserverd mid or discovery
                log("Remove response handler %d, remanin handlers=%d\n", messageId, m_responseHandlers.size());

                m_responseHandlers.remove(messageId);
                m_timestamps.remove(messageId);
                delete m_packets.get(messageId);
                m_packets.remove(messageId);
                m_resentTimestamps.remove(messageId);
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
        m_timestamps.insert(p->getMessageId(), get_current_ms());
        m_resentTimestamps.insert(p->getMessageId(), get_current_ms());
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

    if (handler == nullptr && !keepPacket){
        p->setKeep(false);
    }
    m_packetQueue.append(p);
}

void COAPServer::addResource(String url, COAPCallback callback){
    m_callbacks.insert(url, callback);
}




void COAPServer::tick(){

    for(uint8_t i=0; i<m_packetQueue.size(); i++){
        COAPPacket* p = m_packetQueue.at(i);
        m_sender(p);
        if (!p->keep()){
            //delete p;
        }
    }
    m_packetQueue.clear();

    for(uint16_t messageId: m_responseHandlers){

        if (messageId == 0) continue;
        uint64_t tick = m_timestamps.get(messageId);
        uint64_t tick_diff = get_current_ms()- tick;
        uint64_t waiting = get_current_ms() - m_resentTimestamps.get(messageId);

        if (tick_diff > 5000){
            log("timeout remove handler id=%d\n", messageId);

            // TODO: clean all handlers for that destination ?
            // abort sending

            if (m_responseHandlers.has(messageId)){
                COAPResponseHandler handler = m_responseHandlers.get(messageId);
                log("timeout remove handler - get handler %d\n", m_responseHandlers.has(messageId));

                if (handler != nullptr){
                    log("call handler %d\n", &handler);
                    handler(0);
                }
                m_responseHandlers.remove(messageId); //kasowanie elementow z for eachu ?
                delete m_packets.get(messageId);
                m_packets.remove(messageId);
                m_timestamps.remove(messageId);
                m_resentTimestamps.remove(messageId);
            }

            log("timeout remove handler - remove handler\n");
        }else if (waiting >500){
            log("Resend packet\n");
            m_resentTimestamps.insert(messageId, get_current_ms());
            sendPacket(m_packets.get(messageId), nullptr, true);
        }
    }
}


void COAPServer::deleteObserver(String address, String href){
    log("delete observer %s %s count=%d\n", address.c_str(), href.c_str(), m_observers.size());
    for(uint16_t i=0; i<m_observers.size(); i++){
        COAPObserver* o = m_observers.at(i);

        if (o->getAddress() == address && o->getHref() == href){
            delete o;
            m_observers.remove(i);
            log("delete observer deleted\n");
            return;
        }
    }
    log("delete observer not found\n");

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

            log("notify %s\n", o->getAddress().c_str());

            sendPacket(p, [=](COAPPacket* pa){
                String a = o->getAddress();
                String h = o->getHref();
                if (pa){
                    log("Notify acked\n");
                }else{
                    log("Notify timeout, remove observer %s\n", a.c_str());
                    deleteObserver(a, h);
                }
            });
        }
    }
}



