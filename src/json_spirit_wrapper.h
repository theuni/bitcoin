#ifndef __JSON_SPIRIT_WRAPPER_H__
#define __JSON_SPIRIT_WRAPPER_H__

#include "univalue/univalue.h"

namespace json_spirit {

class Array : public UniValue
{
public:
    Array() : UniValue(VARR) {}
    Array(const UniValue& in) : UniValue(in) {}
};

class Object : public UniValue
{
public:
    Object() : UniValue(VOBJ) {}
    Object(const UniValue& in) : UniValue(in) {}
};

typedef UniValue Value;
typedef UniValue::VType Value_type;

}

#define find_value(val,key) (val[key])

#endif // __JSON_SPIRIT_WRAPPER_H__
