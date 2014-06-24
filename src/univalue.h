// Copyright 2014 BitPay Inc.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//
// TODO: const-ify, harmonize pass-by-foo param usage ("type& foo")
//

#ifndef __UNIVALUE_H__
#define __UNIVALUE_H__

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <cassert>

class UniValue {
public:
    enum VType { VNULL, VOBJ, VARR, VSTR, VNUM, VTRUE, VFALSE, };

    UniValue() { typ = VNULL; }
    UniValue(UniValue::VType initialType, const std::string& initialStr = "") {
        typ = initialType;
        val = initialStr;
    }
    UniValue(int64_t val_) {
        setInt(val_);
    }
    ~UniValue() {}

    void clear();

    bool setNull();
    bool setBool(bool val);
    bool setNumStr(std::string val);
    bool setInt(int64_t val);
    bool setFloat(double val);
    bool setStr(std::string val);
    bool setArray();
    bool setObject();

    enum VType getType() const { return typ; }
    std::string getValStr() const { return val; }
    bool empty() const { return (values.size() == 0); }

    size_t size() const {
        switch (typ) {
        case VNULL:
            return 0;

        case VOBJ:
        case VARR:
            return values.size();

        case VSTR:
        case VNUM:
            return val.size();

        case VTRUE:
        case VFALSE:
            return 1;
        }
    }

    bool getArray(std::vector<UniValue>& arr);
    bool getObject(std::map<std::string,UniValue>& obj);
    bool checkObject(const std::map<std::string,UniValue::VType>& memberTypes);
    UniValue getByKey(const std::string& key);
    UniValue getByIdx(unsigned int index);

    bool isNull() const { return (typ == VNULL); }
    bool isTrue() const { return (typ == VTRUE); }
    bool isFalse() const { return (typ == VFALSE); }
    bool isBool() const { return (typ == VTRUE || typ == VFALSE); }
    bool isStr() const { return (typ == VSTR); }
    bool isNum() const { return (typ == VNUM); }
    bool isArray() const { return (typ == VARR); }
    bool isObject() const { return (typ == VOBJ); }

    bool push(UniValue& val);
    bool push(const std::string& val_) {
        UniValue tmpVal(VSTR, val_);
        return push(tmpVal);
    }

    bool pushKV(std::string key, UniValue& val);
    bool pushKV(std::string key, const UniValue& val_) {
        UniValue val = val_;
        return pushKV(key, val);
    }
    bool pushKV(std::string key, const std::string val) {
        UniValue tmpVal(VSTR, val);
        return pushKV(key, tmpVal);
    }
    bool pushKV(std::string key, int64_t val) {
        UniValue tmpVal(val);
        return pushKV(key, tmpVal);
    }

    std::string write(unsigned int prettyIndent = 0,
                      unsigned int indentLevel = 0) const;

    bool read(const char *raw);
    bool read(std::string rawStr) {
        return read(rawStr.c_str());
    }

private:
    UniValue::VType typ;
    std::string val;                       // numbers are stored as C++ strings
    std::vector<std::string> keys;
    std::vector<UniValue> values;

    int findKey(const std::string& key);
    void writeArray(unsigned int prettyIndent, unsigned int indentLevel, std::string& s) const;
    void writeObject(unsigned int prettyIndent, unsigned int indentLevel, std::string& s) const;
};

#endif // __UNIVALUE_H__
