#ifndef COAPOPTION_H
#define COAPOPTION_H

#include "List.h"
#include <stdint.h>


class COAPOption
{
public:
    COAPOption(uint8_t num, List<uint8_t> data);
    COAPOption(uint8_t num, char *data);
    uint8_t getNumber() { return m_num;}
    List<uint8_t>* getData() { return &m_data;}

private:
    uint8_t m_num;
    List<uint8_t> m_data;
};

#endif // COAPOPTION_H
