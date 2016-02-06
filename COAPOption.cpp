#include "COAPOption.h"

COAPOption::COAPOption(uint8_t num, List<uint8_t> data)
{
    m_num = num;
    m_data = data;
}

COAPOption::COAPOption(uint8_t num, char* data)
{
    m_num = num;

    char* b = data;
    while(*(b) != 0){
        m_data.append(*b);
        b++;
    }
}
