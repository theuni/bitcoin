// Copyright (c) 2024 Cory Fields
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TMI_HASHER_H_
#define TMI_HASHER_H_

#include "tmi_nodehandle.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstring>
#include <cstddef>
#include <iterator>
#include <limits>
#include <tuple>
#include <utility>
namespace tmi {

template <typename T, typename Node, typename Hasher, typename Parent, typename Allocator, int I>
class tmi_hasher
{
public:
    class iterator;

    using node_type = Node;
    using base_type = typename node_type::base_type;
    using size_type = size_t;
    using key_from_value = typename Hasher::key_from_value_type;
    using key_type = typename key_from_value::result_type;
    using hasher = typename Hasher::hasher_type;
    using key_equal = typename Hasher::pred_type;
    using ctor_args = std::tuple<size_type,key_from_value,hasher,key_equal>;
    using allocator_type = Allocator;
    using node_allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<node_type>;
    using node_handle = detail::node_handle<Allocator, Node>;
    using insert_return_type = detail::insert_return_type<iterator, node_handle>;
    friend Parent;

private:
    static constexpr bool hashed_unique() { return Hasher::is_hashed_unique(); }

    struct insert_hints {
        size_t m_hash{0};
        base_type** m_bucket{nullptr};
    };

    struct premodify_cache {
        base_type** m_bucket{nullptr};
        base_type* m_prev{nullptr};
    };

    class hash_buckets
    {
        using bucket_allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<base_type*>;
        bucket_allocator_type m_alloc;
        base_type** m_buckets{nullptr};
        size_t m_bucket_count{0};
        size_t m_capacity{0};

        struct allocation_result
        {
            typename std::allocator_traits<bucket_allocator_type>::pointer ptr;
            typename std::allocator_traits<bucket_allocator_type>::size_type count;
        };

        public:
        hash_buckets(const bucket_allocator_type& alloc) : m_alloc(alloc) {}

        hash_buckets(hash_buckets&& rhs) : m_alloc(std::move(rhs.m_alloc)), m_buckets(rhs.m_buckets), m_bucket_count(rhs.m_bucket_count), m_capacity(rhs.m_capacity)
        {
            rhs.m_buckets = nullptr;
            rhs.m_capacity = 0;
            rhs.m_bucket_count = 0;
        }
        hash_buckets(const hash_buckets& rhs) : m_alloc(std::allocator_traits<bucket_allocator_type>::select_on_container_copy_construction(rhs.m_alloc))
        {
            init(rhs.m_bucket_count);
        }
        hash_buckets(const bucket_allocator_type& alloc, size_t size) : m_alloc(alloc)
        {
            init(size);
        }
        ~hash_buckets()
        {
            do_deallocate(m_buckets, m_capacity);
        }
        void init(size_t requested_size)
        {
            assert(!m_bucket_count);
            size_t new_bucket_count = std::bit_ceil(requested_size);
            if (m_capacity < new_bucket_count) {
                do_deallocate(m_buckets, m_capacity);
                m_buckets = do_allocate(new_bucket_count);
                m_capacity = new_bucket_count;
            }
            m_bucket_count = new_bucket_count;
        }
        void rehash(size_t size)
        {
            size_t new_bucket_count = std::bit_ceil(size);
            if (m_capacity >= new_bucket_count && new_bucket_count > m_bucket_count) {
                do_rehash_inplace(new_bucket_count);
            } else {
                do_rehash_copy(new_bucket_count);
            }
        }
        void clear()
        {
            std::memset(m_buckets, 0, m_bucket_count);
            m_bucket_count = 0;
        }
        size_t size() const
        {
            return m_bucket_count;
        }
        bool empty() const
        {
            return m_bucket_count == 0;
        }
        const base_type* const* begin() const
        {
            return &m_buckets[0];
        }
        const base_type* const* end() const
        {
            return &m_buckets[0] + m_bucket_count;
        }
        base_type** begin()
        {
            return &m_buckets[0];
        }
        base_type** end()
        {
            return &m_buckets[0] + m_bucket_count;
        }
        const base_type* const& at(size_t index) const
        {
            return m_buckets[index];
        }
        base_type*& at(size_t index)
        {
            return m_buckets[index];
        }
        size_type bucket_count() const
        {
            return m_bucket_count;
        }
        size_type max_bucket_count() const
        {
            return std::numeric_limits<size_t>::max();
        }
        size_type bucket_size(size_type bucket) const
        {
            size_t ret = 0;
            base_type* node = m_buckets[bucket];
            while(node) {
                node = node->template next_hash<I>();
                ret++;
            }
            return ret;
        }

        private:

        void do_rehash_copy(size_t new_bucket_count)
        {
            base_type** new_buckets = do_allocate(new_bucket_count);
            base_type** old_buckets = m_buckets;
            size_t old_capacity = m_capacity;
            for(size_t i = 0; i < m_bucket_count; i++) {
                base_type* cur_node = old_buckets[i];
                while (cur_node) {
                    base_type* next_node = cur_node->template next_hash<I>();
                    const size_t index = cur_node->template hash<I>() % new_bucket_count;
                    base_type*& new_bucket = new_buckets[index];
                    cur_node->template set_next_hashptr<I>(new_bucket);
                    new_bucket = cur_node;
                    cur_node = next_node;
                }
            }
            m_buckets = new_buckets;
            m_bucket_count = new_bucket_count;
            m_capacity = new_bucket_count;
            do_deallocate(old_buckets, old_capacity);
        }

        void do_rehash_inplace(size_t new_bucket_count)
        {
            for(size_t i = 0; i < m_bucket_count; i++) {
                base_type* cur_node = m_buckets[i];
                base_type* prev_node = nullptr;
                while (cur_node) {
                    base_type* next_node = cur_node->template next_hash<I>();
                    const size_t index = cur_node->template hash<I>() % new_bucket_count;
                    base_type*& new_bucket = m_buckets[index];
                    if (index != i) {
                        if (prev_node == nullptr) {
                            m_buckets[i] = cur_node->template next_hash<I>();
                        } else {
                            prev_node->template set_next_hashptr<I>(cur_node->template next_hash<I>());
                        }
                        cur_node->template set_next_hashptr<I>(new_bucket);
                        new_bucket = cur_node;
                    }
                    prev_node = cur_node;
                    cur_node = next_node;
                }
            }
            m_bucket_count = new_bucket_count;
        }
        base_type** do_allocate(size_t new_capacity)
        {
            if (!new_capacity)
            {
                return nullptr;
            }
            base_type** ret = std::allocator_traits<bucket_allocator_type>::allocate(m_alloc, new_capacity);
            std::memset(ret, 0, new_capacity * sizeof(*ret));
            return ret;
        }
        void do_deallocate(base_type** buckets, size_t capacity)
        {
            if (capacity) {
                std::allocator_traits<bucket_allocator_type>::deallocate(m_alloc, buckets, capacity);
            }
        }


    };

    static constexpr bool requires_premodify_cache() { return true; }

    static constexpr size_t first_hashes_resize = 2048;

    Parent& m_parent;
    hash_buckets m_buckets;
    key_from_value m_key_from_value;
    hasher m_hasher;
    key_equal m_pred;

    tmi_hasher(Parent& parent, const allocator_type& alloc) : m_parent(parent), m_buckets(alloc) {}

    tmi_hasher(Parent& parent, const allocator_type& alloc, const ctor_args& args) : m_parent(parent), m_buckets(alloc, std::get<0>(args)), m_key_from_value(std::get<1>(args)), m_hasher(std::get<2>(args)), m_pred(std::get<3>(args)){}

    tmi_hasher(Parent& parent, const tmi_hasher& rhs) : m_parent(parent), m_buckets(rhs.m_buckets.size(), nullptr), m_key_from_value(rhs.m_key_from_value), m_hasher(rhs.m_hasher), m_pred(rhs.m_pred){}
    tmi_hasher(Parent& parent, tmi_hasher&& rhs) : m_parent(parent), m_buckets(std::move(rhs.m_buckets)), m_key_from_value(std::move(rhs.m_key_from_value)), m_hasher(std::move(rhs.m_hasher)), m_pred(std::move(rhs.m_pred))
    {
        rhs.m_buckets.clear();
    }

    static base_type* get_next_hash(base_type* base)
    {
        return base->template next_hash<I>();
    }

    static size_t get_hash(base_type* base)
    {
        return base->template hash<I>();
    }

    static void set_hash(base_type* base, size_t hash)
    {
        base->template set_hash<I>(hash);
    }

    static void set_next_hashptr(base_type* lhs, base_type* rhs)
    {
        lhs->template set_next_hashptr<I>(rhs);
    }

    void remove_node(const node_type* node)
    {
        const base_type* base = node->get_base();
        const size_t bucket_count = m_buckets.size();
        if (!bucket_count) {
            return;
        }
        const size_t index = base->template hash<I>() % bucket_count;

        base_type*& bucket = m_buckets.at(index);
        base_type* cur_node = bucket;
        base_type* prev_node = cur_node;
        while (cur_node) {
            if (cur_node->node() == node) {
                if (cur_node == prev_node) {
                    // head of list
                    bucket = cur_node->template next_hash<I>();
                } else {
                    prev_node->template set_next_hashptr<I>(cur_node->template next_hash<I>());
                }
                break;
            }
            prev_node = cur_node;
            cur_node = cur_node->template next_hash<I>();
        }
    }

    void insert_node_direct(node_type* node)
    {
        base_type* base = node->get_base();

        if (base->template hash<I>() == 0) {
            const size_t hash = m_hasher(m_key_from_value(node->value()));
            base->template set_hash<I>(hash);
        }
        const size_t index = base->template hash<I>() % m_buckets.size();
        base_type*& bucket = m_buckets.at(index);

        base->template set_next_hashptr<I>(bucket);
        bucket = base;
    }

    /*
        First rehash if necessary, using first_hashes_resize as the initial
        size if empty. Then find calculate the bucket and insert there.
    */
    node_type* preinsert_node(const node_type* node, insert_hints& hints)
    {
        size_t bucket_count = m_buckets.size();
        const auto& key = m_key_from_value(node->value());
        const size_t hash = m_hasher(key);

        if (!bucket_count) {
            m_buckets.init(first_hashes_resize);
            bucket_count = first_hashes_resize;
        } else if (static_cast<double>(m_parent.get_size()) / static_cast<double>(bucket_count) >= 0.8) {
            bucket_count *= 2;
            m_buckets.rehash(bucket_count);
        }

        const size_t index = hash % m_buckets.size();
        base_type*& bucket = m_buckets.at(index);

        if constexpr (hashed_unique()) {
            base_type* curr = bucket;
            base_type* prev = curr;
            while (curr) {
                if (curr->template hash<I>() == hash) {
                    if (m_pred(m_key_from_value(curr->node()->value()), key)) {
                        return curr->node();
                    }
                }
                prev = curr;
                curr = curr->template next_hash<I>();
            }
        }
        hints.m_bucket = &bucket;
        hints.m_hash = hash;
        return nullptr;
    }

    void create_premodify_cache(const node_type* node, premodify_cache& cache)
    {
        const base_type* base = node->get_base();
        const size_t bucket_count = m_buckets.size();
        if (!bucket_count) {
            return;
        }
        const size_t index = base->template hash<I>() % bucket_count;

        base_type*& bucket = m_buckets.at(index);
        base_type* cur_node = bucket;
        base_type* prev_node = cur_node;
        while (cur_node) {
            if (cur_node->node() == node) {
                if (cur_node == prev_node) {
                    cache.m_prev = nullptr;
                    cache.m_bucket = &bucket;
                } else {
                    cache.m_prev = prev_node;
                    cache.m_bucket = nullptr;
                }
                break;
            }
            prev_node = cur_node;
            cur_node = cur_node->template next_hash<I>();
        }
    }

    bool erase_if_modified(const node_type* node, const premodify_cache& cache)
    {
        const base_type* base = node->get_base();
        if (m_hasher(m_key_from_value(node->value())) != base->template hash<I>()) {
            if (cache.m_prev) {
                cache.m_prev->template set_next_hashptr<I>(base->template next_hash<I>());
            } else {
                *cache.m_bucket = base->template next_hash<I>();
            }
            return true;
        }
        return false;
    }

    void insert_node(node_type* node, const insert_hints& hints)
    {
        base_type* node_base = node->get_base();
        node_base->template set_hash<I>(hints.m_hash);
        node_base->template set_next_hashptr<I>(*hints.m_bucket);
        *hints.m_bucket = node_base;
    }


    node_type* find_hash(const key_type& hash_key) const
    {
        const size_t hash = m_hasher(hash_key);
        const size_t bucket_count = m_buckets.size();
        if (!bucket_count) {
            return nullptr;
        }
        auto* node = m_buckets.at(hash % bucket_count);
        while (node) {
            if (node->template hash<I>() == hash) {
                if (m_pred(m_key_from_value(node->node()->value()), hash_key)) {
                    return node->node();
                }
            }
            node = node->template next_hash<I>();
        }
        return nullptr;
    }

    void do_clear()
    {
        m_buckets.clear();
    }

public:

    class iterator
    {
        const node_type* m_node{};
        const hash_buckets* m_buckets{nullptr};

        iterator(const node_type* node, const hash_buckets* buckets) : m_node(node), m_buckets(buckets) {}
        friend class tmi_hasher;
    public:

        typedef const T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        using difference_type = std::ptrdiff_t;
        using element_type = const T;
        using iterator_category = std::forward_iterator_tag;
        iterator() = default;
        const T& operator*() const { return m_node->value(); }
        const T* operator->() const { return &m_node->value(); }
        iterator& operator++()
        {
            const base_type* next = m_node->get_base()->template next_hash<I>();
            if (!next) {
                const size_t bucket_size = m_buckets->size();
                size_t bucket = m_node->get_base()->template hash<I>() % bucket_size;
                bucket++;
                for (; bucket < bucket_size; ++bucket) {
                    next = m_buckets->at(bucket);
                    if (next) {
                        break;
                    }
                }
            }
            if (next == nullptr) {
                m_node = nullptr;
            } else {
                m_node = next->node();
            }
            return *this;
        }
        iterator operator++(int)
        {
            iterator copy(m_node, m_buckets);
            ++(*this);
            return copy;
        }
        bool operator==(iterator rhs) const { return m_node == rhs.m_node; }
        bool operator!=(iterator rhs) const { return m_node != rhs.m_node; }
    }
;
    using const_iterator = iterator;

    iterator begin()
    {
        for (const auto& bucket : m_buckets) {
            if (bucket) {
                return make_iterator(bucket->node());
            }
        }
        return end();
    }

    const_iterator begin() const
    {
        for (const auto& bucket : m_buckets) {
            if (bucket) {
                return make_iterator(bucket->node());
            }
        }
        return end();
    }

    iterator end()
    {
        return make_iterator(nullptr);
    }

    const_iterator end() const
    {
        return make_iterator(nullptr);
    }

    iterator iterator_to(const T& entry)
    {
        const node_type* node = &node_type::node_cast(entry);
        return make_iterator(node);
    }

    const_iterator iterator_to(const T& entry) const
    {
        const node_type* node = &node_type::node_cast(entry);
        return make_iterator(node);
    }

    template <typename... Args>
    std::pair<iterator,bool> emplace(Args&&... args)
    {
        auto [node, success] = m_parent.do_emplace(std::forward<Args>(args)...);
        return std::make_pair(make_iterator(node), success);
    }

    template <typename Callable>
    bool modify(iterator it, Callable&& func)
    {
        node_type* node = const_cast<node_type*>(it.m_node);
        if (!node) return false;
        return m_parent.do_modify(node, std::forward<Callable>(func));
    }

    template <typename CompatibleKey>
    iterator find(const CompatibleKey& key) const
    {
        const size_t hash = m_hasher(key);
        const size_t bucket_count = m_buckets.size();
        if (!bucket_count) {
            return end();
        }
        auto* node = m_buckets.at(hash % bucket_count);
        while (node) {
            if (node->template hash<I>() == hash) {
                if (m_pred(m_key_from_value(node->node()->value()), key)) {
                    return make_iterator(node->node());
                }
            }
            node = node->template next_hash<I>();
        }
        return end();
    }

    iterator erase(iterator it)
    {
        node_type* node = const_cast<node_type*>(it++.m_node);
        m_parent.do_erase(node);
        return it;
    }

    template <typename CompatibleKey>
    size_t count(const CompatibleKey& key) const
    {
        size_t ret = 0;
        const size_t hash = m_hasher(key);
        const size_t bucket_count = m_buckets.size();
        if (!bucket_count) {
            return 0;
        }
        auto* node = m_buckets.at(hash % bucket_count);
        while (node) {
            if (node->template hash<I>() == hash) {
                if (m_pred(m_key_from_value(node->node()->value()), key)) {
                    ret++;
                    if constexpr (hashed_unique()) break;
                }
            }
            node = node->template next_hash<I>();
        }
        return ret;
    }

    void clear()
    {
        m_parent.do_clear();
    }

    size_t size() const
    {
        return m_parent.get_size();
    }

    bool empty() const
    {
        return m_parent.get_empty();
    }

    insert_return_type insert(node_handle&& handle)
    {
        node_type* node = handle.m_node;
        if(!node) {
            return {end(), false, {}};
        }
        node_type* conflict = m_parent.do_insert(node);
        if (conflict) {
            return {make_iterator(conflict), false, std::move(handle)};
        }
        handle.m_node = nullptr;
        return {make_iterator(node), true, {}};
    }

    node_handle extract(const_iterator it)
    {
        return m_parent.do_extract(const_cast<node_type*>(it.m_node));
    }

    allocator_type get_allocator() const noexcept
    {
        return m_parent.get_allocator();
    }
private:

    const node_type* node_from_iterator(iterator it) const
    {
        return it.m_node;
    }

    iterator make_iterator(const node_type* node) const
    {
        return iterator(node, &m_buckets);
    }
};

} // namespace tmi

#endif // TMI_HASHER_H_
