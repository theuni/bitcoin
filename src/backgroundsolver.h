// Copyright (c) 2012-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_BACKGROUNDSOLVER_H
#define BITCOIN_BACKGROUNDSOLVER_H

#include <thread>
#include <mutex>
#include <list>
#include <condition_variable>
#include <future>

class CBackgroundSolver
{
    typedef std::packaged_task<void()> task_type;
    std::list<task_type> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_condvar;
    bool m_interrupt;
public:
    CBackgroundSolver() : m_interrupt(false){}

    template<typename Callable>
    auto Add(Callable&& func) -> decltype(task_type(func).get_future())
    {
        std::packaged_task<void()> task(std::forward<Callable>(func));
        auto future = task.get_future();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if(m_interrupt) {
                task();
                return future;
            }
            m_tasks.push_back(std::move(task));
        }
        m_condvar.notify_all();
        return future;
    }

    void Thread()
    {
        std::list<task_type> thread_tasks;
        while (true) {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condvar.wait(lock, [this]{ return !m_tasks.empty() || m_interrupt; });
                if(!m_tasks.empty())
                    thread_tasks.splice(thread_tasks.end(), m_tasks);
            }
            if(thread_tasks.empty())
                break;
            for(auto&& task : thread_tasks) {
                task();
            }
            thread_tasks.clear();
        }
    }

    void Interrupt()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_interrupt = true;
        }
        m_condvar.notify_all();
    }
};

#endif // BITCOIN_BACKGROUNDSOLVER_H
