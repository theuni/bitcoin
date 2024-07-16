#ifndef RBNODE_H
#define RBNODE_H

#include <array>

template <typename T, int Size>
class rbnode_base
{
public:
    enum Color : bool
    {
        RED = false,
        BLACK = true
    };
private:
    struct info
    {
        rbnode_base* m_left{nullptr};
        rbnode_base* m_right{nullptr};
        rbnode_base* m_parent{nullptr};
    };
    std::array<info, Size> m_info{};
    unsigned m_color{0};

    static_assert(Size <= sizeof(m_color) * 8);


public:
    void reset()
    {
        m_info = {};
        m_color = 0;
    }

    template <int I>
    void set_right(rbnode_base* rhs) { std::get<I>(m_info).m_right = rhs; }

    template <int I>
    void set_left(rbnode_base* rhs) { std::get<I>(m_info).m_left = rhs; }

    template <int I>
    rbnode_base* parent() const {
        return std::get<I>(m_info).m_parent;
    }

    template <int I>
    void set_parent(rbnode_base* rhs) {
        std::get<I>(m_info).m_parent = rhs;
    }

    template <int I>
    rbnode_base* left() const { return std::get<I>(m_info).m_left; }

    template <int I>
    rbnode_base* right() const {return std::get<I>(m_info).m_right; }

    
    template <int I>
    Color color() const
    {
        return static_cast<Color>(((m_color >> I) & 1));
    }

    template <int I>
    void set_color(Color rhs)
    {
        static constexpr decltype(m_color) mask = -1U ^ (1 << I);
        m_color &= mask;
        m_color |= static_cast<decltype(m_color)>(rhs) << I;
    }
};

template <typename T, int Size>
class rbnode : public rbnode_base<T, Size>
{
    using rbnode_base_type = rbnode_base<T, Size>;

    rbnode* m_prev{nullptr};
    rbnode* m_next{nullptr};

    T m_value;


public:

    explicit rbnode(rbnode* prev, T elem) : m_prev(prev), m_value(std::move(elem))
    {
       if(m_prev) m_prev->m_next = this;
    }

    template <typename... Args>
    rbnode(rbnode* prev, Args&&... args) : m_prev(prev), m_value(std::forward<Args>(args)...)
    {
       if(m_prev) m_prev->m_next = this;
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
