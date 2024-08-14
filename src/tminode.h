#ifndef TMINODE_H
#define TMINODE_H

#include "tminode_base.h"

namespace tmi {

template <typename T, typename Indices>
class tminode
{
public:
    using base_type = tminode_base<T, Indices>;
    using value_type = T;
private:
    T m_value;
    base_type m_base{};
    tminode* m_prev{nullptr};
    tminode* m_next{nullptr};


public:
    void reset()
    {
        m_base = {};
    }

    tminode(const tminode& rhs) : m_value(rhs.m_value), m_base(rhs.m_base)
    {
        m_base.m_node = this;
    }

    explicit tminode(const T& elem) : m_value(elem)
    {
        m_base.m_node = this;
    }

    template <typename... Args>
    tminode(std::in_place_t, Args&&... args) : m_value(std::forward<Args>(args)...)
    {
        m_base.m_node = this;
    }

    const T& value() const { return m_value; }
    T& value() { return m_value; }

    base_type* get_base()
    {
        return &m_base;
    }

    const base_type* get_base() const
    {
        return &m_base;
    }

    tminode* next() const { return m_next; }
    tminode* prev() const { return m_prev; }

    void link(tminode* prev)
    {
        if (prev) {
            prev->m_next = this;
        }
        m_prev = prev;
    }


    void unlink()
    {
        if (m_prev)
            m_prev->m_next = m_next;
        if (m_next)
            m_next->m_prev = m_prev;
    }
};

} // namespace tmi
#endif // TMINODE_H
