#ifndef COAPOBSERVER_H
#define COAPOBSERVER_H

#include "String.h"
#include "List.h"
#include <stdint.h>
#include <functional>
#include "COAPServer.h"


class COAPObserver
{
public:
    COAPObserver(String address, String href, List<uint8_t> token);
    COAPObserver(String address, String href, List<uint8_t> token, COAPResponseHandler handler);
    void notify();

    String getHref() { return m_href; }
    String getAddress() { return m_address; }
    List<uint8_t> getToken() { return m_token; }

    uint8_t getNumber() { return m_number++;}

    void handle(COAPPacket* p){m_handler(p);}

private:
    String m_href;
    String m_address;
    List<uint8_t> m_token;
    uint8_t m_number;
    COAPResponseHandler m_handler;
};

#endif // COAPOBSERVER_H
