#ifndef COAPOBSERVER_H
#define COAPOBSERVER_H

#include "String.h"
#include "List.h"
#include <stdint.h>
#include "COAPServer.h"


class COAPObserver
{
public:
    COAPObserver(String address, String href, List<uint8_t> token);
    COAPObserver(String address, String href, List<uint8_t> token, COAPResponseHandler handler);

    String getHref() { return m_href; }
    String getAddress() { return m_address; }
    List<uint8_t> getToken() { return m_token; }

    uint8_t getNumber() {
        m_number++;

        if (m_number==0) // 0 and 1 are reserverd
            m_number = 2;

        return m_number;
    }

    void handle(COAPPacket* p){
        if (m_handler.storage !=0)
            m_handler(p);

    }

private:
    String m_href;
    String m_address;
    List<uint8_t> m_token;
    uint8_t m_number;
    COAPResponseHandler m_handler;
};

#endif // COAPOBSERVER_H
