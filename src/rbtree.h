#include "rbnode.h"
#include <vector>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <tuple>

template <typename T, typename Allocator = std::allocator<T>, typename... Comparators>
class rbtree
{
    using comparator_types = std::tuple<Comparators...>;
    static constexpr int num_comparators = sizeof...(Comparators);
    using base_node_type = rbnode_base<T, num_comparators>;
    using node_type = rbnode<T, num_comparators>;
    using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<node_type>;
    using Color = typename base_node_type::Color;

    base_node_type m_root{};
    node_type* m_begin{nullptr};
    node_type* m_end{nullptr};
    size_t m_size{0};

    allocator_type m_alloc;


    /* The below insert/erase impls were copied from libc++ */

    template <int I>
    static base_node_type* tree_max(base_node_type* x) {
      while (x->template right<I>() != nullptr)
        x = x->template right<I>();
      return x;
    }

    template <int I>
    static bool tree_is_left_child(base_node_type* x)
    {
        return x == x->template parent<I>()->template left<I>();
    }

    template <int I>
    static base_node_type* tree_min(base_node_type* x)
    {
        while (x->template left<I>() != nullptr)
            x = x->template left<I>();
        return x;
    }

    template <int I>
    static base_node_type* tree_next(base_node_type* x)
    {
        if (x->template right<I>() != nullptr)
            return tree_min<I>(x->template right<I>());
        while (!tree_is_left_child<I>(x))
            x = x->template parent<I>();
        return x->template parent<I>();
    }

    template <int I>
    static base_node_type* tree_prev(base_node_type* x) {
      if (x->template left<I>() != nullptr)
        return tree_max<I>(x->template left<I>());
      while (tree_is_left_child<I>(x))
        x = x->template parent<I>();
      return x->template parent<I>();
    }

    template <int I>
    static void tree_left_rotate(base_node_type* x)
    {
        base_node_type* y = x->template right<I>();
        x->template set_right<I>(y->template left<I>());
        if (x->template right<I>() != nullptr)
            x->template right<I>()->template set_parent<I>(x);
        y->template set_parent<I>(x->template parent<I>());
        if (tree_is_left_child<I>(x))
            x->template parent<I>()->template set_left<I>(y);
        else
            x->template parent<I>()->template set_right<I>(y);
        y->template set_left<I>(x);
        x->template set_parent<I>(y);
    }

    template <int I>
    static void tree_right_rotate(base_node_type* x)
    {
        base_node_type* y = x->template left<I>();
        x->template set_left<I>(y->template right<I>());
        if (x->template left<I>() != nullptr)
            x->template left<I>()->template set_parent<I>(x);
        y->template set_parent<I>(x->template parent<I>());
        if (tree_is_left_child<I>(x))
            x->template parent<I>()->template set_left<I>(y);
        else
            x->template parent<I>()->template set_right<I>(y);
        y->template set_right<I>(x);
        x->template set_parent<I>(y);
    }


    // Precondition:  root != nullptr && z != nullptr.
    //                tree_invariant(root) == true.
    //                z == root or == a direct or indirect child of root.
    // Effects:  unlinks z from the tree rooted at root, rebalancing as needed.
    // Postcondition: tree_invariant(end_node->template left<I>()) == true && end_node->template left<I>()
    //                nor any of its children refer to z.  end_node->template left<I>()
    //                may be different than the value passed in as root.
    template <int I>
    static void tree_remove(base_node_type* root, base_node_type* z)
    {
        assert(root);
        assert(z);
        // z will be removed from the tree.  Client still needs to destruct/deallocate it
        // y is either z, or if z has two children, tree_next(z).
        // y will have at most one child.
        // y will be the initial hole in the tree (make the hole at a leaf)
        base_node_type* y = (z->template left<I>() == nullptr || z->template right<I>() == nullptr) ?
                        z : tree_next<I>(z);
        // x is y's possibly null single child
        base_node_type* x = y->template left<I>() != nullptr ? y->template left<I>() : y->template right<I>();
        // w is x's possibly null uncle (will become x's sibling)
        base_node_type* w = nullptr;
        // link x to y's parent, and find w
        if (x != nullptr)
            x->template set_parent<I>(y->template parent<I>());
        if (tree_is_left_child<I>(y))
        {
            y->template parent<I>()->template set_left<I>(x);
            if (y != root)
                w = y->template parent<I>()->template right<I>();
            else
                root = x;  // w == nullptr
        }
        else
        {
            y->template parent<I>()->template set_right<I>(x);
            // y can't be root if it is a right child
            w = y->template parent<I>()->template left<I>();
        }
        bool removed_black = y->template color<I>() == Color::BLACK;
        // If we didn't remove z, do so now by splicing in y for z,
        //    but copy z's color.  This does not impact x or w.
        if (y != z)
        {
            // z->template left<I>() != nulptr but z->template right<I>() might == x == nullptr
            y->template set_parent<I>(z->template parent<I>());
            if (tree_is_left_child<I>(z))
                y->template parent<I>()->template set_left<I>(y);
            else
                y->template parent<I>()->template set_right<I>(y);
            y->template set_left<I>(z->template left<I>());
            y->template left<I>()->template set_parent<I>(y);
            y->template set_right<I>(z->template right<I>());
            if (y->template right<I>() != nullptr)
                y->template right<I>()->template set_parent<I>(y);
            y->template set_color<I>(z->template color<I>());
            if (root == z)
                root = y;
        }
        if (removed_black && root != nullptr)
        {
            // Rebalance:
            // x has an implicit black color (transferred from the removed y)
            //    associated with it, no matter what its color is.
            // If x is root (in which case it can't be null), it is supposed
            //    to be black anyway, and if it is doubly black, then the double
            //    can just be ignored.
            // If x is red (in which case it can't be null), then it can absorb
            //    the implicit black just by setting its color to black.
            // Since y was black and only had one child (which x points to), x
            //   is either red with no children, else null, otherwise y would have
            //   different black heights under left and right pointers.
            // if (x == root || x != nullptr && !x->is_black_)
            if (x != nullptr)
                x->template set_color<I>(Color::BLACK);
            else {
                //  Else x isn't root, and is "doubly black", even though it may
                //     be null.  w can not be null here, else the parent would
                //     see a black height >= 2 on the x side and a black height
                //     of 1 on the w side (w must be a non-null black or a red
                //     with a non-null black child).
                fixup_after_remove<I>(root, w);
            }
        }
    }

    template <int I>
    static void tree_balance_after_insert(base_node_type* root, base_node_type* x)
    {
        x->template set_color<I>(x == root ? Color::BLACK : Color::RED);
        while (x != root && x->template parent<I>()->template color<I>() == Color::RED)
        {
            // x->template parent<I>() != root because x->template parent<I>()->is_black == false
            if (tree_is_left_child<I>(x->template parent<I>()))
            {
                base_node_type* y = x->template parent<I>()->template parent<I>()->template right<I>();
                if (y != nullptr && y->template color<I>() == Color::RED)
                {
                    x = x->template parent<I>();
                    x->template set_color<I>(Color::BLACK);
                    x = x->template parent<I>();
                    x->template set_color<I>(x == root ? Color::BLACK : Color::RED);
                    y->template set_color<I>(Color::BLACK);
                }
                else
                {
                    if (!tree_is_left_child<I>(x))
                    {
                        x = x->template parent<I>();
                        tree_left_rotate<I>(x);
                    }
                    x = x->template parent<I>();
                    x->template set_color<I>(Color::BLACK);
                    x = x->template parent<I>();
                    x->template set_color<I>(Color::RED);
                    tree_right_rotate<I>(x);
                    break;
                }
            }
            else
            {
                base_node_type* y = x->template parent<I>()->template parent<I>()->template left<I>();
                if (y != nullptr && y->template color<I>() == Color::RED)
                {
                    x = x->template parent<I>();
                    x->template set_color<I>(Color::BLACK);
                    x = x->template parent<I>();
                    x->template set_color<I>(x == root ? Color::BLACK : Color::RED);
                    y->template set_color<I>(Color::BLACK);
                }
                else
                {
                    if (tree_is_left_child<I>(x))
                    {
                        x = x->template parent<I>();
                        tree_right_rotate<I>(x);
                    }
                    x = x->template parent<I>();
                    x->template set_color<I>(Color::BLACK);
                    x = x->template parent<I>();
                    x->template set_color<I>(Color::RED);
                    tree_left_rotate<I>(x);
                    break;
                }
            }
        }
    }

    template <int I>
    static void fixup_after_remove(base_node_type* root, base_node_type* w)
    {
        base_node_type* x = nullptr;
        while (true)
        {
            if (!tree_is_left_child<I>(w))  // if x is left child
            {
                if (w->template color<I>() == Color::RED)
                {
                    w->template set_color<I>(Color::BLACK);
                    w->template parent<I>()->template set_color<I>(Color::RED);
                    tree_left_rotate<I>(w->template parent<I>());
                    // x is still valid
                    // reset root only if necessary
                    if (root == w->template left<I>())
                        root = w;
                    // reset sibling, and it still can't be null
                    w = w->template left<I>()->template right<I>();
                }
                // w->is_black_ is now true, w may have null children
                if ((w->template left<I>()  == nullptr || w->template left<I>()->template color<I>() == Color::BLACK) &&
                    (w->template right<I>() == nullptr || w->template right<I>()->template color<I>() == Color::BLACK))
                {
                    w->template set_color<I>(Color::RED);
                    x = w->template parent<I>();
                    // x can no longer be null
                    if (x == root || x->template color<I>() == Color::RED)
                    {
                        x->template set_color<I>(Color::BLACK);
                        break;
                    }
                    // reset sibling, and it still can't be null
                    w = tree_is_left_child<I>(x) ?
                                x->template parent<I>()->template right<I>() :
                                x->template parent<I>()->template left<I>();
                    // continue;
                }
                else  // w has a red child
                {
                    if (w->template right<I>() == nullptr || w->template right<I>()->template color<I>() == Color::BLACK)
                    {
                        // w left child is non-null and red
                        w->template left<I>()->template set_color<I>(Color::BLACK);
                        w->template set_color<I>(Color::RED);
                        tree_right_rotate<I>(w);
                        // w is known not to be root, so root hasn't changed
                        // reset sibling, and it still can't be null
                        w = w->template parent<I>();
                    }
                    // w has a right red child, left child may be null
                    w->template set_color<I>(w->template parent<I>()->template color<I>());
                    w->template parent<I>()->template set_color<I>(Color::BLACK);
                    w->template right<I>()->template set_color<I>(Color::BLACK);
                    tree_left_rotate<I>(w->template parent<I>());
                    break;
                }
            }
            else
            {
                if (w->template color<I>() == Color::RED)
                {
                    w->template set_color<I>(Color::BLACK);
                    w->template parent<I>()->template set_color<I>(Color::RED);
                    tree_right_rotate<I>(w->template parent<I>());
                    // x is still valid
                    // reset root only if necessary
                    if (root == w->template right<I>())
                        root = w;
                    // reset sibling, and it still can't be null
                    w = w->template right<I>()->template left<I>();
                }
                // w->is_black_ is now true, w may have null children
                if ((w->template left<I>()  == nullptr || w->template left<I>()->template color<I>() == Color::BLACK) &&
                    (w->template right<I>() == nullptr || w->template right<I>()->template color<I>() == Color::BLACK))
                {
                    w->template set_color<I>(Color::RED);
                    x = w->template parent<I>();
                    // x can no longer be null
                    if (x->template color<I>() == Color::RED || x == root)
                    {
                        x->template set_color<I>(Color::BLACK);
                        break;
                    }
                    // reset sibling, and it still can't be null
                    w = tree_is_left_child<I>(x) ?
                                x->template parent<I>()->template right<I>() :
                                x->template parent<I>()->template left<I>();
                    // continue;
                }
                else  // w has a red child
                {
                    if (w->template left<I>() == nullptr || w->template left<I>()->template color<I>() == Color::BLACK)
                    {
                        // w right child is non-null and red
                        w->template right<I>()->template set_color<I>(Color::BLACK);
                        w->template set_color<I>(Color::RED);
                        tree_left_rotate<I>(w);
                        // w is known not to be root, so root hasn't changed
                        // reset sibling, and it still can't be null
                        w = w->template parent<I>();
                    }
                    // w has a left red child, right child may be null
                    w->template set_color<I>(w->template parent<I>()->template color<I>());
                    w->template parent<I>()->template set_color<I>(Color::BLACK);
                    w->template left<I>()->template set_color<I>(Color::BLACK);
                    tree_right_rotate<I>(w->template parent<I>());
                    break;
                }
            }
        }
    }
    template <int I>
    void insert_node(node_type* node) {
      using comparator = std::tuple_element_t<I, comparator_types>;
      base_node_type* parent = nullptr;
      node_type* curr = static_cast<node_type*>(m_root.template left<I>());
      bool inserted_left = false;
      while (curr != nullptr) {
        parent = curr;

        if (comparator()(node->value(), curr->value())) {
          curr = static_cast<node_type*>(curr->template left<I>());
          inserted_left = true;
        } else {
          curr = static_cast<node_type*>(curr->template right<I>());
          inserted_left = false;
        }
      }
      if (parent == nullptr) {
        m_root.template set_left<I>(node);
        parent = &m_root;
      } else {
        if (inserted_left) {
          parent->template set_left<I>(node);
        } else {
          parent->template set_right<I>(node);
        }
      }
      node->template set_parent<I>(parent);
      tree_balance_after_insert<I>(m_root.template left<I>(), node);
    }

    template <int I>
    void do_insert(node_type* node)
    {
        insert_node<I>(node);
        if constexpr (I + 1 < num_comparators) {
            do_insert<I+1>(node);
        }
    }

    void do_insert(node_type* node)
    {
        do_insert<0>(node);
        if (m_begin == nullptr) {
            assert(m_end == nullptr);
            m_begin = m_end = node;
        } else {
            m_end = node;
        }
        m_size++;
    }

    template <int I>
    void do_erase(node_type* node)
    {
        tree_remove<I>(m_root.template left<I>(), node);
        if constexpr (I + 1 < num_comparators) {
            do_erase<I+1>(node);
        }
    }

    void do_erase(node_type* node)
    {
        do_erase<0>(node);
        if (node == m_end) {
            m_end = node->prev();
        }
        if (node == m_begin) {
            m_begin = node->next();
        }
        node->unlink();
        std::destroy_at(node);
        m_alloc.deallocate(node, 1);
        m_size--;
    }

    void erase(const T& value) {
        node_type* node = do_find<0>(value);
        if (node) {
            do_erase(node);
        }
    }

    template <int I>
    void do_resort(node_type* node)
    {
        assert(node != &m_root);
        // Only re-sort if the relative order has changed
        using comparator = std::tuple_element_t<I, comparator_types>;
        base_node_type* min_node = tree_min<I>(m_root.template left<I>());
        base_node_type* max_node = tree_max<I>(m_root.template left<I>());
        node_type* next_ptr = nullptr;
        node_type* prev_ptr = nullptr;
        if (node != min_node)
            prev_ptr = static_cast<node_type*>(tree_prev<I>(node));
        if (node != max_node)
            next_ptr = static_cast<node_type*>(tree_next<I>(node));

        if ((next_ptr != nullptr && comparator()(next_ptr->value(), node->value())) || \
            (prev_ptr != nullptr && comparator()(node->value(), prev_ptr->value())))
        {
            tree_remove<I>(m_root.template left<I>(), node);
            node->template set_parent<I>(nullptr);
            node->template set_left<I>(nullptr);
            node->template set_right<I>(nullptr);
            node->template set_color<I>(Color::RED);
            insert_node<I>(node);
        }
        if constexpr (I + 1 < num_comparators) {
            do_resort<I+1>(node);
        }
    }

public:

    class iterator;
    template <int I>
    class sort_iterator {
        base_node_type* m_node{};
    public:
        friend class iterator;
        friend class rbtree;
        typedef T value_type;
        typedef T* pointer;
        typedef T& reference;
        using element_type = T;
        sort_iterator() = default;
        sort_iterator(base_node_type* node) : m_node(node) {}
        T& operator*() const { return static_cast<node_type*>(m_node)->value(); }
        T* operator->() const { return &static_cast<node_type*>(m_node)->value(); }
        sort_iterator& operator++() {
            m_node = tree_next<I>(m_node);
            return *this;
        }
        sort_iterator& operator--() {
            m_node = tree_prev<I>(m_node);
            return *this;
        }
        sort_iterator operator++(int) { sort_iterator copy(m_node); ++(*this); return copy; }
        sort_iterator operator--(int) { sort_iterator copy(m_node); --(*this); return copy; }
        bool operator==(sort_iterator rhs) const { return m_node == rhs.m_node; }
        bool operator!=(sort_iterator rhs) const { return m_node != rhs.m_node; }
    };

    template <int I>
    class const_sort_iterator {
        const base_node_type* m_node{};
    public:
        typedef const T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        using element_type = const T;
        const_sort_iterator() = default;
        const_sort_iterator(const base_node_type* node) : m_node(node) {}
        const T& operator*() const { return m_node->value(); }
        const T* operator->() const { return &m_node->value(); }
        const_sort_iterator& operator++() {
            m_node = tree_next<I>(m_node);
            return *this;
        }
        const_sort_iterator& operator--() {
            m_node = tree_prev<I>(m_node);
            return *this;
        }
        const_sort_iterator operator++(int) { const_sort_iterator copy(m_node); ++(*this); return copy; }
        const_sort_iterator operator--(int) { const_sort_iterator copy(m_node); --(*this); return copy; }
        bool operator==(const_sort_iterator rhs) const { return m_node == rhs.m_node; }
        bool operator!=(const_sort_iterator rhs) const { return m_node != rhs.m_node; }
    };

    class iterator {
        node_type* m_node{};
    public:
        friend class rbtree;
        friend class const_iterator;
        typedef T value_type;
        typedef T* pointer;
        typedef T& reference;
        using element_type = T;
        iterator() = default;
        template <int I>
        iterator(sort_iterator<I> it) : m_node(static_cast<node_type*>(it.m_node)){}
        iterator(node_type* node) : m_node(node) {}
        T& operator*() const { return m_node->value(); }
        T* operator->() const { return &m_node->value(); }
        iterator& operator++() { m_node = m_node->next(); return *this; }
        iterator& operator--() { m_node = m_node->prev(); return *this; }
        iterator operator++(int) { iterator copy(m_node); ++(*this); return copy; }
        iterator operator--(int) { iterator copy(m_node); --(*this); return copy; }
        bool operator==(iterator rhs) const { return m_node == rhs.m_node; }
        bool operator!=(iterator rhs) const { return m_node != rhs.m_node; }
    };

    class const_iterator {
        const node_type* m_node{};
    public:
        friend class rbtree;
        typedef const T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        using element_type = const T;
        const_iterator() = default;
        const_iterator(iterator it) : m_node(it.m_node) {}
        const_iterator(const node_type* node) : m_node(node) {}
        const T& operator*() const { return m_node->value(); }
        const T* operator->() const { return &m_node->value(); }
        const_iterator& operator++() { m_node = m_node->next(); return *this; }
        const_iterator& operator--() { m_node = m_node->prev(); return *this; }
        const_iterator operator++(int) { const_iterator copy(m_node); ++(*this); return copy; }
        const_iterator operator--(int) { const_iterator copy(m_node); --(*this); return copy; }
        bool operator==(const_iterator rhs) const { return m_node == rhs.m_node; }
        bool operator!=(const_iterator rhs) const { return m_node != rhs.m_node; }
    };


    rbtree() = default;

    ~rbtree() {
        clear();
    }

    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    {
        node_type* node = m_alloc.allocate(1);
        node = std::construct_at(node, m_end, std::forward<Args>(args)...);
        do_insert(node);
        return std::make_pair(iterator(node), true);
    }

    void clear()
    {
        auto* node = m_begin;
        while(node) {
            auto* to_delete = node;
            node = node->next();
            std::destroy_at(to_delete);
            m_alloc.deallocate(to_delete, 1);
        }
        m_begin = m_end = nullptr;
        m_root = {};
    }

    iterator erase(const_iterator it) {
        node_type* node = const_cast<node_type*>(it.m_node);
        node_type* ret = node->next();
        do_erase(node);
        return iterator(ret);
    }

    template <typename Callable>
    void modify(const T& elem, Callable&& func) {
        base_node_type* node = do_find<0>(elem);
        if (node) {
            func(node->value());
            do_resort<0>(node);
        }
    }

    template <typename Callable>
    void modify(const_iterator it, Callable&& func) {
        node_type* node = const_cast<node_type*>(it.m_node);
        if (node) {
            func(node->value());
            do_resort<0>(node);
        }
    }

    size_t size() const { return m_size; }

    iterator begin() const { return iterator(m_begin); }

    iterator end() const { return iterator(nullptr); }


    template <int I>
    sort_iterator<I> sort_begin() const
    {
        base_node_type* root = m_root.template left<I>();
        if (root) {
            return sort_iterator<I>(static_cast<node_type*>(tree_min<I>(root)));
        }
        root = const_cast<base_node_type*>(&m_root);
        return sort_iterator<I>(static_cast<node_type*>(root));
    }

    template <int I>
    sort_iterator<I> sort_end() const
    {
        base_node_type* root = const_cast<base_node_type*>(&m_root);
        return sort_iterator<I>(static_cast<node_type*>(root));
    }

};
