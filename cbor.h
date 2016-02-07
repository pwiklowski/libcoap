#ifndef CBOR_H
#define CBOR_H

#include "String.h"
#include "List.h"
#include "Map.h"

using namespace std;

enum CborType_t {
    CBOR_TYPE_UNSIGNED,
    CBOR_TYPE_NEGATIVE,
    CBOR_TYPE_BINARY,
    CBOR_TYPE_String,
    CBOR_TYPE_ARRAY,
    CBOR_TYPE_MAP,
    CBOR_TYPE_TAGGED,
    CBOR_TYPE_SIMPLE,
    CBOR_TYPE_FLOAT
};


class cbor{
public:

    static cbor* parse(List<uint8_t>* data, uint16_t* p = 0){
        if(data->size() == 0)return 0;
        uint16_t pointer;
        if (p == 0)
            pointer = 0;
        else
            pointer = *p;

        uint8_t type = data->at(pointer++);
        CborType_t majorType = (CborType_t)(type >> 5);
        uint8_t minorType = (unsigned char) (type & 31);

        uint64_t value =0;
        if(minorType < 24) {
            value = minorType;
        } else if(minorType == 24) { // 1 byte
            value = data->at(pointer++);
        } else if(minorType == 25) { // 2 byte
            value = ((unsigned short) data->at(pointer) << 8) | ((unsigned short) data->at(pointer+ 1));
            pointer += 2;
        } else if(minorType == 26) { // 4 byte
            value = ((unsigned int) data->at(pointer) << 24) |
                  ((unsigned int) data->at(pointer+ 1) << 16) |
                  ((unsigned int) data->at(pointer+ 2) << 8 ) |
                  ((unsigned int) data->at(pointer+ 3));
            pointer +=4;
        } else {

        }

        cbor* aa = new cbor(majorType);

        if (majorType == CBOR_TYPE_ARRAY){
            for (uint16_t i=0; i<value; i++)
                aa->append(cbor::parse(data, &pointer));
        }else if (majorType == CBOR_TYPE_String){
            for (uint16_t i=0; i<value; i++)
                aa->data()->append(data->at(pointer++));
        }else if (majorType == CBOR_TYPE_MAP){
            for (uint16_t i=0; i<value; i++){
                cbor* key = cbor::parse(data, &pointer);
                cbor* value = cbor::parse(data, &pointer);
                aa->append(key, value);
            }
        }else if (majorType == CBOR_TYPE_NEGATIVE || majorType == CBOR_TYPE_UNSIGNED){
            if(value < 256ULL) {
                aa->data()->append(value);
            } else if(value < 65536ULL) {
                aa->data()->append(value >> 8);
                aa->data()->append(value);
            } else if(value < 4294967296ULL) {
                aa->data()->append(value >> 24);
                aa->data()->append(value >> 16);
                aa->data()->append(value >> 8);
                aa->data()->append(value);
            } else {
                aa->data()->append(value >> 56);
                aa->data()->append(value >> 48);
                aa->data()->append(value >> 40);
                aa->data()->append(value >> 32);
                aa->data()->append(value >> 24);
                aa->data()->append(value >> 16);
                aa->data()->append(value >> 8);
                aa->data()->append(value);
            }

        }


        if (p!=0)
            *p = pointer;


        return aa;
    }
    ~cbor(){
        m_data.clear();
    }

    cbor(String str) {
        m_type = CBOR_TYPE_String;
        for(uint16_t i=0; i<str.size();i++)
            m_data.append(str.at(i));
    }


    cbor(long long v) {
        unsigned long long value;
        if (value < 0){
            m_type = CBOR_TYPE_NEGATIVE;
            value = -(value+1);
        } else{
            value = v;
            m_type = CBOR_TYPE_UNSIGNED;
        }

        if(value < 256ULL) {
            m_data.append(value);
        } else if(value < 65536ULL) {
            m_data.append(value >> 8);
            m_data.append(value);
        } else if(value < 4294967296ULL) {
            m_data.append(value >> 24);
            m_data.append(value >> 16);
            m_data.append(value >> 8);
            m_data.append(value);
        } else {
            m_data.append(value >> 56);
            m_data.append(value >> 48);
            m_data.append(value >> 40);
            m_data.append(value >> 32);
            m_data.append(value >> 24);
            m_data.append(value >> 16);
            m_data.append(value >> 8);
            m_data.append(value);
        }



    }

    cbor(List<cbor*> array) {
        m_type = CBOR_TYPE_ARRAY;
        m_array = array;
    }

    static cbor* array(){
        return new cbor(CBOR_TYPE_ARRAY);
    }

    static cbor* map(){
        return new cbor(CBOR_TYPE_MAP);
    }

    cbor(CborType_t type){
        m_type = type;
    }

    CborType_t getType(){
        return m_type;
    }

    Map<cbor*, cbor*>* toMap() {
        if (m_type == CBOR_TYPE_MAP)
            return &m_map;
        else
            return 0;
    }


   cbor* getMapValue(String key) {
       if (m_type != CBOR_TYPE_MAP)
           return 0;

       for(cbor* k: m_map){

           if (k->compare(key)){
                return *m_map.get(k);
           }
       }
       return 0;

    }

   //workd only for Strings
    bool compare(String str){
        if (str.size() != m_data.size())
            return false;

        for (uint16_t i=0; i<str.size(); i++){
            if (str.at(i) != m_data.at(i))
                return false;
        }
        return true;
    }

    void append(cbor* key, cbor* value){
        m_map.insert(key, value);
    }
    void append(cbor* value){
        m_array.append(value);
    }

    List<cbor*>* toArray() {
        if (m_type == CBOR_TYPE_ARRAY)
            return &m_array;
        else
            return 0;
    }

    String toString(){
        String out;
        for(uint16_t i=0; i<m_data.size();i++)
            out.append(m_data.at(i));
        return out;
    }

    int toInt(){
        int value;

        if(m_data.size() == 1) {
            value = m_data.at(0);
        } else if(m_data.size()== 2) { // 2 byte
            value = (m_data.at(0) << 8) | m_data.at(1);
        } else if(m_data.size() == 4) { // 4 byte
            value = (m_data.at(0) << 24) |
                    (m_data.at(1) << 16) |
                    (m_data.at(2) << 8 ) |
                    (m_data.at(3));
        }
        if (m_type ==CBOR_TYPE_NEGATIVE)
            value = -(value+1);
        return value;
    }

    List<uint8_t>* data(){
        return &m_data;
    }


    void dump(List<uint8_t>* data){
       uint8_t majorType = m_type << 5;

       unsigned long long value = 0;
       if (m_type == CBOR_TYPE_ARRAY){
           value = m_array.size();
       }else if (m_type == CBOR_TYPE_MAP){
           value = m_map.size();
       }else if (m_type == CBOR_TYPE_String){
           value = m_data.size();
       }else if (m_type == CBOR_TYPE_UNSIGNED || m_type == CBOR_TYPE_NEGATIVE){
            if (m_data.size() == 1){
                if (m_data.at(0) < 24){
                    data->append(majorType | m_data.at(0));
                }else{
                    data->append(majorType | 24);
                    data->append(m_data.at(0));
                }
            }else if (m_data.size() > 1){

                if (m_data.size() == 2) data->append(majorType | 25);
                if (m_data.size() == 4) data->append(majorType | 26);
                if (m_data.size() == 8) data->append(majorType | 27);

                for (uint16_t i=0; i<m_data.size(); i++) data->append(m_data.at(i));
            }
       }

       if (m_type != CBOR_TYPE_UNSIGNED && m_type != CBOR_TYPE_NEGATIVE){
           if(value < 24ULL) {
               data->append(majorType | value);
           } else if(value < 256ULL) {
               data->append(majorType | 24);
               data->append(value);
           } else if(value < 65536ULL) {
               data->append(majorType | 25);
               data->append(value >> 8);
               data->append(value);
           } else if(value < 4294967296ULL) {
               data->append(majorType | 26);
               data->append(value >> 24);
               data->append(value >> 16);
               data->append(value >> 8);
               data->append(value);
           } else {
               data->append(majorType | 27);
               data->append(value >> 56);
               data->append(value >> 48);
               data->append(value >> 40);
               data->append(value >> 32);
               data->append(value >> 24);
               data->append(value >> 16);
               data->append(value >> 8);
               data->append(value);
           }
       }


       if (m_type == CBOR_TYPE_ARRAY){
           for(uint16_t i=0;i<m_array.size();i++){
               m_array.at(i)->dump(data);
           }
       } else if (m_type == CBOR_TYPE_MAP){
           for(cbor* key: m_map){
               cbor* value = *(m_map.get(key));

               key->dump(data);
               value->dump(data);
           }
       } else if (m_type == CBOR_TYPE_String){
            for(uint16_t i=0; i<m_data.size();i++)
                data->append(m_data.at(i));

       }



    }

    bool operator <(const cbor& rhs) const
    {
        return true;
    }
    bool is_string(){
        if (m_type == CBOR_TYPE_String)
            return true;
        else
            return false;

    }

    bool is_int(){
        if (m_type == CBOR_TYPE_NEGATIVE || m_type == CBOR_TYPE_UNSIGNED)
            return true;
        else
            return false;
    }

private:
    CborType_t m_type;
    List<uint8_t> m_data;
    List<cbor*> m_array;
    Map<cbor*, cbor*> m_map;
};


#endif // cbor_H
