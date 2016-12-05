// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <mutex>
#include <condition_variable>
#include <atomic>

class CThreadInterrupt
{
public:
    CThreadInterrupt(std::condition_variable& condIn, std::mutex& mutIn) : cond(condIn), mut(mutIn), val(false){}
    void reset()
    {
        val.store(false, std::memory_order_release);
    }
    void operator()()
    {
        {
            std::unique_lock<std::mutex> lock(mut);
            val.store(true, std::memory_order_release);
        }
        cond.notify_all();
    }
    explicit operator bool() const
    {
        return val.load(std::memory_order_acquire) == true;
    }
private:
    std::condition_variable& cond;
    std::mutex& mut;
    std::atomic<bool> val;
    
    template <typename Duration>
    friend bool InterruptibleSleep(const Duration&, CThreadInterrupt&);
};

template <typename Duration>
bool InterruptibleSleep(const Duration& rel_time, CThreadInterrupt& threadInterrupt)
{
    std::unique_lock<std::mutex> lock(threadInterrupt.mut);
    return !threadInterrupt.cond.wait_for(lock, rel_time, [&threadInterrupt](){ return threadInterrupt.val.load(std::memory_order_acquire); });
}
