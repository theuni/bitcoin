// Copyright (c) 2025-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/chacha20_vec.h>

#include <bit>
#include <cassert>
#include <cstring>
#include <limits>

#if defined(ENABLE_CHACHA20_VEC)

#if defined(CHACHA20_VEC_DISABLE_STATES_16) && \
    defined(CHACHA20_VEC_DISABLE_STATES_8) && \
    defined(CHACHA20_VEC_DISABLE_STATES_6) && \
    defined(CHACHA20_VEC_DISABLE_STATES_4) && \
    defined(CHACHA20_VEC_DISABLE_STATES_2)
#define CHACHA20_VEC_ALL_MULTI_STATES_DISABLED
#endif


#if !defined(CHACHA20_VEC_ALL_MULTI_STATES_DISABLED)

#if defined(__has_attribute)
#  if __has_attribute(always_inline)
#    define ALWAYS_INLINE __attribute__ ((always_inline)) inline
#  endif
#endif

#if !defined(ALWAYS_INLINE)
#  define ALWAYS_INLINE inline
#endif


namespace {

#if defined(__AVX__)
static constexpr bool enable_avx = true;
#else
static constexpr bool enable_avx = false;
#endif

#if defined(__AVX2__)
static constexpr bool enable_avx2 = true;
#else
static constexpr bool enable_avx2 = false;
#endif

static constexpr bool enable_256bit_operations = enable_avx2;

class [[maybe_unused]] vec256_4x32x2
{
    using vec_type = uint32_t __attribute__((__vector_size__(16)));
    vec_type m_vec0;
    vec_type m_vec1;
public:

    [[maybe_unused]] ALWAYS_INLINE constexpr vec256_4x32x2(uint32_t x0, uint32_t x1, uint32_t x2, uint32_t x3, uint32_t x4, uint32_t x5, uint32_t x6, uint32_t x7)
        : m_vec0{x0, x1, x2, x3}, m_vec1{x4, x5, x6, x7} {}

    [[maybe_unused]] ALWAYS_INLINE constexpr vec256_4x32x2() noexcept = default;

    [[maybe_unused]] ALWAYS_INLINE constexpr vec256_4x32x2& operator+=(const vec256_4x32x2& rhs)
    {
        m_vec0 += rhs.m_vec0;
        m_vec1 += rhs.m_vec1;
        return *this;
    }
    [[maybe_unused]] ALWAYS_INLINE constexpr vec256_4x32x2& operator^=(const vec256_4x32x2& rhs)
    {
        m_vec0 ^= rhs.m_vec0;
        m_vec1 ^= rhs.m_vec1;
        return *this;
    }

    /** Left-rotate vector */
    template <int BITS>
    ALWAYS_INLINE
    constexpr void rotl()
    {
        using vec128_u8 = uint8_t __attribute__((__vector_size__(16)));
        if constexpr(enable_avx && BITS == 16) {
            m_vec0 = (vec_type)__builtin_shufflevector(reinterpret_cast<vec128_u8>(m_vec0), vec128_u8{}, 2,3,0,1,6,7,4,5,10,11,8,9,14,15,12,13);
            m_vec1 = (vec_type)__builtin_shufflevector(reinterpret_cast<vec128_u8>(m_vec1), vec128_u8{}, 2,3,0,1,6,7,4,5,10,11,8,9,14,15,12,13);
        } else if constexpr(enable_avx && BITS == 8) {
            m_vec0 = (vec_type)__builtin_shufflevector(reinterpret_cast<vec128_u8>(m_vec0), vec128_u8{}, 3,0,1,2,7,4,5,6,11,8,9,10,15,12,13,14);
            m_vec1 = (vec_type)__builtin_shufflevector(reinterpret_cast<vec128_u8>(m_vec1), vec128_u8{}, 3,0,1,2,7,4,5,6,11,8,9,10,15,12,13,14);
        } else {
            m_vec0 = (m_vec0 << BITS) | (m_vec0 >> (32 - BITS));
            m_vec1 = (m_vec1 << BITS) | (m_vec1 >> (32 - BITS));
        }
    }

    template <int A, int B, int C, int D>
    ALWAYS_INLINE constexpr void shuf()
    {
        m_vec0 = vec_type{m_vec0[A], m_vec0[B], m_vec0[C], m_vec0[D]};
        m_vec1 = vec_type{m_vec1[A], m_vec1[B], m_vec1[C], m_vec1[D]};
    }

    [[maybe_unused]] ALWAYS_INLINE constexpr void xor_write(std::span<uint32_t, 8> data)
    {
        byteswap(m_vec0);
        byteswap(m_vec1);
        m_vec0 ^= (vec_type){data[0], data[1], data[2], data[3]};
        m_vec1 ^= (vec_type){data[4], data[5], data[6], data[7]};
        data[0] = m_vec0[0];
        data[1] = m_vec0[1];
        data[2] = m_vec0[2];
        data[3] = m_vec0[3];
        data[4] = m_vec1[0];
        data[5] = m_vec1[1];
        data[6] = m_vec1[2];
        data[7] = m_vec1[3];
    }

    [[maybe_unused]] ALWAYS_INLINE constexpr void split(vec256_4x32x2& rhs)
    {
        vec_type temp = rhs.m_vec1;
        rhs.m_vec1 = rhs.m_vec0;
        rhs.m_vec0 = m_vec0;
        m_vec0 = m_vec1;
        m_vec1 = temp;
    }


    ALWAYS_INLINE static constexpr void byteswap(vec_type& vec)
    {
        if constexpr (std::endian::native == std::endian::big)
        {
            vec[0] = __builtin_bswap32(vec[0]);
            vec[1] = __builtin_bswap32(vec[1]);
            vec[2] = __builtin_bswap32(vec[2]);
            vec[3] = __builtin_bswap32(vec[3]);
        }
    }
};


class vec256_8x32x1
{
    using vec_type = uint32_t __attribute__((__vector_size__(32)));
    vec_type m_vec;
public:

    [[maybe_unused]] ALWAYS_INLINE constexpr vec256_8x32x1(uint32_t x0, uint32_t x1, uint32_t x2, uint32_t x3, uint32_t x4, uint32_t x5, uint32_t x6, uint32_t x7) : m_vec{x0, x1, x2, x3, x4, x5, x6, x7}
    {}

    [[maybe_unused]] ALWAYS_INLINE constexpr vec256_8x32x1() noexcept = default;

    [[maybe_unused]] ALWAYS_INLINE constexpr vec256_8x32x1& operator+=(const vec256_8x32x1& rhs)
    {
        m_vec += rhs.m_vec;
        return *this;
    }
    [[maybe_unused]] ALWAYS_INLINE constexpr vec256_8x32x1& operator^=(const vec256_8x32x1& rhs)
    {
        m_vec ^= rhs.m_vec;
        return *this;
    }

    /** Left-rotate vector */
    template <int BITS>
    ALWAYS_INLINE constexpr void rotl()
    {
        using vec256_u8 = uint8_t __attribute__((__vector_size__(32)));

        if constexpr(enable_avx2 && BITS == 16) {
            m_vec = (vec_type)__builtin_shufflevector(reinterpret_cast<vec256_u8>(m_vec), vec256_u8{}, 2,3,0,1,6,7,4,5,10,11,8,9,14,15,12,13,18,19,16,17,22,23,20,21,26,27,24,25,30,31,28,29);
        } else if constexpr(enable_avx2 && BITS == 8) {
            m_vec = (vec_type)__builtin_shufflevector(reinterpret_cast<vec256_u8>(m_vec), vec256_u8{}, 3,0,1,2,7,4,5,6,11,8,9,10,15,12,13,14,19,16,17,18,23,20,21,22,27,24,25,26,31,28,29,30);
        } else {
            m_vec = (m_vec << BITS) | (m_vec >> (32 - BITS));
        }
    }

    template <int A, int B, int C, int D>
    ALWAYS_INLINE constexpr void shuf()
    {
        m_vec = vec_type{m_vec[A], m_vec[B], m_vec[C], m_vec[D], m_vec[A + 4], m_vec[B + 4], m_vec[C + 4], m_vec[D + 4]};
    }

    [[maybe_unused]] ALWAYS_INLINE constexpr void xor_write(std::span<uint32_t, 8> data)
    {
        byteswap(m_vec);
        m_vec ^= (vec_type){data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]};
        data[0] = m_vec[0];
        data[1] = m_vec[1];
        data[2] = m_vec[2];
        data[3] = m_vec[3];
        data[4] = m_vec[4];
        data[5] = m_vec[5];
        data[6] = m_vec[6];
        data[7] = m_vec[7];
    }

    [[maybe_unused]] ALWAYS_INLINE constexpr void split(vec256_8x32x1& rhs)
    {
        vec_type temp{m_vec[4], m_vec[5], m_vec[6], m_vec[7], rhs.m_vec[4], rhs.m_vec[5], rhs.m_vec[6], rhs.m_vec[7]};
        rhs.m_vec = vec_type{m_vec[0], m_vec[1], m_vec[2], m_vec[3], rhs.m_vec[0], rhs.m_vec[1], rhs.m_vec[2], rhs.m_vec[3]};
        m_vec = temp;
    }

/** Endian-conversion for big-endian */
    ALWAYS_INLINE static constexpr void byteswap(vec_type& vec)
    {
        if constexpr (std::endian::native == std::endian::big)
        {
            vec[0] = __builtin_bswap32(vec[0]);
            vec[1] = __builtin_bswap32(vec[1]);
            vec[2] = __builtin_bswap32(vec[2]);
            vec[3] = __builtin_bswap32(vec[3]);
            vec[4] = __builtin_bswap32(vec[4]);
            vec[5] = __builtin_bswap32(vec[5]);
            vec[6] = __builtin_bswap32(vec[6]);
            vec[7] = __builtin_bswap32(vec[7]);
        }
    }
};

using vec256 = std::conditional<enable_256bit_operations, vec256_8x32x1, vec256_4x32x2>::type;

/** Store a vector in all array elements */
template <size_t I, size_t ITER = 0>
ALWAYS_INLINE void arr_set_vec256(std::array<vec256, I>& arr, const vec256& vec)
{
    std::get<ITER>(arr) = vec;
    if constexpr(ITER + 1 < I ) arr_set_vec256<I, ITER + 1>(arr, vec);
}

/** Add a vector to all array elements */
template <size_t I, size_t ITER = 0>
ALWAYS_INLINE void arr_add_vec256(std::array<vec256, I>& arr, const vec256& vec)
{
    std::get<ITER>(arr) += vec;
    if constexpr(ITER + 1 < I ) arr_add_vec256<I, ITER + 1>(arr, vec);
}

/** Add corresponding vectors in arr1 to arr0 */
template <size_t I, size_t ITER = 0>
ALWAYS_INLINE void arr_add_arr(std::array<vec256, I>& arr0, const std::array<vec256, I>& arr1)
{
    std::get<ITER>(arr0) += std::get<ITER>(arr1);
    if constexpr(ITER + 1 < I ) arr_add_arr<I, ITER + 1>(arr0, arr1);
}

/** Perform add/xor/rotate for the round function */
template <size_t BITS, size_t I, size_t ITER = 0>
ALWAYS_INLINE void arr_add_xor_rot(std::array<vec256, I>& arr0, const std::array<vec256, I>& arr1, std::array<vec256, I>& arr2)
{
    vec256& x = std::get<ITER>(arr0);
    const vec256& y = std::get<ITER>(arr1);
    vec256& z = std::get<ITER>(arr2);

    x += y;
    z ^= x;
    z.rotl<BITS>();

    if constexpr(ITER + 1 < I ) arr_add_xor_rot<BITS, I, ITER + 1>(arr0, arr1, arr2);
}

/*
The first round:
            QUARTERROUND( x0, x4, x8,x12);
            QUARTERROUND( x1, x5, x9,x13);
            QUARTERROUND( x2, x6,x10,x14);
            QUARTERROUND( x3, x7,x11,x15);

The second round:
            QUARTERROUND( x0, x5,x10,x15);
            QUARTERROUND( x1, x6,x11,x12);
            QUARTERROUND( x2, x7, x8,x13);
            QUARTERROUND( x3, x4, x9,x14);

After the first round, arr_shuf0, arr_shuf1, and arr_shuf2 are used to shuffle
the layout to prepare for the second round.

After the second round, they are used (in reverse) to restore the original
layout.

*/
template <size_t I, size_t ITER = 0>
ALWAYS_INLINE void arr_shuf0(std::array<vec256, I>& arr)
{
    vec256& x = std::get<ITER>(arr);
    x.shuf<1, 2, 3, 0>();
    if constexpr(ITER + 1 < I ) arr_shuf0<I, ITER + 1>(arr);
}

template <size_t I, size_t ITER = 0>
ALWAYS_INLINE void arr_shuf1(std::array<vec256, I>& arr)
{
    vec256& x = std::get<ITER>(arr);
    x.shuf<2, 3, 0, 1>();

    if constexpr(ITER + 1 < I ) arr_shuf1<I, ITER + 1>(arr);
}

template <size_t I, size_t ITER = 0>
ALWAYS_INLINE void arr_shuf2(std::array<vec256, I>& arr)
{
    vec256& x = std::get<ITER>(arr);
    x.shuf<3, 0, 1, 2>();

    if constexpr(ITER + 1 < I ) arr_shuf2<I, ITER + 1>(arr);
}

/* Main round function. */
template <size_t I, size_t ITER = 0>
ALWAYS_INLINE void doubleround(std::array<vec256, I>& arr0, std::array<vec256, I>& arr1, std::array<vec256, I>&arr2, std::array<vec256, I>&arr3)
{
    arr_add_xor_rot<16>(arr0, arr1, arr3);
    arr_add_xor_rot<12>(arr2, arr3, arr1);
    arr_add_xor_rot<8>(arr0, arr1, arr3);
    arr_add_xor_rot<7>(arr2, arr3, arr1);
    arr_shuf0(arr1);
    arr_shuf1(arr2);
    arr_shuf2(arr3);
    arr_add_xor_rot<16>(arr0, arr1, arr3);
    arr_add_xor_rot<12>(arr2, arr3, arr1);
    arr_add_xor_rot<8>(arr0, arr1, arr3);
    arr_add_xor_rot<7>(arr2, arr3, arr1);
    arr_shuf2(arr1);
    arr_shuf1(arr2);
    arr_shuf0(arr3);

    if constexpr (ITER + 1 < 10) doubleround<I, ITER + 1>(arr0, arr1, arr2, arr3);
}

/* Merge the 128 bit lanes from 2 states to the proper order, then pass each vec_xor_write */
template <size_t I, size_t ITER = 0>
ALWAYS_INLINE void arr_xor_write(std::span<uint32_t, (I - ITER) * 32> data, std::array<vec256, I>& arr0, std::array<vec256, I>& arr1, std::array<vec256, I>& arr2, std::array<vec256, I>& arr3)
{
    vec256& w = std::get<ITER>(arr0);
    vec256& x = std::get<ITER>(arr1);
    vec256& y = std::get<ITER>(arr2);
    vec256& z = std::get<ITER>(arr3);

    w.split(x);
    y.split(z);

    w.xor_write(data.template first<8>());
    y.xor_write(data.template subspan<8, 8>());
    x.xor_write(data.template subspan<16, 8>());
    z.xor_write(data.template subspan<24, 8>());

    if constexpr(ITER + 1 < I ) arr_xor_write<I, ITER + 1>(data.template subspan<32>(), arr0, arr1, arr2, arr3);
}

/* Compile-time helper to create addend vectors which used to increment the states

    Generates vectors of the pattern:
    1 0 0 0 0 0 0 0
    3 0 0 0 2 0 0 0
    5 0 0 0 4 0 0 0
    ...
*/
template <size_t SIZE>
consteval std::array<vec256, SIZE> generate_increments()
{
    std::array<vec256, SIZE> rows;
    for (uint32_t i = 0; i < SIZE; i ++)
    {
        rows[i] = {(2U * i) + 1U, 0, 0, 0, 2U * i, 0, 0, 0};
    }
    return rows;
}

/* Main crypt function. Calculates up to 16 states.

    Each array contains one or more vectors, with each array representing a
    quarter of a state. Initially, the high and low parts of each vector are
    duplicated. They each contain a portion of the current and next state.

    arr0[0]    arr1[0]    arr2[0]    arr3[0]   increment
    ----------|---------|----------|----------|---------
    0x61707865 input[0]   input[4]   input[8]   [1]
    0x3320646e input[1]   input[5]   input[9]   [0]
    0x79622d32 input[2]   input[6]   input[10]  [0]
    0x6b206574 input[3]   input[7]   input[11]  [0]

    0x61707865 input[0]   input[4]   input[8]   [0]
    0x3320646e input[1]   input[5]   input[9]   [0]
    0x79622d32 input[2]   input[6]   input[10]  [0]
    0x6b206574 input[3]   input[7]   input[11]  [0]

    After loading the states, arr3's vectors are incremented as-necessary to
    contain the correct counter values.

    This way, operations like "arr0[0] += arr1[0]" can perform all 8 operations
    in parallel, taking advantage of 256bit registers where available.

    arrX[0] represents states 0 and 1.
    arrX[1] represents states 2 and 3 (if present)
    etc.

    After the doublerounds have been run and the initial state has been mixed
    back in, the high and low portions of the vectors in each array are
    shuffled in order to prepare them for mixing with the input bytes. Finally,
    each state is xor'd with its corresponding input, byteswapped if necessary,
    and written to its output.
*/
template <size_t STATES>
ALWAYS_INLINE void multi_block_crypt(std::span<uint32_t, 16 * STATES> data, const vec256& state0, const vec256& state1, const vec256& state2)
{
    static constexpr size_t HALF_STATES = STATES / 2;
    static constexpr vec256 nums256 = (vec256){0x61707865, 0x3320646e, 0x79622d32, 0x6b206574, 0x61707865, 0x3320646e, 0x79622d32, 0x6b206574};
    static constinit std::array<vec256, HALF_STATES> increments = generate_increments<HALF_STATES>();

    std::array<vec256, HALF_STATES> arr0, arr1, arr2, arr3;

    arr_set_vec256(arr0, nums256);
    arr_set_vec256(arr1, state0);
    arr_set_vec256(arr2, state1);
    arr_set_vec256(arr3, state2);

    arr_add_arr(arr3, increments);

    doubleround(arr0, arr1, arr2, arr3);

    arr_add_vec256(arr0, nums256);
    arr_add_vec256(arr1, state0);
    arr_add_vec256(arr2, state1);
    arr_add_vec256(arr3, state2);

    arr_add_arr(arr3, increments);

    arr_xor_write(data, arr0, arr1, arr2, arr3);
}

} // anonymous namespace
#endif // CHACHA20_VEC_ALL_MULTI_STATES_DISABLED

#if defined(CHACHA20_NAMESPACE)
namespace CHACHA20_NAMESPACE {
#endif

void chacha20_crypt_vectorized(std::span<const std::byte>& in_bytes, std::span<std::byte>& out_bytes, const std::array<uint32_t, 12>& input) noexcept
{
#if !defined(CHACHA20_VEC_ALL_MULTI_STATES_DISABLED)
    assert(in_bytes.size() == out_bytes.size());
    const vec256 state0 =  (vec256){input[0], input[1], input[2], input[3], input[0], input[1], input[2], input[3]};
    const vec256 state1 =  (vec256){input[4], input[5], input[6], input[7], input[4], input[5], input[6], input[7]};
    vec256 state2 =  (vec256){input[8], input[9], input[10], input[11], input[8], input[9], input[10], input[11]};
    std::array<uint32_t, 16 * 16> out_uints;
#if !defined(CHACHA20_VEC_DISABLE_STATES_16)
    while(in_bytes.size() >= CHACHA20_VEC_BLOCKLEN * 16) {
        auto out = std::span(out_uints).first<16 * 16>();
        memcpy(out.data(), in_bytes.data(), CHACHA20_VEC_BLOCKLEN * 16);
        multi_block_crypt<16>(out, state0, state1, state2);
        memcpy(out_bytes.data(), out.data(), CHACHA20_VEC_BLOCKLEN * 16);
        in_bytes = in_bytes.subspan(CHACHA20_VEC_BLOCKLEN * 16);
        out_bytes = out_bytes.subspan(CHACHA20_VEC_BLOCKLEN * 16);
        state2 += (vec256){16, 0, 0, 0, 16, 0, 0, 0};
    }
#endif
#if !defined(CHACHA20_VEC_DISABLE_STATES_8)
    while(in_bytes.size() >= CHACHA20_VEC_BLOCKLEN * 8) {
        auto out = std::span(out_uints).first<16 * 8>();
        memcpy(out.data(), in_bytes.data(), CHACHA20_VEC_BLOCKLEN * 8);
        multi_block_crypt<8>(out, state0, state1, state2);
        memcpy(out_bytes.data(), out.data(), CHACHA20_VEC_BLOCKLEN * 8);
        in_bytes = in_bytes.subspan(CHACHA20_VEC_BLOCKLEN * 8);
        out_bytes = out_bytes.subspan(CHACHA20_VEC_BLOCKLEN * 8);
        state2 += (vec256){8, 0, 0, 0, 8, 0, 0, 0};
    }
#endif
#if !defined(CHACHA20_VEC_DISABLE_STATES_6)
    while(in_bytes.size() >= CHACHA20_VEC_BLOCKLEN * 6) {
        auto out = std::span(out_uints).first<16 * 6>();
        memcpy(out.data(), in_bytes.data(), CHACHA20_VEC_BLOCKLEN * 6);
        multi_block_crypt<6>(out, state0, state1, state2);
        memcpy(out_bytes.data(), out.data(), CHACHA20_VEC_BLOCKLEN * 6);
        in_bytes = in_bytes.subspan(CHACHA20_VEC_BLOCKLEN * 6);
        out_bytes = out_bytes.subspan(CHACHA20_VEC_BLOCKLEN * 6);
        state2 += (vec256){6, 0, 0, 0, 6, 0, 0, 0};
    }
#endif
#if !defined(CHACHA20_VEC_DISABLE_STATES_4)
    while(in_bytes.size() >= CHACHA20_VEC_BLOCKLEN * 4) {
        auto out = std::span(out_uints).first<16 * 4>();
        memcpy(out.data(), in_bytes.data(), CHACHA20_VEC_BLOCKLEN * 4);
        multi_block_crypt<4>(out, state0, state1, state2);
        memcpy(out_bytes.data(), out.data(), CHACHA20_VEC_BLOCKLEN * 4);
        in_bytes = in_bytes.subspan(CHACHA20_VEC_BLOCKLEN * 4);
        out_bytes = out_bytes.subspan(CHACHA20_VEC_BLOCKLEN * 4);
        state2 += (vec256){4, 0, 0, 0, 4, 0, 0, 0};
    }
#endif
#if !defined(CHACHA20_VEC_DISABLE_STATES_2)
    while(in_bytes.size() >= CHACHA20_VEC_BLOCKLEN * 2) {
        auto out = std::span(out_uints).first<16 * 2>();
        memcpy(out.data(), in_bytes.data(), CHACHA20_VEC_BLOCKLEN * 2);
        multi_block_crypt<2>(out, state0, state1, state2);
        memcpy(out_bytes.data(), out.data(), CHACHA20_VEC_BLOCKLEN * 2);
        in_bytes = in_bytes.subspan(CHACHA20_VEC_BLOCKLEN * 2);
        out_bytes = out_bytes.subspan(CHACHA20_VEC_BLOCKLEN * 2);
        state2 += (vec256){2, 0, 0, 0, 2, 0, 0, 0};
    }
#endif
#endif // CHACHA20_VEC_ALL_MULTI_STATES_DISABLED
}

#if defined(CHACHA20_NAMESPACE)
}
#endif

#endif // ENABLE_CHACHA20_VEC
