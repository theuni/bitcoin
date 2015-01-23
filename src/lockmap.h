// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_LOCKMAP_H
#define BITCOIN_LOCKMAP_H

#include <map>
#include <memory>
#include <utility>

#include <boost/thread/mutex.hpp>
#include <boost/noncopyable.hpp>

#define MAP_LOCK(MAPNAME) maplock_t maplock(MAPNAME.get_lock());

typedef boost::unique_lock<boost::mutex> maplock_t;

class lock_base : boost::noncopyable
{
protected:
    lock_base(){}
public:
    inline maplock_t get_lock()
    {
        return maplock_t(m_mutex);
    }

private:
    boost::mutex m_mutex;
};

template<typename Key, typename T, typename Compare = std::less<Key>, typename Alloc = std::allocator<std::pair<const Key,T> > >
class CLockMap : public lock_base
{
protected:
    typedef std::map<Key, T, Compare, Alloc> maptype;
public:

    typedef typename maptype::key_type key_type;
    typedef typename maptype::mapped_type mapped_type;
    typedef typename maptype::value_type value_type;
    typedef typename maptype::key_compare key_compare;
    typedef typename maptype::allocator_type allocator_type;
    typedef typename maptype::value_compare value_compare;
    typedef typename maptype::reference reference;
    typedef typename maptype::const_reference const_reference;
    typedef typename maptype::pointer pointer;
    typedef typename maptype::iterator iterator;
    typedef typename maptype::reverse_iterator reverse_iterator;
    typedef typename maptype::const_reverse_iterator const_reverse_iterator;
    typedef typename maptype::const_iterator const_iterator;
    typedef typename maptype::difference_type difference_type;
    typedef typename maptype::size_type size_type;

    explicit CLockMap(const key_compare& comp = key_compare(),
                            const allocator_type& alloc = allocator_type())
    : m_map(comp, alloc){}

    inline virtual ~CLockMap()
    {
    }

    inline value_compare value_comp() const
    {
        return m_map.value_comp();
    }

    inline key_compare key_comp() const
    {
        return m_map.key_comp();
    }

    inline iterator begin()
    {
        return m_map.begin();
    }

    inline const_iterator begin() const
    {
        return m_map.begin();
    }

    inline reverse_iterator rbegin()
    {
        return m_map.rbegin();
    }

    inline const_reverse_iterator rbegin() const
    {
        return m_map.rbegin();
    }

    inline iterator end()
    {
        return m_map.end();
    }

    inline const_iterator end() const
    {
        return m_map.end();
    }

    inline reverse_iterator rend()
    {
        return m_map.rend();
    }

    inline const_reverse_iterator rend() const
    {
        return m_map.rend();
    }

    inline bool empty() const
    {
        return m_map.empty();
    }

    inline size_type size() const
    {
        return m_map.size();
    }

    inline size_type max_size() const
    {
        return m_map.max_size();
    }

    inline mapped_type& operator[] (const key_type& k)
    {
        return m_map.operator[](k);
    }

    inline std::pair<iterator,bool> insert (const value_type& val)
    {
        return m_map.insert(val);
    }

    inline iterator insert (iterator position, const value_type& val)
    {
        return m_map.insert(position, val);
    }

    template <class InputIterator>
    inline void insert (InputIterator first, InputIterator last)
    {
        m_map.insert(first, last);
    }

    inline void erase (iterator position)
    {
        m_map.erase(position);
    }

    inline size_type erase (const key_type& k)
    {
        return m_map.erase(k);
    }

    inline void swap (maptype& x)
    {
        m_map.swap(x);
    }

    inline void clear()
    {
        m_map.clear();
    }

    inline iterator find (const key_type& k)
    {
        return m_map.find(k);
    }

    inline const_iterator find (const key_type& k) const
    {
        return m_map.find(k);
    }

    inline size_type count (const key_type& k) const
    {
        return m_map.count(k);
    }
private: 
    maptype m_map;
};

#endif
