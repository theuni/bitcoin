// Copyright (c) 2025-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_OPAQUEHANDLE_H
#define BITCOIN_OPAQUEHANDLE_H

#include <memory>
#include <type_traits>
#include <utility>

template <typename T>
struct OpaqueHandleTag {
    using tag_type = T;

protected:
    consteval OpaqueHandleTag() noexcept = default;
};

template <typename Tag, typename IdType>
class OpaqueHandle
{
    std::weak_ptr<Tag> m_ptr{};
    IdType m_id{};

    template <typename Underlying, typename Tag2, typename IdType2>
    friend std::shared_ptr<Underlying> handle_cast(const OpaqueHandle<Tag2, IdType2>&) noexcept;

    template <typename Underlying, typename Tag2, typename IdType2>
    friend std::shared_ptr<Underlying> handle_cast(OpaqueHandle<Tag2, IdType2>&&) noexcept;

    template <typename Tag2, typename IdType2>
    friend class OpaqueHandle;

    template <typename Underlying>
    std::shared_ptr<Underlying> lock() const noexcept
    {
        static_assert(std::is_same_v<typename Tag::tag_type, typename Underlying::tag_type>, "tag type mismatch");
        return std::static_pointer_cast<Underlying>(m_ptr.lock());
    }

public:
    OpaqueHandle() noexcept = default;

    template <typename Underlying>
    OpaqueHandle(const std::shared_ptr<Underlying>& ptr, const IdType& id) noexcept : m_ptr{ptr}, m_id{id}
    {
        static_assert(std::is_base_of_v<OpaqueHandleTag<std::remove_cv_t<Tag>>, std::remove_cv_t<Tag>>, "tag not derived from OpaqueHandleTag");
        static_assert(std::is_base_of_v<Tag, Underlying>, "shared_ptr type not derived from tag");
        static_assert(std::is_same_v<typename Tag::tag_type, typename Underlying::tag_type>, "tag type mismatch");
    }

    template <typename Tag2, typename IdType2>
    OpaqueHandle(const OpaqueHandle<Tag2, IdType2>& rhs) noexcept : m_ptr{rhs.m_ptr}, m_id{rhs.m_id}
    {
    }

    template <typename Tag2, typename IdType2>
    OpaqueHandle(OpaqueHandle<Tag2, IdType2>&& rhs) noexcept : m_ptr{std::move(rhs.m_ptr)}, m_id{std::move(rhs.m_id)}
    {
    }

    template <typename Tag2, typename IdType2>
    OpaqueHandle& operator=(const OpaqueHandle<Tag2, IdType2>& rhs) noexcept
    {
        m_ptr = rhs.m_ptr;
        m_id = rhs.m_id;
        return *this;
    }

    template <typename Tag2, typename IdType2>
    OpaqueHandle& operator=(OpaqueHandle<Tag2, IdType2>&& rhs) noexcept
    {
        m_ptr = std::move(rhs.m_ptr);
        m_id = std::move(rhs.m_id);
        return *this;
    }

    [[nodiscard]] const IdType& id() const noexcept
    {
        return m_id;
    }

    [[nodiscard]] bool expired() const noexcept
    {
        return m_ptr.expired();
    }

    template <typename Tag2, typename IdType2>
    [[nodiscard]] auto operator<=>(const OpaqueHandle<Tag2, IdType2>& rhs) const noexcept
    {
        static_assert(std::is_same_v<typename Tag::tag_type, typename Tag2::tag_type>, "tag type mismatch");
        return m_id <=> rhs.m_id;
    }

    template <typename Tag2, typename IdType2>
    [[nodiscard]] bool operator==(const OpaqueHandle<Tag2, IdType2>& rhs) const noexcept
    {
        static_assert(std::is_same_v<typename Tag::tag_type, typename Tag2::tag_type>, "tag type mismatch");
        return m_id == rhs.m_id;
    }
};

// Deduction guides
template <typename Underlying, typename IdType>
OpaqueHandle(const std::shared_ptr<Underlying>&, const IdType&) -> OpaqueHandle<typename Underlying::tag_type, IdType>;

template <typename Underlying, typename IdType>
OpaqueHandle(const std::shared_ptr<const Underlying>&, const IdType&) -> OpaqueHandle<const typename Underlying::tag_type, IdType>;

template <typename Underlying, typename Tag, typename IdType>
[[nodiscard]] std::shared_ptr<Underlying> handle_cast(const OpaqueHandle<Tag, IdType>& handle) noexcept
{
    return handle.template lock<Underlying>();
}

template <typename Underlying, typename Tag, typename IdType>
[[nodiscard]] std::shared_ptr<Underlying> handle_cast(OpaqueHandle<Tag, IdType>&& handle) noexcept
{
    return OpaqueHandle<Tag, IdType>{std::move(handle)}.template lock<Underlying>();
}

template <typename Tag, typename IdType>
struct std::hash<OpaqueHandle<Tag, IdType>> {
    std::size_t operator()(const OpaqueHandle<Tag, IdType>& handle) const noexcept
    {
        return std::hash<IdType>{}(handle.id());
    }
};

#endif // BITCOIN_OPAQUEHANDLE_H
