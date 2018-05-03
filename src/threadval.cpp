// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <threadval.h>

/*  TODO: using thread_local changes the abi in ways that may not play nice
    when the c++ stdlib is linked dynamically. Disable it until thorough
    testing has been done. */
#undef HAVE_THREAD_LOCAL

#if defined(HAVE_THREAD_LOCAL)
#include <atomic>
#include <thread>

#elif defined(HAVE_PTHREAD)
#include <atomic>
#include <pthread.h>

#else
#include <mutex>
#include <thread>
#include <unordered_map>
#endif

struct thread_data_type
{
    constexpr thread_data_type() = default;
    thread_data_type(long id, std::string name) : m_id(id), m_name(std::move(name)){}

    long m_id{0};
    std::string m_name{"unnamed"};
};

#if defined(HAVE_THREAD_LOCAL)

static thread_local thread_data_type g_thread_data;
std::string thread_data::get_internal_name()
{
    return g_thread_data.m_name;
}

long thread_data::get_internal_id()
{
    return g_thread_data.m_id;
}

bool thread_data::set_internal_name(std::string name)
{
    static std::atomic<long> internal_id{0};
    g_thread_data = {internal_id++, std::move(name)};
    return true;
}

#elif defined(HAVE_PTHREAD)

static pthread_key_t g_key;
static void destruct_data(void* data)
{
    delete static_cast<thread_data_type*>(data);
}

static void make_key()
{
    pthread_key_create(&g_key, destruct_data);
}

std::string thread_data::get_internal_name()
{
    void* data = pthread_getspecific(g_key);
    if (data) {
        return static_cast<thread_data_type*>(data)->m_name;
    }
    return "unnamed";
}
long thread_data::get_internal_id()
{
    void* data = pthread_getspecific(g_key);
    if (data) {
        return static_cast<thread_data_type*>(data)->m_id;
    }
    return -1;
}
bool thread_data::set_internal_name(std::string name)
{
    static std::atomic<long> internal_id{0};
    static pthread_once_t key_once = PTHREAD_ONCE_INIT;
    if (pthread_once(&key_once, make_key)) {
        return false;
    }
    void* data = pthread_getspecific(g_key);
    if (data) {
        static_cast<thread_data_type*>(data)->m_name = std::move(name);
        return true;
    }
    data = new thread_data_type{internal_id++, std::move(name)};
    return pthread_setspecific(g_key, data) == 0;
}
#else

static std::unordered_map<std::thread::id, thread_data_type> m_thread_map;
std::mutex m_map_mutex;

static inline thread_data_type get_thread_data()
{
    thread_data_type ret{-1, "unnamed"};
    std::thread::id thread_id(std::this_thread::get_id());
    {
        std::lock_guard<std::mutex> lock(m_map_mutex);
        auto it = m_thread_map.find(thread_id);
        if(it != m_thread_map.end()) {
            ret = it->second;
        }
    }
    return ret;
}

bool thread_data::set_internal_name(std::string name)
{
    static long internal_id{0};
    std::string name_copy(name);
    std::thread::id thread_id(std::this_thread::get_id());
    {
        std::lock_guard<std::mutex> lock(m_map_mutex);
        auto it = m_thread_map.emplace(std::piecewise_construct, std::forward_as_tuple(std::move(thread_id)), std::forward_as_tuple(internal_id++, std::move(name)));
        if (it.second) {
            it.first->second.m_name = std::move(name_copy);
        }
    }
    return true;
}

long thread_data::get_internal_id()
{
    return get_thread_data().m_id;
}

std::string thread_data::get_internal_name()
{
    return get_thread_data().m_name;
}
#endif
