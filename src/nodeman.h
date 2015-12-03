// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_THREADEDNODEMAN_H
#define BITCOIN_THREADEDNODEMAN_H

#include "net.h"
#include "libbtcnet/connection.h"
#include "protocol.h"

#ifdef USE_CXX11
#include <mutex>
#include <condition_variable>
#include <thread>
typedef std::mutex base_mutex_type;
typedef std::lock_guard<base_mutex_type> lock_guard_type;
typedef std::unique_lock<base_mutex_type> unique_lock_type;
typedef std::condition_variable condition_variable_type;
typedef std::thread thread_type;
typedef std::function<void(uint64_t, CNode*, bool)> function_type;
#else
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/thread.hpp>
#include <boost/function.hpp>
typedef boost::mutex base_mutex_type;
typedef boost::lock_guard<base_mutex_type> lock_guard_type;
typedef boost::unique_lock<base_mutex_type> unique_lock_type;
typedef boost::condition_variable condition_variable_type;
typedef boost::thread thread_type;
typedef boost::function<void(uint64_t, CNode*, bool)> function_type;
#endif

#include <map>
#include <assert.h>

class CThreadedNodeManager
{
    template <typename>
    struct CNodeHolder
    {
        CNodeHolder(uint64_t id, const CAddress& addr, const std::string& addrName, bool incoming, bool whitelist, bool oneshot)
        : m_node(id, addr, addrName, incoming), m_refcount(0), m_remove(false), m_disconnected(false)
        {
            m_node.fOneShot = oneshot;
            m_node.fWhitelisted = whitelist;
        };
        CNode m_node;
        int m_refcount;
        bool m_remove;
        bool m_disconnected;
    };
public:
    typedef CNodeHolder<CNode> HolderType;
    typedef std::map<uint64_t, HolderType*> NodeMapType;
    typedef typename NodeMapType::iterator NodeMapIter;

    CThreadedNodeManager() : m_shutdown(false)
    {
    }
    ~CThreadedNodeManager()
    {
    }

    bool AddNode(uint64_t id, const CAddress& addr, const std::string& addrName, bool incoming, bool whitelist, bool oneshot)
    {
        lock_guard_type lock(m_nodes_mutex);
        if(m_shutdown)
            return false;
        const std::pair<uint64_t, HolderType*> node(id, new HolderType(id, addr, addrName, incoming, whitelist, oneshot));
        node.second->m_node.fNetworkNode = true;
        GetNodeSignals().InitializeNode(id, &node.second->m_node);
        return m_nodes.insert(node).second;
    }

    CNode* GetNode(uint64_t id)
    {
        lock_guard_type lock(m_nodes_mutex);
        if(m_shutdown)
            return NULL;
        NodeMapIter it = m_nodes.find(id);
        if(it != m_nodes.end() && !it->second->m_remove)
        {
            HolderType* holder = it->second;
            holder->m_refcount++;
            return &holder->m_node;
        }
        return NULL;
    }

    void Remove(uint64_t id)
    {
        lock_guard_type lock(m_nodes_mutex);
        NodeMapIter it = m_nodes.find(id);
        if(it != m_nodes.end())
        {
            HolderType* holder = it->second;
            assert(!holder->m_remove);
            holder->m_remove = true;
            if(!holder->m_refcount)
            {
                if(!m_shutdown)
                    GetNodeSignals().FinalizeNode(id);
                delete holder;
                m_nodes.erase(it);
            }
        }
    }

    void SetDisconnected(uint64_t id)
    {
        lock_guard_type lock(m_nodes_mutex);
        NodeMapIter it = m_nodes.find(id);
        if(it != m_nodes.end())
        {
            HolderType* holder = it->second;
            assert(!holder->m_disconnected);
            holder->m_disconnected = true;
        }
    }

    int AddRef(uint64_t id)
    {
        lock_guard_type lock(m_nodes_mutex);
        NodeMapIter it = m_nodes.find(id);
        if(it != m_nodes.end())
        {
            HolderType* holder = it->second;
            if(!m_shutdown)
                holder->m_refcount++;
            return holder->m_refcount;
        }
        return 0;
    }

    int Release(uint64_t id)
    {
        lock_guard_type lock(m_nodes_mutex);
        NodeMapIter it = m_nodes.find(id);
        if(it == m_nodes.end())
            return 0;
        HolderType* holder = it->second;
        int refcount = holder->m_refcount--;
        if(!refcount && (holder->m_remove == true))
        {
            if(!m_shutdown)
                GetNodeSignals().FinalizeNode(id);
            delete holder;
            m_nodes.erase(it);
        }
        return refcount;
    }

    void ForEach(const function_type& func)
    {
        std::set<uint64_t> nodes;
        {
            lock_guard_type lock(m_nodes_mutex);
            for(NodeMapType::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
                nodes.insert(it->first);
        }
        size_t trickle = GetRand(nodes.size());
        for(std::set<uint64_t>::iterator it = nodes.begin(); it != nodes.end(); ++it)
        {
            CNode* node = GetNode(*it);
            if(node)
            {
                func(*it, node, trickle-- == 0);
                Release(*it);
            }
        }
    }
    
    void Shutdown()
    {
        lock_guard_type lock(m_nodes_mutex);
        m_shutdown = true;
    }
private:

    NodeMapType m_nodes;
    base_mutex_type m_nodes_mutex;
    bool m_shutdown;
};

#endif
