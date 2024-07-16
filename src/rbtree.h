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
    using node_type = rbnode<T, num_comparators>;
    using info_type = typename rbnode<T, num_comparators>::info;
    using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<node_type>;
    using Color = typename node_type::Color;

    std::array<info_type, num_comparators> m_roots{};
    node_type* m_begin{nullptr};
    node_type* m_end{nullptr};
    size_t m_size{0};

    allocator_type m_alloc;

    /*

    The below insert/erase impls were copied from libc++

    Their base type inheritance was dropped in order to come closer to
    providing a standard layout. Keeping T as he first member allows us to
    convert Between T* and node_type*. This is the trick that makes iterator_to
    work.


    Their root node trick is borrowed as well. The following is adapted from
    their comment:

    The algorithms taking info_type* are red black tree algorithms.  Those
    algorithms taking a parameter named root should assume that root
    points to a proper red black tree (unless otherwise specified).

    Each algorithm herein assumes that root->m_parent points to a non-null
    structure which has a member m_left which points back to root.  No other
    member is read or written to at root->m_parent.

    root->m_parent_ will be referred to below (in comments only) as m_roots[I].
    m_roots[I]->m_left is an externably accessible lvalue for root, and can be
    changed by node insertion and removal (without explicit reference to
    m_roots[I]).

    All nodes (with the exception of m_roots[I]), even the node referred to as
    root, have a non-null m_parent field.

*/


    static info_type* tree_max(info_type* x) {
      while (x->right() != nullptr)
        x = x->right();
      return x;
    }

    static bool tree_is_left_child(info_type* x)
    {
        return x == x->parent()->left();
    }

    static info_type* tree_min(info_type* x)
    {
        while (x->left() != nullptr)
            x = x->left();
        return x;
    }

    static info_type* tree_next(info_type* x)
    {
        if (x->right() != nullptr)
            return tree_min(x->right());
        while (!tree_is_left_child(x))
            x = x->parent();
        return x->parent();
    }

    static info_type* tree_prev(info_type* x) {
      if (x->left() != nullptr)
        return tree_max(x->left());
      while (tree_is_left_child(x))
        x = x->parent();
      return x->parent();
    }

    static void tree_left_rotate(info_type* x)
    {
        info_type* y = x->right();
        x->set_right(y->left());
        if (x->right() != nullptr)
            x->right()->set_parent(x);
        y->set_parent(x->parent());
        if (tree_is_left_child(x))
            x->parent()->set_left(y);
        else
            x->parent()->set_right(y);
        y->set_left(x);
        x->set_parent(y);
    }

    static void tree_right_rotate(info_type* x)
    {
        info_type* y = x->left();
        x->set_left(y->right());
        if (x->left() != nullptr)
            x->left()->set_parent(x);
        y->set_parent(x->parent());
        if (tree_is_left_child(x))
            x->parent()->set_left(y);
        else
            x->parent()->set_right(y);
        y->set_right(x);
        x->set_parent(y);
    }


    // Precondition:  root != nullptr && z != nullptr.
    //                tree_invariant(root) == true.
    //                z == root or == a direct or indirect child of root.
    // Effects:  unlinks z from the tree rooted at root, rebalancing as needed.
    // Postcondition: tree_invariant(end_node->left()) == true && end_node->left()
    //                nor any of its children refer to z.  end_node->left()
    //                may be different than the value passed in as root.
    static void tree_remove(info_type* root, info_type* z)
    {
        assert(root);
        assert(z);
        // z will be removed from the tree.  Client still needs to destruct/deallocate it
        // y is either z, or if z has two children, tree_next(z).
        // y will have at most one child.
        // y will be the initial hole in the tree (make the hole at a leaf)
        info_type* y = (z->left() == nullptr || z->right() == nullptr) ?
                        z : tree_next(z);
        // x is y's possibly null single child
        info_type* x = y->left() != nullptr ? y->left() : y->right();
        // w is x's possibly null uncle (will become x's sibling)
        info_type* w = nullptr;
        // link x to y's parent, and find w
        if (x != nullptr)
            x->set_parent(y->parent());
        if (tree_is_left_child(y))
        {
            y->parent()->set_left(x);
            if (y != root)
                w = y->parent()->right();
            else
                root = x;  // w == nullptr
        }
        else
        {
            y->parent()->set_right(x);
            // y can't be root if it is a right child
            w = y->parent()->left();
        }
        bool removed_black = y->color() == Color::BLACK;
        // If we didn't remove z, do so now by splicing in y for z,
        //    but copy z's color.  This does not impact x or w.
        if (y != z)
        {
            // z->left() != nulptr but z->right() might == x == nullptr
            y->set_parent(z->parent());
            if (tree_is_left_child(z))
                y->parent()->set_left(y);
            else
                y->parent()->set_right(y);
            y->set_left(z->left());
            y->left()->set_parent(y);
            y->set_right(z->right());
            if (y->right() != nullptr)
                y->right()->set_parent(y);
            y->set_color(z->color());
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
                x->set_color(Color::BLACK);
            else {
                //  Else x isn't root, and is "doubly black", even though it may
                //     be null.  w can not be null here, else the parent would
                //     see a black height >= 2 on the x side and a black height
                //     of 1 on the w side (w must be a non-null black or a red
                //     with a non-null black child).
                fixup_after_remove(root, w);
            }
        }
    }

    static void tree_balance_after_insert(info_type* root, info_type* x)
    {
        x->set_color(x == root ? Color::BLACK : Color::RED);
        while (x != root && x->parent()->color() == Color::RED)
        {
            // x->parent() != root because x->parent()->is_black == false
            if (tree_is_left_child(x->parent()))
            {
                info_type* y = x->parent()->parent()->right();
                if (y != nullptr && y->color() == Color::RED)
                {
                    x = x->parent();
                    x->set_color(Color::BLACK);
                    x = x->parent();
                    x->set_color(x == root ? Color::BLACK : Color::RED);
                    y->set_color(Color::BLACK);
                }
                else
                {
                    if (!tree_is_left_child(x))
                    {
                        x = x->parent();
                        tree_left_rotate(x);
                    }
                    x = x->parent();
                    x->set_color(Color::BLACK);
                    x = x->parent();
                    x->set_color(Color::RED);
                    tree_right_rotate(x);
                    break;
                }
            }
            else
            {
                info_type* y = x->parent()->parent()->left();
                if (y != nullptr && y->color() == Color::RED)
                {
                    x = x->parent();
                    x->set_color(Color::BLACK);
                    x = x->parent();
                    x->set_color(x == root ? Color::BLACK : Color::RED);
                    y->set_color(Color::BLACK);
                }
                else
                {
                    if (tree_is_left_child(x))
                    {
                        x = x->parent();
                        tree_right_rotate(x);
                    }
                    x = x->parent();
                    x->set_color(Color::BLACK);
                    x = x->parent();
                    x->set_color(Color::RED);
                    tree_left_rotate(x);
                    break;
                }
            }
        }
    }

    static void fixup_after_remove(info_type* root, info_type* w)
    {
        info_type* x = nullptr;
        while (true)
        {
            if (!tree_is_left_child(w))  // if x is left child
            {
                if (w->color() == Color::RED)
                {
                    w->set_color(Color::BLACK);
                    w->parent()->set_color(Color::RED);
                    tree_left_rotate(w->parent());
                    // x is still valid
                    // reset root only if necessary
                    if (root == w->left())
                        root = w;
                    // reset sibling, and it still can't be null
                    w = w->left()->right();
                }
                // w->is_black_ is now true, w may have null children
                if ((w->left()  == nullptr || w->left()->color() == Color::BLACK) &&
                    (w->right() == nullptr || w->right()->color() == Color::BLACK))
                {
                    w->set_color(Color::RED);
                    x = w->parent();
                    // x can no longer be null
                    if (x == root || x->color() == Color::RED)
                    {
                        x->set_color(Color::BLACK);
                        break;
                    }
                    // reset sibling, and it still can't be null
                    w = tree_is_left_child(x) ?
                                x->parent()->right() :
                                x->parent()->left();
                    // continue;
                }
                else  // w has a red child
                {
                    if (w->right() == nullptr || w->right()->color() == Color::BLACK)
                    {
                        // w left child is non-null and red
                        w->left()->set_color(Color::BLACK);
                        w->set_color(Color::RED);
                        tree_right_rotate(w);
                        // w is known not to be root, so root hasn't changed
                        // reset sibling, and it still can't be null
                        w = w->parent();
                    }
                    // w has a right red child, left child may be null
                    w->set_color(w->parent()->color());
                    w->parent()->set_color(Color::BLACK);
                    w->right()->set_color(Color::BLACK);
                    tree_left_rotate(w->parent());
                    break;
                }
            }
            else
            {
                if (w->color() == Color::RED)
                {
                    w->set_color(Color::BLACK);
                    w->parent()->set_color(Color::RED);
                    tree_right_rotate(w->parent());
                    // x is still valid
                    // reset root only if necessary
                    if (root == w->right())
                        root = w;
                    // reset sibling, and it still can't be null
                    w = w->right()->left();
                }
                // w->is_black_ is now true, w may have null children
                if ((w->left()  == nullptr || w->left()->color() == Color::BLACK) &&
                    (w->right() == nullptr || w->right()->color() == Color::BLACK))
                {
                    w->set_color(Color::RED);
                    x = w->parent();
                    // x can no longer be null
                    if (x->color() == Color::RED || x == root)
                    {
                        x->set_color(Color::BLACK);
                        break;
                    }
                    // reset sibling, and it still can't be null
                    w = tree_is_left_child(x) ?
                                x->parent()->right() :
                                x->parent()->left();
                    // continue;
                }
                else  // w has a red child
                {
                    if (w->left() == nullptr || w->left()->color() == Color::BLACK)
                    {
                        // w right child is non-null and red
                        w->right()->set_color(Color::BLACK);
                        w->set_color(Color::RED);
                        tree_left_rotate(w);
                        // w is known not to be root, so root hasn't changed
                        // reset sibling, and it still can't be null
                        w = w->parent();
                    }
                    // w has a left red child, right child may be null
                    w->set_color(w->parent()->color());
                    w->parent()->set_color(Color::BLACK);
                    w->left()->set_color(Color::BLACK);
                    tree_right_rotate(w->parent());
                    break;
                }
            }
        }
    }

    template <int I>
    info_type* get_root_info() const
    {
        return std::get<I>(m_roots).left();
    }

    template <int I>
    void set_root_node(node_type* node)
    {
        std::get<I>(m_roots).set_left(&std::get<I>(node->m_info));
    }

    template <int I>
    void insert_node(node_type* node) {
      using comparator = std::tuple_element_t<I, comparator_types>;
      info_type* parent = nullptr;
      info_type* curr = get_root_info<I>();

      bool inserted_left = false;
      while (curr != nullptr) {
        parent = curr;

        if (comparator()(node->value(), curr->m_node->value())) {
          curr = curr->left();
          inserted_left = true;
        } else {
          curr = curr->right();
          inserted_left = false;
        }
      }
      if (parent == nullptr) {
        set_root_node<I>(node);
        parent = &m_roots[I];
      } else {
        if (inserted_left) {
          parent->set_left(&std::get<I>(node->m_info));
        } else {
          parent->set_right(&std::get<I>(node->m_info));
        }
      }
      std::get<I>(node->m_info).set_parent(parent);
      tree_balance_after_insert(get_root_info<I>(), &std::get<I>(node->m_info));
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
        tree_remove(get_root_info<I>(), &std::get<I>(node->m_info));
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
        // Only re-sort if the relative order has changed
        using comparator = std::tuple_element_t<I, comparator_types>;
        info_type* next_ptr = nullptr;
        info_type* prev_ptr = nullptr;
        if (&std::get<I>(node->m_info) != tree_min(get_root_info<I>()))
            prev_ptr = tree_prev(&std::get<I>(node->m_info));
        if (&std::get<I>(node->m_info) != tree_max(get_root_info<I>()))
            next_ptr = tree_next(&std::get<I>(node->m_info));

        if ((next_ptr != nullptr && comparator()(next_ptr->m_node->value(), node->value())) || \
            (prev_ptr != nullptr && comparator()(node->value(), prev_ptr->m_node->value())))
        {
            tree_remove(get_root_info<I>(), &std::get<I>(node->m_info));
            std::get<I>(node->m_info).set_parent(nullptr);
            std::get<I>(node->m_info).set_left(nullptr);
            std::get<I>(node->m_info).set_right(nullptr);
            std::get<I>(node->m_info).set_color(Color::RED);
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
        node_type* m_node{};
    public:
        friend class iterator;
        friend class rbtree;
        typedef T value_type;
        typedef T* pointer;
        typedef T& reference;
        using element_type = T;
        sort_iterator() = default;
        sort_iterator(node_type* node) : m_node(node) {}
        T& operator*() const { return m_node->value(); }
        T* operator->() const { return &m_node->value(); }
        sort_iterator& operator++() {
            m_node = tree_next(&std::get<I>(m_node->m_info))->m_node;
            return *this;
        }
        sort_iterator& operator--() {
            m_node = tree_prev(std::get<I>(m_node->m_info))->m_node;
            return *this;
        }
        sort_iterator operator++(int) { sort_iterator copy(m_node); ++(*this); return copy; }
        sort_iterator operator--(int) { sort_iterator copy(m_node); --(*this); return copy; }
        bool operator==(sort_iterator rhs) const { return m_node == rhs.m_node; }
        bool operator!=(sort_iterator rhs) const { return m_node != rhs.m_node; }
    };

    template <int I>
    class const_sort_iterator {
        const node_type* m_node{};
    public:
        typedef const T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        using element_type = const T;
        const_sort_iterator() = default;
        const_sort_iterator(const node_type* node) : m_node(node) {}
        const T& operator*() const { return m_node->value(); }
        const T* operator->() const { return &m_node->value(); }
        const_sort_iterator& operator++() {
            m_node = tree_next(std::get<I>(m_node->m_info))->m_node;
            return *this;
        }
        const_sort_iterator& operator--() {
            m_node = tree_prev(std::get<I>(m_node->m_info))->m_node;
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
        iterator(sort_iterator<I> it) : m_node(it.m_node){}
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
        m_roots = {};
    }

    iterator erase(const_iterator it) {
        node_type* node = const_cast<node_type*>(it.m_node);
        node_type* ret = node->next();
        do_erase(node);
        return iterator(ret);
    }

    template <typename Callable>
    void modify(const T& elem, Callable&& func) {
        node_type* node = do_find<0>(elem);
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
        info_type* root = get_root_info<I>();
        if (root == nullptr)
            return sort_iterator<I>(nullptr);
        return sort_iterator<I>(tree_min(root)->m_node);
    }

    template <int I>
    sort_iterator<I> sort_end() const
    {
        return sort_iterator<I>(nullptr);
    }

    iterator iterator_to(const T& entry) const
    {
        // Note that this function is technically unsafe because we can't
        // Guarantee a 0 offset.
        // static_assert(offsetof(node_type, m_value) == 0);

        T& ref = const_cast<T&>(entry);
        node_type* node = reinterpret_cast<node_type*>(&ref);
        return iterator(node);
    }
};
