// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SYNC_H
#define BITCOIN_SYNC_H

#include "threadsafety.h"

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include <chrono>

////////////////////////////////////////////////
//                                            //
// THE SIMPLE DEFINITION, EXCLUDING DEBUG CODE //
//                                            //
////////////////////////////////////////////////

/*
CCriticalSection mutex;
    boost::recursive_mutex mutex;

LOCK(mutex);
    boost::unique_lock<boost::recursive_mutex> criticalblock(mutex);

LOCK2(mutex1, mutex2);
    boost::unique_lock<boost::recursive_mutex> criticalblock1(mutex1);
    boost::unique_lock<boost::recursive_mutex> criticalblock2(mutex2);

TRY_LOCK(mutex, name);
    boost::unique_lock<boost::recursive_mutex> name(mutex, boost::try_to_lock_t);

ENTER_CRITICAL_SECTION(mutex); // no RAII
    mutex.lock();

LEAVE_CRITICAL_SECTION(mutex); // no RAII
    mutex.unlock();
 */

///////////////////////////////
//                           //
// THE ACTUAL IMPLEMENTATION //
//                           //
///////////////////////////////

extern thread_local std::string g_thread_name;

/**
 * Template mixin that adds -Wthread-safety locking
 * annotations to a subset of the mutex API.
 */
template <typename PARENT>
class LOCKABLE AnnotatedMixin : public PARENT
{
public:
    void lock() EXCLUSIVE_LOCK_FUNCTION()
    {
        PARENT::lock();
    }

    void unlock() UNLOCK_FUNCTION()
    {
        PARENT::unlock();
    }

    bool try_lock() EXCLUSIVE_TRYLOCK_FUNCTION(true)
    {
        return PARENT::try_lock();
    }
};

#ifdef DEBUG_LOCKORDER
void EnterCritical(const char* pszName, const char* pszFile, int nLine, void* cs, bool fTry = false);
void LeaveCritical();
std::string LocksHeld();
void AssertLockHeldInternal(const char* pszName, const char* pszFile, int nLine, void* cs);
void DeleteLock(void* cs);
#else
void static inline EnterCritical(const char* pszName, const char* pszFile, int nLine, void* cs, bool fTry = false) {}
void static inline LeaveCritical() {}
void static inline AssertLockHeldInternal(const char* pszName, const char* pszFile, int nLine, void* cs) {}
void static inline DeleteLock(void* cs) {}
#endif
#define AssertLockHeld(cs) AssertLockHeldInternal(#cs, __FILE__, __LINE__, &cs)

/**
 * Wrapped boost mutex: supports recursive locking, but no waiting
 * TODO: We should move away from using the recursive lock by default.
 */
class CCriticalSection : public AnnotatedMixin<boost::recursive_mutex>
{
public:
    ~CCriticalSection() {
        DeleteLock((void*)this);
    }
};

/** Wrapped boost mutex: supports waiting but not recursive locking */
typedef AnnotatedMixin<boost::mutex> CWaitableCriticalSection;

/** Just a typedef for boost::condition_variable, can be wrapped later if desired */
typedef boost::condition_variable CConditionVariable;

#ifdef DEBUG_LOCKCONTENTION
void PrintLockContention(const char* pszName, const char* pszFile, int nLine);
#endif
using lock_duration = std::chrono::steady_clock::duration;

template <typename native_handle>
struct thread_lock_map
{
    struct entry
    {
        entry(const char* pszNameIn, const char* pszFileIn, int nLineIn) : pszName(pszNameIn), pszFile(pszFileIn), nLine(nLineIn){}
        bool operator<(const entry& rhs) const {
            return std::tie(pszFile, pszName, nLine) < std::tie(rhs.pszFile, rhs.pszName, rhs.nLine);
        }
        const char* pszName;
        const char* pszFile;
        int nLine;
    };
    thread_lock_map() : m_start_time(std::chrono::steady_clock::now()){}
    ~thread_lock_map()
    {
        bool printed_header = false;
        auto running_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_start_time).count();
        int i = 0;
        for(const auto& mut : lockmap) {
            int index = i++;
            std::chrono::steady_clock::duration mut_time{0};
            for(const auto& item : mut.second) {
                mut_time += item.second;
            }
            auto mut_msec = std::chrono::duration_cast<std::chrono::milliseconds>(mut_time).count();
            double mut_ratio = (double)mut_msec / running_time;
            if (!(mut_ratio > 0)) {
                continue;
            }
            if (!printed_header) {
                printf("lock report for thread: %s\n", g_thread_name.c_str());
                printed_header = true;
            }
            printf("\tmutex %i locked for %li msec. %g %% of thread\n", index, mut_msec, mut_ratio * 100.0);
            for(const auto& item : mut.second) {
                long msec = std::chrono::duration_cast<std::chrono::milliseconds>(item.second).count();
                double thread_ratio = (double)msec / running_time;
                double lock_ratio = (double)msec / std::chrono::duration_cast<std::chrono::milliseconds>(mut_time).count();
                if (!(lock_ratio > 0)) {
                    continue;
                }
                printf("\t\t%s:%i %s locked for: %li msec (%g %% of mutex, %g %% of thread)\n", item.first.pszFile, item.first.nLine, item.first.pszName, msec, lock_ratio * 100.0, thread_ratio * 100.0);
            }
        }
        if (printed_header) {
            printf("\n");
        }
    }
    std::map<native_handle, std::map<entry, lock_duration>> lockmap;
    std::chrono::steady_clock::time_point m_start_time;
};

static thread_local thread_lock_map<CCriticalSection::native_handle_type> g_locktimes;

template <typename Mutex>
static inline void LogLockTime(const char* pszName, const char* pszFile, int nLine, Mutex* mut, lock_duration locktime)
{
    g_locktimes.lockmap[mut->native_handle()][{pszName, pszFile, nLine}] += locktime;
}

/** Wrapper around boost::unique_lock<Mutex> */
template <typename Mutex>
class SCOPED_LOCKABLE CMutexLock
{
private:
    boost::unique_lock<Mutex> lock;

    void Enter(const char* pszName, const char* pszFile, int nLine)
    {
        EnterCritical(pszName, pszFile, nLine, (void*)(lock.mutex()));
        auto before = std::chrono::steady_clock::now();
#ifdef DEBUG_LOCKCONTENTION
        if (!lock.try_lock()) {
            PrintLockContention(pszName, pszFile, nLine);
#endif
            lock.lock();
#ifdef DEBUG_LOCKCONTENTION
        }
#endif
        LogLockTime(pszName, pszFile, nLine, lock.mutex(), std::chrono::steady_clock::now() - before);
    }

    bool TryEnter(const char* pszName, const char* pszFile, int nLine)
    {
        EnterCritical(pszName, pszFile, nLine, (void*)(lock.mutex()), true);
        lock.try_lock();
        if (!lock.owns_lock())
            LeaveCritical();
        return lock.owns_lock();
    }

public:
    CMutexLock(Mutex& mutexIn, const char* pszName, const char* pszFile, int nLine, bool fTry = false) EXCLUSIVE_LOCK_FUNCTION(mutexIn) : lock(mutexIn, boost::defer_lock)
    {
        if (fTry)
            TryEnter(pszName, pszFile, nLine);
        else
            Enter(pszName, pszFile, nLine);
    }

    CMutexLock(Mutex* pmutexIn, const char* pszName, const char* pszFile, int nLine, bool fTry = false) EXCLUSIVE_LOCK_FUNCTION(pmutexIn)
    {
        if (!pmutexIn) return;

        lock = boost::unique_lock<Mutex>(*pmutexIn, boost::defer_lock);
        if (fTry)
            TryEnter(pszName, pszFile, nLine);
        else
            Enter(pszName, pszFile, nLine);
    }

    ~CMutexLock() UNLOCK_FUNCTION()
    {
        if (lock.owns_lock())
            LeaveCritical();
    }

    operator bool()
    {
        return lock.owns_lock();
    }
};

typedef CMutexLock<CCriticalSection> CCriticalBlock;

#define PASTE(x, y) x ## y
#define PASTE2(x, y) PASTE(x, y)

#define LOCK(cs) CCriticalBlock PASTE2(criticalblock, __COUNTER__)(cs, #cs, __FILE__, __LINE__)
#define LOCK2(cs1, cs2) CCriticalBlock criticalblock1(cs1, #cs1, __FILE__, __LINE__), criticalblock2(cs2, #cs2, __FILE__, __LINE__)
#define TRY_LOCK(cs, name) CCriticalBlock name(cs, #cs, __FILE__, __LINE__, true)

#define ENTER_CRITICAL_SECTION(cs)                            \
    {                                                         \
        EnterCritical(#cs, __FILE__, __LINE__, (void*)(&cs)); \
        (cs).lock();                                          \
    }

#define LEAVE_CRITICAL_SECTION(cs) \
    {                              \
        (cs).unlock();             \
        LeaveCritical();           \
    }

class CSemaphore
{
private:
    boost::condition_variable condition;
    boost::mutex mutex;
    int value;

public:
    CSemaphore(int init) : value(init) {}

    void wait()
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        while (value < 1) {
            condition.wait(lock);
        }
        value--;
    }

    bool try_wait()
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        if (value < 1)
            return false;
        value--;
        return true;
    }

    void post()
    {
        {
            boost::unique_lock<boost::mutex> lock(mutex);
            value++;
        }
        condition.notify_one();
    }
};

/** RAII-style semaphore lock */
class CSemaphoreGrant
{
private:
    CSemaphore* sem;
    bool fHaveGrant;

public:
    void Acquire()
    {
        if (fHaveGrant)
            return;
        sem->wait();
        fHaveGrant = true;
    }

    void Release()
    {
        if (!fHaveGrant)
            return;
        sem->post();
        fHaveGrant = false;
    }

    bool TryAcquire()
    {
        if (!fHaveGrant && sem->try_wait())
            fHaveGrant = true;
        return fHaveGrant;
    }

    void MoveTo(CSemaphoreGrant& grant)
    {
        grant.Release();
        grant.sem = sem;
        grant.fHaveGrant = fHaveGrant;
        fHaveGrant = false;
    }

    CSemaphoreGrant() : sem(NULL), fHaveGrant(false) {}

    CSemaphoreGrant(CSemaphore& sema, bool fTry = false) : sem(&sema), fHaveGrant(false)
    {
        if (fTry)
            TryAcquire();
        else
            Acquire();
    }

    ~CSemaphoreGrant()
    {
        Release();
    }

    operator bool()
    {
        return fHaveGrant;
    }
};

#endif // BITCOIN_SYNC_H
