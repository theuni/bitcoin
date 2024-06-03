#include <array>
#include <list>
#include <functional>

template <typename T, typename Compare0, typename Compare1, typename Compare2>
class Tree
{
    struct Node
    {
        Node(T element) : m_element(std::move(element)){}
        struct Position {
            Node* m_parent{nullptr};
            Node* m_left{nullptr};
            Node* m_right{nullptr};
            bool m_black{false};
        };
        using Positions = std::array<Position, 3>;
        Positions m_positions{};
        T m_element;
    };

    using NodeList = std::list<Node>;
    using NodeRoots = std::array<Node*, 3>;
    NodeList m_nodes;
    NodeRoots m_roots{};

    template <int I>
    void left_rotate(Node* x) {
        if (x == nullptr || x->m_positions[I].m_right == nullptr)
            return;

        Node* y = x->m_positions[I].m_right;
        x->m_positions[I].m_right = y->m_positions[I].m_left;

        if (y->m_positions[I].m_left != nullptr)
            y->m_positions[I].m_left->m_positions[I].m_parent = x;

        y->m_positions[I].m_parent = x->m_positions[I].m_parent;

        if (x->m_positions[I].m_parent == nullptr)
            m_roots[I] = y;
        else if (x == x->m_positions[I].m_parent->m_positions[I].m_left)
            x->m_positions[I].m_parent->m_positions[I].m_left = y;
        else
            x->m_positions[I].m_parent->m_positions[I].m_right = y;

        y->m_positions[I].m_left = x;
        x->m_positions[I].m_parent = y;
    }

    template <int I>
    void right_rotate(Node* y) {
        if (y == nullptr || y->m_positions[I].m_left == nullptr)
            return;

        Node* x = y->m_positions[I].m_left;
        y->m_positions[I].m_left = x->m_positions[I].m_right;
        if (x->m_positions[I].m_right != nullptr)
            x->m_positions[I].m_right->m_positions[I].m_parent = y;
        x->m_positions[I].m_parent = y->m_positions[I].m_parent;
        if (y->m_positions[I].m_parent == nullptr)
            m_roots[I] = x;
        else if (y == y->m_positions[I].m_parent->m_positions[I].m_left)
            y->m_positions[I].m_parent->m_positions[I].m_left = x;
        else
            y->m_positions[I].m_parent->m_positions[I].m_right = x;
        x->m_positions[I].m_right = y;
        y->m_positions[I].m_parent = x;
    }

    template <int I>
    void fix_insert(Node* z) {
        while (z != m_roots[I] && !z->m_positions[I].m_parent->m_positions[I].m_black) {
            if (z->m_positions[I].m_parent == z->m_positions[I].m_parent->m_positions[I].m_parent->m_positions[I].m_left) {
                Node* y = z->m_positions[I].m_parent->m_positions[I].m_parent->m_positions[I].m_right;
                if (y != nullptr && !y->m_positions[I].m_black) {
                    z->m_positions[I].m_parent->m_positions[I].m_black = true;
                    y->m_positions[I].m_black = true;
                    z->m_positions[I].m_parent->m_positions[I].m_parent->m_positions[I].m_black = false;
                    z = z->m_positions[I].m_parent->m_positions[I].m_parent;
                } else {
                    if (z == z->m_positions[I].m_parent->m_positions[I].m_right) {
                        z = z->m_positions[I].m_parent;
                        left_rotate<I>(z);
                    }
                    z->m_positions[I].m_parent->m_positions[I].m_black = true;
                    z->m_positions[I].m_parent->m_positions[I].m_parent->m_positions[I].m_black = false;
                    right_rotate<I>(z->m_positions[I].m_parent->m_positions[I].m_parent);
                }
            } else {
                Node* y = z->m_positions[I].m_parent->m_positions[I].m_parent->m_positions[I].m_left;
                if (y != nullptr && !y->m_positions[I].m_black) {
                    z->m_positions[I].m_parent->m_positions[I].m_black = true;
                    y->m_positions[I].m_black = true;
                    z->m_positions[I].m_parent->m_positions[I].m_parent->m_positions[I].m_black = false;
                    z = z->m_positions[I].m_parent->m_positions[I].m_parent;
                } else {
                    if (z == z->m_positions[I].m_parent->m_positions[I].m_left) {
                        z = z->m_positions[I].m_parent;
                        right_rotate<I>(z);
                    }
                    z->m_positions[I].m_parent->m_positions[I].m_black = true;
                    z->m_positions[I].m_parent->m_positions[I].m_parent->m_positions[I].m_black = false;
                    left_rotate<I>(z->m_positions[I].m_parent->m_positions[I].m_parent);
                }
            }
        }
        m_roots[I]->m_positions[I].m_black = true;
    }

    template <int I>
    void transplant(Node* u, Node* v) {
        if (u->m_positions[I].m_parent == nullptr)
            m_roots[I] = v;
        else if (u == u->m_positions[I].m_parent->m_positions[I].m_left)
            u->m_positions[I].m_parent->m_positions[I].m_left = v;
        else
            u->m_positions[I].m_parent->m_positions[I].m_right = v;
        if (v != nullptr)
            v->m_positions[I].m_parent = u->m_positions[I].m_parent;
    }

    template <int I>
    Node* minimum(Node* node) {
        while (node->m_positions[I].m_left != nullptr)
            node = node->m_positions[I].m_left;
        return node;
    }

    template <int I>
    void fix_delete(Node* x) {
        while (x != m_roots[I] && x != nullptr && x->m_positions[I].m_black) {
            if (x == x->m_positions[I].m_parent->m_positions[I].m_left) {
                Node* w = x->m_positions[I].m_parent->m_positions[I].m_right;
                if (!w->m_positions[I].m_black) {
                    w->m_positions[I].m_black = true;
                    x->m_positions[I].m_parent->m_positions[I].m_black = false;
                    left_rotate<I>(x->m_positions[I].m_parent);
                    w = x->m_positions[I].m_parent->m_positions[I].m_right;
                }
                if ((w->m_positions[I].m_left == nullptr || w->m_positions[I].m_left->m_positions[I].m_black) &&
                    (w->m_positions[I].m_right == nullptr || w->m_positions[I].m_right->m_positions[I].m_black)) {
                    w->m_positions[I].m_black = false;
                    x = x->m_positions[I].m_parent;
                } else {
                    if (w->m_positions[I].m_right == nullptr || w->m_positions[I].m_right->m_positions[I].m_black) {
                        if (w->m_positions[I].m_left != nullptr)
                            w->m_positions[I].m_left->m_positions[I].m_black = true;
                        w->m_positions[I].m_black = false;
                        right_rotate<I>(w);
                        w = x->m_positions[I].m_parent->m_positions[I].m_right;
                    }
                    w->m_positions[I].m_black = x->m_positions[I].m_parent->m_positions[I].m_black;
                    x->m_positions[I].m_parent->m_positions[I].m_black = true;
                    if (w->m_positions[I].m_right != nullptr)
                        w->m_positions[I].m_right->m_positions[I].m_black = true;
                    left_rotate<I>(x->m_positions[I].m_parent);
                    x = m_roots[I];
                }
            } else {
                Node* w = x->m_positions[I].m_parent->m_positions[I].m_left;
                if (!w->m_positions[I].m_black) {
                    w->m_positions[I].m_black = true;
                    x->m_positions[I].m_parent->m_positions[I].m_black = false;
                    right_rotate<I>(x->m_positions[I].m_parent);
                    w = x->m_positions[I].m_parent->m_positions[I].m_left;
                }
                if ((w->m_positions[I].m_right == nullptr || w->m_positions[I].m_right->m_positions[I].m_black) &&
                    (w->m_positions[I].m_left == nullptr || w->m_positions[I].m_left->m_positions[I].m_black)) {
                    w->m_positions[I].m_black = false;
                    x = x->m_positions[I].m_parent;
                } else {
                    if (w->m_positions[I].m_left == nullptr || w->m_positions[I].m_left->m_positions[I].m_black) {
                        if (w->m_positions[I].m_right != nullptr)
                            w->m_positions[I].m_right->m_positions[I].m_black = true;
                        w->m_positions[I].m_black = false;
                        left_rotate<I>(w);
                        w = x->m_positions[I].m_parent->m_positions[I].m_left;
                    }
                    w->m_positions[I].m_black = x->m_positions[I].m_parent->m_positions[I].m_black;
                    x->m_positions[I].m_parent->m_positions[I].m_black = true;
                    if (w->m_positions[I].m_left != nullptr)
                        w->m_positions[I].m_left->m_positions[I].m_black = true;
                    right_rotate<I>(x->m_positions[I].m_parent);
                    x = m_roots[I];
                }
            }
        }
        if (x != nullptr)
            x->m_positions[I].m_black = true;
    }

    template <int I>
    void delete_node(Node* z) {
        if (z == nullptr)
            return;

        Node* y = z;
        Node* x = nullptr;
        bool y_was_black = y->m_positions[I].m_black;

        if (z->m_positions[I].m_left == nullptr) {
            x = z->m_positions[I].m_right;
            transplant<I>(z, z->m_positions[I].m_right);
        } else if (z->m_positions[I].m_right == nullptr) {
            x = z->m_positions[I].m_left;
            transplant<I>(z, z->m_positions[I].m_left);
        } else {
            y = minimum<I>(z->m_positions[I].m_right);
            y_was_black = y->m_positions[I].m_black;
            x = y->m_positions[I].m_right;

            if (y->m_positions[I].m_parent == z) {
                if (x != nullptr)
                    x->m_positions[I].m_parent = y; // Check if x is not nullptr before assigning parent
            } else {
                if (x != nullptr)
                    x->m_positions[I].m_parent = y->m_positions[I].m_parent; // Check if x and y->m_positions[I].m_parent are not nullptr before assigning parent
                transplant<I>(y, y->m_positions[I].m_right);
                if (y->m_positions[I].m_right != nullptr)
                    y->m_positions[I].m_right->m_positions[I].m_parent = y; // Check if y->m_positions[I].m_right is not nullptr before assigning parent
                y->m_positions[I].m_right = z->m_positions[I].m_right;
                if (y->m_positions[I].m_right != nullptr)
                    y->m_positions[I].m_right->m_positions[I].m_parent = y; // Check if y->m_positions[I].m_right is not nullptr before assigning parent
            }
            transplant<I>(z, y);
            y->m_positions[I].m_left = z->m_positions[I].m_left;
            if (y->m_positions[I].m_left != nullptr)
                y->m_positions[I].m_left->m_positions[I].m_parent = y; // Check if y->m_positions[I].m_left is not nullptr before assigning parent
            y->m_positions[I].m_black = z->m_positions[I].m_black;
        }

        if (y_was_black && x != nullptr) // Check if x is not nullptr
            fix_delete<I>(x);
    }

    template <int I, typename Compare>
    void remove_one(Node* node)
    {
        Node* z = m_roots[I];
        while (z != nullptr) {
            if (Compare()(node->m_element, z->m_element))
                z = z->m_positions[I].m_left;
            else if (Compare()(z->m_element, node->m_element))
                z = z->m_positions[I].m_right;
            else {
                delete_node<I>(z);
                return;
            }
        }
    }

    template <int I, typename Compare>
    void insert_one(Node& node)
    {
        Node* y = nullptr;
        Node* x = m_roots[I];
        while (x != nullptr) {
            y = x;
            if (Compare()(node.m_element, x->m_element))
                x = x->m_positions[I].m_left;
            else
                x = x->m_positions[I].m_right;
        }
        node.m_positions[I].m_parent = y;
        if (y == nullptr)
            m_roots[I] = &node;
        else if (Compare()(node.m_element, y->m_element))
            y->m_positions[I].m_left = &node;
        else
            y->m_positions[I].m_right = &node;
        fix_insert<I>(&node);
    }

public:
    typename std::list<Node>::iterator insert(T element)
    {
        auto it = m_nodes.insert(m_nodes.end(), std::move(element));
        insert_one<0, Compare0>(*it);
        insert_one<1, Compare1>(*it);
        insert_one<2, Compare2>(*it);
        return it;
    }

    void remove(typename std::list<Node>::iterator it)
    {
        remove_one<0, Compare0>(&*it);
        remove_one<1, Compare1>(&*it);
        remove_one<2, Compare2>(&*it);
        m_nodes.erase(it);
    }
};

struct Transaction
{
    std::array<unsigned char, 32> m_txid{0};
    std::array<unsigned char, 32> m_wtxid{0};
    bool operator<(const Transaction& rhs) const { return m_txid < rhs.m_txid || m_wtxid < rhs.m_wtxid; }
};

int main()
{
    Tree<Transaction, std::less<Transaction>, std::less<Transaction>, std::less<Transaction>> tree;
    Transaction foo{};
    auto it = tree.insert(foo);
    foo.m_txid = {1};
    auto it2 = tree.insert(foo);
    foo.m_wtxid = {2};
    auto it3 = tree.insert(foo);
    tree.remove(it);
    tree.remove(it2);
    tree.remove(it3);
}
