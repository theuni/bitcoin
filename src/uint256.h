// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UINT256_H
#define BITCOIN_UINT256_H

#include <assert.h>
#include <cstring>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>
#include "crypto/common.h"
#include <array>

/** Template base class for fixed-sized opaque blobs. */
template<unsigned int BITS>
class base_blob
{
protected:
    static constexpr unsigned int WIDTH = BITS/8;
    std::array<uint8_t, WIDTH> data;
public:
    constexpr base_blob() : data {}{}

    explicit base_blob(const std::vector<unsigned char>& vch);

    explicit constexpr base_blob(std::array<uint8_t, WIDTH> vch) : data(vch){}

    bool IsNull() const
    {
        for(const auto& i : data)
            if(i != 0)
                return false;
        return true;
    }

    void SetNull()
    {
        data.fill(0);
    }

    inline int Compare(const base_blob& other) const { return memcmp(data.data(), other.data.data(), size()); }

    friend inline bool operator==(const base_blob& a, const base_blob& b) { return a.Compare(b) == 0; }
    friend inline bool operator!=(const base_blob& a, const base_blob& b) { return a.Compare(b) != 0; }
    friend inline bool operator<(const base_blob& a, const base_blob& b) { return a.Compare(b) < 0; }

    std::string GetHex() const;
    void SetHex(const char* psz);
    void SetHex(const std::string& str);
    std::string ToString() const;

    unsigned char* begin()
    {
        return data.data();
    }

    unsigned char* end()
    {
        return data.data() + size();
    }

    const unsigned char* begin() const
    {
        return data.data();
    }

    const unsigned char* end() const
    {
        return data.data() + size();
    }

    static constexpr unsigned int size()
    {
        return WIDTH;
    }

    unsigned int GetSerializeSize(int nType, int nVersion) const
    {
        return size();
    }

    uint64_t GetUint64(int pos) const
    {
        const uint8_t* ptr = data.data() + pos * 8;
        return ((uint64_t)ptr[0]) | \
               ((uint64_t)ptr[1]) << 8 | \
               ((uint64_t)ptr[2]) << 16 | \
               ((uint64_t)ptr[3]) << 24 | \
               ((uint64_t)ptr[4]) << 32 | \
               ((uint64_t)ptr[5]) << 40 | \
               ((uint64_t)ptr[6]) << 48 | \
               ((uint64_t)ptr[7]) << 56;
    }

    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const
    {
        s.write((char*)data.data(), size());
    }

    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion)
    {
        s.read((char*)data.data(), size());
    }
};

/** 160-bit opaque blob.
 * @note This type is called uint160 for historical reasons only. It is an opaque
 * blob of 160 bits and has no integer operations.
 */
class uint160 : public base_blob<160> {
public:
    uint160() {}
    uint160(const base_blob<160>& b) : base_blob<160>(b) {}
    explicit uint160(const std::vector<unsigned char>& vch) : base_blob<160>(vch) {}
    constexpr explicit uint160(std::array<uint8_t, 20> vch) : base_blob(vch){}
};

/** 256-bit opaque blob.
 * @note This type is called uint256 for historical reasons only. It is an
 * opaque blob of 256 bits and has no integer operations. Use arith_uint256 if
 * those are required.
 */
class uint256 : public base_blob<256> {
public:
    uint256() {}
    uint256(const base_blob<256>& b) : base_blob<256>(b) {}
    explicit uint256(const std::vector<unsigned char>& vch) : base_blob<256>(vch) {}
    constexpr explicit uint256(std::array<uint8_t, 32> vch) : base_blob(vch){}

    /** A cheap hash function that just returns 64 bits from the result, it can be
     * used when the contents are considered uniformly random. It is not appropriate
     * when the value can easily be influenced from outside as e.g. a network adversary could
     * provide values to trigger worst-case behavior.
     */
    uint64_t GetCheapHash() const
    {
        return ReadLE64(data.data());
    }
};

template <size_t len>
struct arraymaker
{
    static constexpr char ascii2int(const char in)
    {
        return in >= 48 && in <= 57  ? in - 48 :
               in >= 65 && in <= 70  ? in - 55 :
               in >= 97 && in <= 102 ? in - 87 : -1;
    }

    template <int n>
    static constexpr uint8_t getn(const std::array<char, len * 2>& arr)
    {
        static_assert(n < len * 2, "index out of range");
        return static_cast<uint8_t>(ascii2int((std::get<n>(arr))) << 4 | ascii2int((std::get<n+1>(arr))));
    }

    static constexpr std::array<uint8_t, len> parse(std::array<char, len * 2> arr);

    template<char a, char b, char... c >
    static constexpr std::array<uint8_t, len> make()
    {
        static_assert(a == '0',"hex literal must begin with 0x");
        static_assert(b == 'x' || b == 'X',"hex literal must begin with 0x");
        return parse({{c...}});
    }
};

template<>
constexpr std::array<uint8_t, 32> arraymaker<32>::parse(std::array<char, 32 * 2> arr)
{
    return  {{
                getn<62>(arr), getn<60>(arr), getn<58>(arr), getn<56>(arr), getn<54>(arr), getn<52>(arr), getn<50>(arr), getn<48>(arr),
                getn<46>(arr), getn<44>(arr), getn<42>(arr), getn<40>(arr), getn<38>(arr), getn<36>(arr), getn<34>(arr), getn<32>(arr),
                getn<30>(arr), getn<28>(arr), getn<26>(arr), getn<24>(arr), getn<22>(arr), getn<20>(arr), getn<18>(arr), getn<16>(arr),
                getn<14>(arr), getn<12>(arr), getn<10>(arr), getn< 8>(arr), getn< 6>(arr), getn< 4>(arr), getn< 2>(arr), getn< 0>(arr)
            }};
}

template<>
constexpr std::array<uint8_t, 20> arraymaker<20>::parse(std::array<char, 20 * 2> arr)
{
    return  {{
                getn<18>(arr), getn<16>(arr), getn<14>(arr), getn<12>(arr), getn<10>(arr),
                getn< 8>(arr), getn< 6>(arr), getn< 4>(arr), getn< 2>(arr), getn< 0>(arr)
            }};
}

template< char... c >
constexpr uint256 operator "" _u256 () {
    static_assert(sizeof...(c) == 66, "bad length");
    return uint256{arraymaker<32>::make<c...>()};
}

template< char... c >
constexpr uint160 operator "" _u160 () {
    static_assert(sizeof...(c) == 42, "bad length");
    return uint160{arraymaker<20>::make<c...>()};
}

/* uint256 from const char *.
 * This is a separate function because the constructor uint256(const char*) can result
 * in dangerously catching uint256(0).
 */
inline uint256 uint256S(const char *str)
{
    uint256 rv;
    rv.SetHex(str);
    return rv;
}
/* uint256 from std::string.
 * This is a separate function because the constructor uint256(const std::string &str) can result
 * in dangerously catching uint256(0) via std::string(const char*).
 */
inline uint256 uint256S(const std::string& str)
{
    uint256 rv;
    rv.SetHex(str);
    return rv;
}

#endif // BITCOIN_UINT256_H
