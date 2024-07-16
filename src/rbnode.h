#ifndef RBNODE_H
#define RBNODE_H

#include <array>

template <typename T, int Size>
class rbnode
{
public:

    enum Color : bool
    {
        RED = false,
        BLACK = true
    };
    struct info
    {
        rbnode* m_node{nullptr};
        info* m_left{nullptr};
        info* m_right{nullptr};
        info* m_parent{nullptr};
        Color m_color{Color::RED};

        void set_right(info* rhs) {
            m_right = rhs;
        }

        void set_left(info* rhs) {
            m_left = rhs;
        }

        info* parent() const {
            return m_parent;
        }

        void set_parent(info* rhs) {
            m_parent = rhs;
        }

        info* left() const {
            return m_left;
        }

        info* right() const {
            return m_right;
        }

        Color color() const
        {
            return m_color;
        }

        void set_color(Color rhs)
        {
            m_color = rhs;
        }

    };

    T m_value;

    std::array<info, Size> m_info{};

    rbnode* m_prev{nullptr};
    rbnode* m_next{nullptr};


public:
    void reset()
    {
        m_info = {};
    }

    explicit rbnode(rbnode* prev, T elem) : m_prev(prev), m_value(std::move(elem))
    {
       if(m_prev) m_prev->m_next = this;
       for (auto& elem : m_info) {
           elem.m_node = this;
       }
    }

    template <typename... Args>
    rbnode(rbnode* prev, Args&&... args) : m_value(std::forward<Args>(args)...), m_prev(prev)
    {
       if(m_prev) m_prev->m_next = this;
       for (auto& elem : m_info) {
           elem.m_node = this;
       }
    }

    const T& value() const { return m_value; }
    T& value()  { return m_value; }


    rbnode* next() const {return m_next; }
    rbnode* prev() const {return m_prev; }

    void unlink()
    {
        if (m_prev)
            m_prev->m_next = m_next;
        if (m_next)
            m_next->m_prev = m_prev;
    }

};

#endif // RBNODE_H
