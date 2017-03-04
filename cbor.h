#ifndef CBOR_H
#define CBOR_H

#include "String.h"
#include "List.h"
#include "SimpleList.h"
#include "Map.h"
#include <stdint.h>
#include <stdlib.h>

typedef enum {
    CBOR_TYPE_UNSIGNED,
    CBOR_TYPE_NEGATIVE,
    CBOR_TYPE_BINARY,
    CBOR_TYPE_String,
    CBOR_TYPE_ARRAY,
    CBOR_TYPE_MAP,
    CBOR_TYPE_TAGGED,
    CBOR_TYPE_FLOAT,

    CBOR_TYPE_BOOLEAN //officialy it doesn't exists but it is more convinient to have special type for it
}CborType_t ;


class cbor{
public:

    cbor(){
    }

    cbor(const cbor& l){
        m_type = l.m_type;
        m_data = l.m_data;
        m_array = l.m_array;
        m_map = l.m_map;
    }

    static void parse(cbor* cb, Vector<uint8_t>* data, uint16_t* p = 0){
        if(data->size() == 0)return;
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
        cb->m_type = majorType;

        if (majorType == CBOR_TYPE_ARRAY){
            for (uint16_t i=0; i<value; i++){
                cbor a;
                cbor::parse(&a, data, &pointer);
                cb->append(a);
            }
        }else if (majorType == CBOR_TYPE_String){
            for (uint16_t i=0; i<value; i++)
                cb->data()->append(data->at(pointer++));
        }else if (majorType == CBOR_TYPE_MAP){
            for (uint16_t i=0; i<value; i++){
                cbor key;
                cbor::parse(&key, data, &pointer);
                cbor value;
                cbor::parse(&value, data, &pointer);

                cb->append(key, value);
            }
        }else if (majorType == CBOR_TYPE_NEGATIVE || majorType == CBOR_TYPE_UNSIGNED){
            cbor::parseNumber(cb->data(), value);
        }else if(majorType == CBOR_TYPE_FLOAT && value < 24){
            cb->m_type = CBOR_TYPE_BOOLEAN;
            cb->data()->append((uint8_t)value);

        }

        if (p!=0)
            *p = pointer;
    }
    ~cbor(){
        m_data.clear();
        m_map.clear();
        m_array.clear();
    }

    cbor(const char* str) {
        m_type = CBOR_TYPE_String;
        for(uint16_t i=0; i<strlen(str);i++)
            m_data.append(str[i]);
    }
    cbor(const String& str) {
        m_type = CBOR_TYPE_String;
        for(uint16_t i=0; i<str.size();i++)
            m_data.append(str.at(i));
    }
    cbor(bool value) {
        m_type = CBOR_TYPE_BOOLEAN;
        m_data.append(value ? 21: 20);
    }


    cbor(long long v) {
        parseNumberValue(&m_data, v);
    }

    cbor(int v) {
        parseNumberValue(&m_data, v);
    }

    cbor(List<cbor> array) {
        m_type = CBOR_TYPE_ARRAY;
        m_array = array;
    }

    cbor(CborType_t type){
        m_type = type;
    }

    CborType_t getType(){
        return m_type;
    }

    Map<cbor, cbor>* toMap() {
        if (m_type == CBOR_TYPE_MAP)
            return &m_map;
        else
            return 0;
    }


   cbor getMapValue(String key) {
       for(cbor k: m_map){
           if (k.toString() == key){
                return m_map.get(k);
           }
       }
    }

    void append(const cbor& key, const cbor& value){
        m_map.insert(key, value);
    }
    void append(const cbor& value){
        m_array.append(value);
    }

    List<cbor>* toArray() {
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

    bool toBool(){
        return m_data.at(0) == 21;
    }

    Vector<uint8_t>* data(){
        return &m_data;
    }

    void dump(Vector<uint8_t>* data){
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
       }else if (m_type == CBOR_TYPE_BOOLEAN){
           uint8_t d = CBOR_TYPE_FLOAT << 5;

           data->append((CBOR_TYPE_FLOAT<<5) | m_data.at(0));
       }
       //TODO: add support for float values

       //handle complex types
       if (m_type != CBOR_TYPE_UNSIGNED && m_type != CBOR_TYPE_NEGATIVE && m_type != CBOR_TYPE_BOOLEAN){
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
               m_array.at(i).dump(data);
           }
       } else if (m_type == CBOR_TYPE_MAP){
           for(cbor key: m_map){
               cbor value = m_map.get(key);

               key.dump(data);
               value.dump(data);
           }
       } else if (m_type == CBOR_TYPE_String){
            for(uint16_t i=0; i<m_data.size();i++)
                data->append(m_data.at(i));

       }
    }

    void parseNumberValue(Vector<uint8_t>* data, long long v){
        long long value;
        if (v < 0){
            m_type = CBOR_TYPE_NEGATIVE;
            value = -(v+1);
        } else{
            value = v;
            m_type = CBOR_TYPE_UNSIGNED;
        }
        cbor::parseNumber(data, value);
    }
    static void parseNumber(Vector<uint8_t>* data, long long value){
        if(value < 256) {
            data->append(value);
        } else if(value < 65536) {
            data->append(value >> 8);
            data->append(value);
        } else if(value < 4294967296) {
            data->append(value >> 24);
            data->append(value >> 16);
            data->append(value >> 8);
            data->append(value);
        } else {
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

    bool operator <(const cbor& rhs) const
    {
        return true;
    }

    bool operator == (const cbor& rhs) const
    {
        if (m_type != rhs.m_type) return false;

        if (m_type == CBOR_TYPE_String || m_type == CBOR_TYPE_UNSIGNED || m_type == CBOR_TYPE_NEGATIVE)
            return m_data == rhs.m_data;

        return true;
    }
    cbor& operator = (const cbor& rhs) {
        m_data.clear();
        m_array.clear();
        m_map.clear();

        m_type = rhs.m_type;
        m_data = rhs.m_data;
        m_array = rhs.m_array;
        m_map = rhs.m_map;

        return *this;
    }
    bool operator != (const cbor& rhs) const
    {
        if (*this == rhs) return false;
        return true;
    }

    bool is_array(){
        return m_type == CBOR_TYPE_ARRAY;
    }

    bool is_string(){
        return m_type == CBOR_TYPE_String;
    }

    bool is_bool(){
        return m_type == CBOR_TYPE_BOOLEAN;
    }

    bool is_int(){
        return m_type == CBOR_TYPE_NEGATIVE || m_type == CBOR_TYPE_UNSIGNED;
    }
    const char *parse_string(const char *str)
    {
         char* p =  const_cast<char*>(str+1);

         m_type = CBOR_TYPE_String;
         while(*p != '\"'){
             m_data.append(*p++);
         }
         p++;
         return p;
    }

    const char *parse_boolean(const char *str, bool value)
    {
         char* p =  const_cast<char*>(str+1);

         m_type = CBOR_TYPE_BOOLEAN;
         m_data.append(value ? 21 : 20);

         return p + (value ? 4 : 5);
    }

    const char *skip(const char *in) {while (in && *in && (unsigned char)*in<=32) in++; return in;}

    const char *parse_object(const char *value)
    {
        m_type = CBOR_TYPE_MAP;
        cbor key;
        cbor val;

        if (*value!='{')	{return 0;}	/* not an object! */

        value=skip(value+1);
        if (*value=='}') return value+1;	/* empty array. */

        value=skip(key.parse_string(skip(value)));

        if (*value!=':') {return 0;}	/* fail! */
        value=skip(val.parse_value(skip(value+1)));	/* skip any spacing, get the value. */
        if (!value) return 0;

        m_map.insert(key, val);

        while (*value==',')
        {
                cbor key;
                cbor val;
                value=skip(key.parse_string(skip(value+1)));
                if (!value) return 0;

                if (*value!=':') {return 0;}	/* fail! */
                value=skip(val.parse_value(skip(value+1)));	/* skip any spacing, get the value. */
                if (!value) return 0;

                m_map.insert(key, val);
        }

        if (*value=='}') return value+1;	/* end of array */
        return 0;	/* malformed. */
    }


    const char *parse_array(const char *value)
    {
        m_type = CBOR_TYPE_ARRAY;

        if (*value!='[')	{return 0;}	/* not an array! */

        value=skip(value+1);
        if (*value==']') return value+1;	/* empty array. */

        cbor child;
        value=skip(child.parse_value(skip(value)));	/* skip any spacing, get the value. */
        if (!value) return 0;
        append(child);

        while (*value==',')
        {
            cbor new_item;
            value=skip(new_item.parse_value(skip(value+1)));
            m_array.append(new_item);
        }

        if (*value==']') return value+1;	/* end of array */
        return 0;	/* malformed. */
    }


    const char *parse_number(const char *number)
    {
        m_data.clear();
        const char* num = skip(number);
        int sign=1;
        long long n=0;

        if (*num=='-') sign=-1,num++;
        if (*num=='0') num++;
        if (*num>='1' && *num<='9'){
            do{
                n = (n*10.0)+(*num++ -'0');
            }while (*num>='0' && *num<='9');
        }

        parseNumberValue(&m_data, n);
        return num;
    }
    const char *parse_value(const char *value)
    {
        if (!value)						return 0;
        if (!strncmp(value,"false",5))	{ return this->parse_boolean(value, false); }
        if (!strncmp(value,"true",4))	{ return this->parse_boolean(value, true); }
        if (*value=='\"')				{ return this->parse_string(value); }
        if (*value=='-' || (*value>='0' && *value<='9')){ return this->parse_number(value); }
        if (*value=='[')				{ return this->parse_array(value); }
        if (*value=='{')				{ return this->parse_object(value); }

        return 0;	/* failure. */
    }






    static String toJsonString(cbor* c){
        String res;

        if (c->getType()== CBOR_TYPE_MAP){
            res.append("{");
            for (cbor key: *c->toMap()){
                cbor value = c->toMap()->get(key);
                res.append(toJsonString(&key));
                res.append(": ");
                res.append(toJsonString(&value));
                if(key != c->toMap()->keys().at(  c->toMap()->keys().size()-1)){
                    res.append(",");
                    res.append("\n");
                }

            }
            res.append("}");
        }else if (c->getType() ==CBOR_TYPE_ARRAY){
            res.append("[");
            for (cbor value: *c->toArray()){
                res.append(toJsonString(&value));

                if(value != c->toArray()->at(c->toArray()->size()-1)){
                    res.append(", ");
                    res.append("\n");
                }
            }
            res.append("]");
        }else if (c->getType() == CBOR_TYPE_BOOLEAN){
            res.append(c->data()->at(0) == 21 ? "true" : "false");
        }else if (c->getType() == CBOR_TYPE_String){
            res.append("\"");
            res.append(c->toString());
            res.append("\"");
        }else if (c->getType() == CBOR_TYPE_NEGATIVE || c->getType() == CBOR_TYPE_UNSIGNED){
            char temp[20];
            itoa(c->toInt(), temp);
            res.append(temp);
        }


        return res;
    }



private:
    static void reverse(char* s)
    {
        int i, j;
        char c;

        for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
            c = s[i];
            s[i] = s[j];
            s[j] = c;
        }
    }

    static void itoa(int n, char* s)
    {
         int i, sign;

         if ((sign = n) < 0)  /* записываем знак */
             n = -n;          /* делаем n положительным числом */
         i = 0;
         do {       /* генерируем цифры в обратном порядке */
             s[i++] = n % 10 + '0';   /* берем следующую цифру */
         } while ((n /= 10) > 0);     /* удаляем */
         if (sign < 0)
             s[i++] = '-';
         s[i] = '\0';
         reverse(s);
    }

    CborType_t m_type;
    Vector<uint8_t> m_data;
    List<cbor> m_array;
    Map<cbor, cbor> m_map;
};


#endif // cbor_H
