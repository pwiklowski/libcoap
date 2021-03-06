#include "COAPPacket.h"
#include <cstdio>
#include  <cstring>
#include "log.h"


#define packet_log(line, ...) //log(line, ## __VA_ARGS__ )

COAPPacket* COAPPacket::parse(uint8_t* data, size_t len, String address){

    COAPPacket* p = new COAPPacket();

    if (!p->parseHeader(data, len)){
        delete p;
        return nullptr;
    }
    if (!p->parseToken(data,len)){
        return nullptr;
        delete p;
        return nullptr;
    }
    if (!p->parseOptions(data, len)){
        delete p;
        return nullptr;
    }


//    packet_log("Header:\n");
//    packet_log("  ver  0x%02X\n", p->hdr.ver);
//    packet_log("  t    0x%02X\n", p->hdr.t);
//    packet_log("  tkl  0x%02X\n", p->hdr.tkl);
//    packet_log("  code 0x%02X\n", p->hdr.code);
//    packet_log("  id   0x%02X\n", p->hdr.mid);

    p->setAddress(address);
    return p;
}


COAPPacket::~COAPPacket(){
    packet_log("delete packet %d %d\n", hdr.mid, getType());
    for(uint16_t i=0; i<m_options.size(); i++)
    {
        delete m_options.at(i);
    }

}

bool COAPPacket::parseHeader(const uint8_t *buf, size_t buflen)
{
    if (buflen < 4)
        return false;
    hdr.ver = (buf[0] & 0xC0) >> 6;
    if (hdr.ver != 1)
        return false;
    hdr.t = (buf[0] & 0x30) >> 4;
    hdr.tkl = buf[0] & 0x0F;
    hdr.code = buf[1];
    hdr.mid = buf[3] <<8 | buf[2];
    return true;
}
bool COAPPacket::parseToken(const uint8_t *buf, size_t buflen)
{
    if (hdr.tkl <= 8)
    {
        if (4U + hdr.tkl > buflen)
            return false;
        for (int i=0; i<hdr.tkl;i++)
            m_token.append(*(buf+4+i));
        return true;
    }
    return false;
}
bool COAPPacket::parseOptions(const uint8_t *buf, size_t buflen)
{
    uint16_t delta = 0;
    const uint8_t *p = buf + 4 + hdr.tkl;
    const uint8_t *end = buf + buflen;
    uint8_t optionsNumber = 0;

    while((p < end) && (*p != 0xFF))
    {
        if (!parseOption(&delta, &p, end-p))
            return false;
        optionsNumber++;
    }

    if (p+1 < end && *p == 0xFF)  // payload marker
    {
        while(p<=end){
            m_payload.append(*(p+1));
            p++;
        }
    }

    return true;
}
bool COAPPacket::parseOption(uint16_t *running_delta, const uint8_t **buf, size_t buflen)
{
    const uint8_t *p = *buf;
    uint16_t len, delta;

    delta = (p[0] & 0xF0) >> 4;
    len = p[0] & 0x0F;

    List<uint8_t> data;

    if (len == 15){
        len = 15 + p[1];
        for (int i=0; i<len;i++) data.append(*(p+2+i));
        *buf = p + 2 + len;
    }else{
        for (int i=0; i<len;i++) data.append(*(p+1+i));
        *buf = p + 1 + len;
    }

    m_options.append(new COAPOption(delta + *running_delta, data));
    *running_delta += delta;
    return true;
}


int COAPPacket::build(uint8_t *buf, size_t *buflen)
{
    size_t opts_len = 0;
    uint8_t *p;
    uint16_t running_delta = 0;

    buf[0] = (hdr.ver & 0x03) << 6;
    buf[0] |= (hdr.t & 0x03) << 4;
    buf[0] |= (hdr.tkl & 0x0F);
    buf[1] = hdr.code;
    buf[2] = hdr.mid;
    buf[3] = hdr.mid >> 8;

    // inject token
    p = buf + 4;

    for (uint8_t i=0; i<m_token.size();i++)
        *(p++) = m_token.at(i);

    for (COAPOption* option: m_options) {

        uint32_t optDelta;
        uint8_t len, delta = 0;

        optDelta = option->getNumber() - running_delta;

        if (option->getData()->size()> 15){
            len = 15;
        }else{
            len = option->getData()->size();
        }

        *p++ = (0xFF & (optDelta << 4 | len));
        if (delta == 14)
        {
            *p++ = ((optDelta-269) >> 8);
            *p++ = (0xFF & (optDelta-269));
        }
        if (option->getData()->size() > 15)
        {
            *p++ = 0xFF & (option->getData()->size()-15);
        }

        for (uint16_t i=0; i<option->getData()->size();i++){
            *(p++) = option->getData()->at(i);
        }

        running_delta = option->getNumber();
    }

    opts_len = (p - buf) - 4;   // number of bytes used by options

    if (m_payload.size()> 0)
    {
        buf[4 + opts_len] = 0xFF;  // payload marker

        for (uint16_t i=0; i<m_payload.size(); i++){
            buf[5 + opts_len +i ] = m_payload.at(i);  // payload marker
        }
        *buflen = opts_len + 5 + m_payload.size();
    }
    else
        *buflen = opts_len + 4;
    return 0;
}


String COAPPacket::getUri(){
    String uri;
    for(uint8_t i=0; i<m_options.size(); i++) {
        packet_log("option %d\n", i);
        if ((*m_options.at(i)).getNumber() == COAP_OPTION_URI_PATH){
            uri.append("/");
            List<uint8_t>* data = (*m_options.at(i)).getData();

            for(uint16_t j=0; j<data->size(); j++) {
                uri.append(data->at(j));
            }
        }
    }
    return uri;
}


void COAPPacket::parseUri(COAPPacket* p, String uri){
    size_t start = 1;
    int end;

    while((end = uri.find("/" , start)) != -1){
        p->addOption(new COAPOption(COAP_OPTION_URI_PATH, (char*) uri.substr(start, end-start).c_str()));
        start = end +1;
    }
    p->addOption(new COAPOption(COAP_OPTION_URI_PATH, (char*) uri.substr(start, uri.size()).c_str()));
}



COAPPacket::COAPPacket(){
    hdr.ver = 0x01;
    hdr.t = COAP_TYPE_ACK;
    hdr.tkl = 0;
    hdr.code= 0;
    hdr.mid = 0;
    hdr.mid_valid = false;
}

void COAPPacket::addPayload(String payload){
    for (uint16_t i=0; i<payload.size(); i++){
        m_payload.append(payload.at(i));
    }
}

void COAPPacket::addPayload(uint8_t* payload, uint16_t size){
    for (int i=0; i<size; i++){
        m_payload.append(*(payload+i));
    }
}

COAPOption* COAPPacket::getOption(coap_option_num_t option){

    for(uint8_t i=0; i<m_options.size(); i++) {
        if ((*m_options.at(i)).getNumber() == option){
            return m_options.at(i);
        }
    }
    return 0;
}
