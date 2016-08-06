// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPTCHECK_H
#define BITCOIN_SCRIPTCHECK_H
#include "uint256.h"

#include <list>
#include <set>
#include <array>
#include <boost/thread/tss.hpp>
#include <stdio.h>
#include <assert.h>

struct SignatureCacheResults {
    std::list<uint256> hits;
    std::list<uint256> misses;
};

template <int Size>
class CScriptCheckList
{
    static void cleanup(SignatureCacheResults*)
    {
        // don't delete stack-allocated data.
    }
public:
    CScriptCheckList() : m_ptr(&cleanup)
    {
    }

    SignatureCacheResults& get()
    {
        SignatureCacheResults* ptr = m_ptr.get();
        if(!ptr) {
            ptr = &m_results[m_max_index++];
            m_ptr.reset(ptr);
        }
        assert(ptr != NULL);
        return *ptr;
    }

    std::list<uint256> get_hits()
    {
        std::list<uint256> ret;
        for(int i = 0; i <= m_max_index; i++)
            ret.splice(ret.end(), m_results[i].hits);
        return ret;
    }

    std::list<uint256> get_misses()
    {
        std::list<uint256> ret;
        for(int i = 0; i <= m_max_index; i++)
            ret.splice(ret.end(), m_results[i].misses);
        return ret;
    }
    
private:
    std::array<SignatureCacheResults, Size> m_results;
    boost::thread_specific_ptr<SignatureCacheResults> m_ptr;
    int m_max_index = 0;
};

#endif // BITCOIN_SCRIPTCHECK_H
