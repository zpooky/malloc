#ifndef SP_MALLOC_AVL_TREE_H
#define SP_MALLOC_AVL_TREE_H

#include "tree.h"
#include <cassert>
#include <cstdint>
#include <tuple>
#include <utility>

#include <iostream>
#include <string> //debug

// TODO noexcept operator

namespace avl {
template <typename T>
struct Node {
  using value_type = T;

  // TODO lesser
  Node<T> *left;
  // TODO greater
  Node<T> *right;
  Node<T> *parent;
  T value;
  std::int8_t balance;

  template <typename K>
  explicit Node(K &&v, Node<T> *p = nullptr)
      : left(nullptr)
      , right(nullptr)
      , parent(p)
      , value(std::forward<K>(v))
      , balance(0) {
  }

  explicit operator std::string() const {
    std::string s;
    s.append("[v:");
    s.append(std::to_string(int(value)));
    s.append("|b:");
    s.append(std::to_string(balance));
    s.append("]");
    return s;
  }

  template <typename O>
  bool
  operator<(const O &o) const noexcept {
    return value < o;
  }

  template <typename O>
  bool
  operator>(const O &o) const noexcept {
    return value > o;
  }

  template <typename O>
  bool
  operator==(const O &o) const noexcept {
    return value == o;
  }

  ~Node() noexcept {
    // TODO this is recursive
    if (left) {
      delete left;
      left = nullptr;
    }
    if (right) {
      delete right;
      right = nullptr;
    }
  }
};

template <typename T>
using Tree = sp::Tree<avl::Node<T>>;

template <typename T>
void
dump(Tree<T> &tree, std::string prefix = "") noexcept;

template <typename T>
void
verify(Tree<T> &tree) noexcept;

template <typename T, typename K>
std::tuple<T *, bool>
insert(Tree<T> &, K &&) noexcept;

template <typename T, typename K>
bool
remove(Tree<T> &, const K &) noexcept;

namespace impl {
namespace avl {
// BalanceFactor(N) := Height(RightSubtree(N)) - Height(LeftSubtree(N))
// BalanceFactor(N) = {–1, 0, +1}
//
// BalanceFactor holds for every node N in the tree.
// A node N with BalanceFactor(N) < 0 is called "left-heavy", one with
// BalanceFactor(N) > 0 is called "right-heavy", and one with BalanceFactor(N)
// = 0 is sometimes simply called "balanced".

enum class Direction : bool { LEFT, RIGHT };

template <typename T>
static Direction
direction(const Node<T> *const child) noexcept {
  assert(child);
  Node<T> *const parent = child->parent;
  assert(parent);

  if (parent->left == child) {
    return Direction::LEFT;
  }

  assert(parent->right == child);
  return Direction::RIGHT;
}

template <typename T>
void
dump_root(Node<T> *tree, std::string prefix = "") noexcept {
  Node<T> *root = tree;
Lstart:
  if (root->parent) {
    root = root->parent;
    goto Lstart;
  }
  return sp::impl::tree::dump(root, prefix);
}

template <typename T>
static std::int8_t
balance(const Node<T> *const node) noexcept {
  return node ? node->balance : 0;
}

template <typename T>
static Node<T> *
rotate_left(Node<T> *const A) noexcept {
  // printf("\trotate_left(%s)\n", std::string(*A).c_str());
  // dump_root(A, "\t");
  /*
   * <_
   *   \
   * __/
   *
   * C:3, B:2, A:1
   *
   * A                           B
   *  \           L(1)          / \
   *   B          ---->        A   C
   *  / \                       \
   * x1  C                       x1
   */
  Node<T> *const A_parent = A->parent;
  Node<T> *const B = A->right;
  Node<T> *const B_left = B ? B->left : nullptr; // nullptr

  //#Rotate
  A->parent = B;
  A->right = B_left;

  if (B_left) {
    B_left->parent = A;
  }

  if (B) {
    B->parent = A_parent;
    B->left = A;
  }

  //#Update Balance
  /*We do not rebalance C since its children has not been altered*/

  A->balance -= 1;
  if (balance(B) > 0) {
    A->balance -= B->balance;
  }
  if (B) {
    B->balance -= 1;
    if (balance(A) < 0) {
      B->balance -= -A->balance;
    }
  }

  return B ? B : A;
}

template <typename T>
static Node<T> *
rotate_right(Node<T> *const C) noexcept {
  // printf("\trotate_right(%s)\n", std::string(*C).c_str());
  // dump_root(C, "\t");
  /*
  * _.
  *   \
  * <-´
  *
  * C:3, B:2, A:1
  *
  *     C                         B
  *    /           R(3)          / \
  *   B            ---->        A   C
  *  / \                           /
  * A   x1                        x1
  */
  Node<T> *const C_parent = C->parent;
  Node<T> *const B = C->left;
  Node<T> *const B_right = B ? B->right : nullptr;

  //#Rotate
  C->parent = B;
  C->left = B_right;

  if (B_right) {
    B_right->parent = C;
  }

  if (B) {
    B->parent = C_parent;
    B->right = C;
  }

  // x = C
  // y = B
  C->balance += 1;
  if (balance(B) < 0) {
    C->balance += -B->balance;
  }

  if (B) {
    B->balance += 1;
    if (balance(C) > 0) {
      B->balance += C->balance;
    }
  }

  return B;
}

template <typename T>
static std::size_t
remove_parent_balance(Node<T> *const child) noexcept {
  Node<T> *parent = child->parent;
  Direction d = direction(child);

  if (d == Direction::LEFT) {
    parent->balance++;
  } else {
    parent->balance--;
  }
  return parent->balance;
}

template <typename T>
static std::size_t
insert_parent_balance(Node<T> *const child) noexcept {
  Node<T> *parent = child->parent;
  Direction d = direction(child);

  if (d == Direction::LEFT) {
    parent->balance--;
  } else {
    parent->balance++;
  }
  return parent->balance;
}

template <typename T>
static Node<T> *&
set(Node<T> *&child) noexcept {
  assert(child);

  Node<T> *const parent = child->parent;

  if (!parent) {
    return child;
  }

  if (parent->left == child) {
    return parent->left;
  }

  assert(parent->right == child);
  return parent->right;
}

// - The retracing can stop if the balance factor becomes 0 implying that the
//   height of that subtree remains unchanged.
// - If balance factor becomes -1 or +1 continue retraceing
// - If balance factor becomes -2 or +2 we need to repair.
//   After which the subtree has the same height as before
template <typename T, typename F>
static Node<T> *
retrace(Node<T> *it, F parent_balance) noexcept {
  Node<T> *current = nullptr;
Lstart:
  if (it) {
    current = it;

    /* Left Heavy */
    if (balance(current) == -2) {
      if (balance(current->left) == 1) {
        current->left = rotate_left(current->left);
      }

      // update parent with new child
      set(current) = rotate_right(current);

      // if there is _no_ parent then current is root.
      // if there are a parent then we have not altered the root node.
      return current->parent ? nullptr : current;
    }
    /* Right Heavy */
    else if (balance(current) == 2) {
      if (balance(current->right) == -1) {
        current->right = rotate_right(current->right);
      }

      set(current) = rotate_left(current);

      return current->parent ? nullptr : current;
    }

    if (current->parent) {
      if (parent_balance(current) == 0) {
        return nullptr;
      }
    }

    it = current->parent;
    goto Lstart;
  }

  return current;
} // avl::impl::retrace()

template <typename T>
std::uint32_t
verify(const Node<T> *parent, const Node<T> *tree) noexcept {
  std::uint32_t result = 0;
  if (tree) {
    assert(tree->parent == parent);

    std::uint32_t left = 0;
    if (tree->left) {
      assert(tree->value > tree->left->value);
      left = verify(tree, tree->left);
    }

    std::uint32_t right = 0;
    if (tree->right) {
      assert(tree->value < tree->right->value);
      right = verify(tree, tree->right);
    }

    result++;

    std::int64_t bl = std::int64_t(right) - std::int64_t(left);
    std::int8_t b = bl;
    if (tree->balance != b) {
      std::cout << "right: " << right << "|";
      std::cout << "left: " << left << "|";
      // std::cout << "bl: " << bl << "|";
      std::cout << "b: " << int(b) << "|";
      std::cout << "tree: " << std::string(*tree) << "|";
      std::cout << "\n";
    }

    assert(bl == b);
    if (tree->balance != b) {
      assert(tree->balance == b);
    }

    assert(!(tree->balance > 1));
    assert(!(tree->balance < -1));

    result += std::max(left, right);
  }
  return result;
} // avl::impl::verify

template <typename T>
void
exchange(Node<T> *node, Node<T> *n) noexcept {
  auto *parent = node->parent;
  if (parent) {
    if (parent->left == node) {
      parent->left = n;
    }
    if (parent->right != node) {
      assert(parent->right == node);
    }
    parent->right = n;
  }

  if (n) {
    n->parent = parent;

    n->left = node->left;
    if (n->left) {
      n->left->parent = n;
    }

    n->right = node->right;
    if (n->right) {
      n->right->parent = n;
    }
  }
} // avl::impl::exchange()

/*
 * Delete:
 * - a node with no children: simply remove the node from the tree.
 * - a node with one child: remove the node and replace it with its child.
 * - a node with two children:
 *   call the node to be deleted D.
 *   Do not delete D. Instead, choose either its in-order predecessor node or
 *   its in-order successor node as replacement node E (s. figure). Copy the
 *   user values of E to D.[note 2] If E does not have a child simply remove E
 *   from its previous parent G. If E has a child, say F, it is a right child.
 *   Replace E with F at E's parent.
 */

template <typename T>
void
nop(Node<T> *) noexcept {
} // avl::impl::nop()

template <typename T>
/*new root*/ Node<T> *
remove(Node<T> *const current, void (*cleanup)(Node<T> *)) noexcept {
  if (current->left && current->right) {
    // two children

    /*
     * find the smallest value in the _right_ (greater) branch which will be
     * the replacement for the removed node
     */
    Node<T> *const successor = sp::impl::tree::find_min(current->right);
    Node<T> *const new_root = remove(successor, nop);
    exchange(current, successor);
    cleanup(current);
    /*     X
     *    / \
     * min   max
     *    \
     *    A
     * take min and move it to current position
     */
    return new_root;
  } else if (!current->left && !current->right) {
    // zero children

    exchange(current, (Node<T> *)nullptr);
    cleanup(current);
    Node<T> *const parent = current->parent;
    if (parent) {
      parent->balance--;

      return retrace(parent, [](Node<T> *child) { //
        return remove_parent_balance(child);
      });
    }

    // we removed the last node in the tree
    return nullptr;
  } else if (current->left) {
    // one child

    auto *const left = current->left;
    exchange(current, left);
    cleanup(current);

    return retrace(left, [](Node<T> *child) { //
      return remove_parent_balance(child);
    });
  } else if (/*current->right*/ true) {
    // one child

    auto *const right = current->right;
    exchange(current, right);
    cleanup(current);

    return retrace(right, [](Node<T> *child) { //
      return remove_parent_balance(child);
    });
  }
} // avl::impl::remove()

} // namespace avl
} // namespace impl

template <typename T>
void
dump(Tree<T> &tree, std::string prefix) noexcept {
  return sp::impl::tree::dump(tree.root, prefix);
}

template <typename T>
void
verify(Tree<T> &tree) noexcept {
  impl::avl::verify((Node<T> *)nullptr, tree.root);
}

template <typename T, typename K>
std::tuple<T *, bool>
insert(Tree<T> &tree, K &&ins) noexcept {
  /*Ordinary Binary Insert*/
  auto set_root = [&tree](Node<T> *new_root) {
    if (new_root) {
      tree.root = new_root;
    }
  };

  if (!tree.root) {
    /*Insert into empty tree*/
    tree.root = new (std::nothrow) Node<T>(std::forward<T>(ins));
    if (tree.root) {
      return std::make_tuple(&tree.root->value, true);
    }

    return std::make_tuple(nullptr, false);
  }

  // TODO share with bst
  Node<T> *it = tree.root;

Lstart:
  if (ins < it->value) {
    if (it->left) {
      it = it->left;

      goto Lstart;
    }

    auto res = it->left = new (std::nothrow) Node<T>(std::forward<K>(ins), it);
    if (it->left) {
      set_root(impl::avl::retrace(it->left, [](Node<T> *child) {
        return impl::avl::insert_parent_balance(child);
      }));

      return std::make_tuple(&res->value, true);
    }
  } else if (ins > it->value) {
    if (it->right) {
      it = it->right;

      goto Lstart;
    }

    auto res = it->right = new (std::nothrow) Node<T>(std::forward<K>(ins), it);
    if (it->right) {
      set_root(impl::avl::retrace(it->right, [](Node<T> *child) {
        return impl::avl::insert_parent_balance(child);
      }));

      return std::make_tuple(&res->value, true);
    }
  } else {

    return std::make_tuple(&it->value, false);
  }

  return std::make_tuple(nullptr, false);
}

template <typename T>
static void
clean(Node<T> *n) noexcept { // TODO move to impl
  assert(n);
  delete n;
}

template <typename T, typename K>
bool
remove(Tree<T> &tree, const K &k) noexcept {
  Node<T> *const node = sp::impl::tree::find_node(tree.root, k);
  if (node) {
    // auto cleanup = [](Node<T> *n) {};
    Node<T> *const new_root = impl::avl::remove(node, clean);

    if (new_root) {
      tree.root = new_root;
    } else {
      if (tree.root == node) {
        tree.root = nullptr;
      }
    }

    return true;
  }

  return false;
}

} // namespace avl

#endif
