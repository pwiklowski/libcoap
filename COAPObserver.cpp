#include "COAPObserver.h"
#include "log.h"


COAPObserver::COAPObserver(String address, String href, List<uint8_t> token)
{
    m_number = 5;
    log("New observer %s %s\n", address.c_str(), href.c_str());

    m_address = address;
    m_href = href;
    m_token = token;
}

COAPObserver::COAPObserver(String address, String href, List<uint8_t> token, COAPResponseHandler handler){

    m_address = address;
    m_href = href;
    m_token = token;

    m_handler = handler;
}

