#ifndef SP_MALLOC_AVL_H
#define SP_MALLOC_AVL_H

#include "tree.h"
#include <cassert>
#include <cstdint>
#include <tuple>
#include <utility>

#include <iostream>
#include <string> //debug

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

  ~Node() {
    // this is recursive
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
dump(Node<T> *tree, std::string prefix = "", bool isTail = true,
     const char *ctx = "") noexcept {
  if (tree) {
    char name[256] = {0};
    auto val = std::string(*tree);
    sprintf(name, "%s%s", ctx, val.c_str());

    printf("%s%s%s\n", prefix.c_str(), (isTail ? "└── " : "├── "), name);

    const char *ls = (isTail ? "    " : "│   ");
    if (tree->right && tree->left) {
      dump(tree->right, prefix + ls, false, "gt:");
      dump(tree->left, prefix + ls, true, "lt:");
    } else {
      if (tree->left) {
        dump(tree->left, prefix + ls, true, "lt:");
      } else if (tree->right) {
        dump(tree->right, prefix + ls, true, "gt:");
      }
    }
  }
}

template <typename T>
static std::int8_t
balance(const Node<T> *const node) {
  return node ? node->balance : 0;
}

template <typename T>
static Node<T> *
rotate_left(Node<T> *const A) noexcept {
  // printf("\trotate_left(%s)\n", std::string(*A).c_str());
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
  std::int8_t root_balance = A->balance;
  Node<T> *const A_parent = A->parent;
  Node<T> *const B = A->right;
  Node<T> *const B_left = B->left;

  //#Rotate
  A->parent = B;
  A->right = B_left;

  if (B_left) {
    B_left->parent = A;
  }

  B->parent = A_parent;
  B->left = A;

  // x =A
  // y = B
  //#Update Balance
  /*We do not rebalance C since its children has not been altered*/

  A->balance -= 1;
  if (B->balance > 0) {
    A->balance -= B->balance;
  }
  B->balance -= 1;
  if (A->balance < 0) {
    B->balance -= -A->balance;
  }

  // if (root_balance == 0) {
  //   //?? since root balance is 0 we know all its children is the same??
  //   A->balance = 1;
  //   B->balance = -1;
  // } else {
  //   A->balance = 0;
  //   B->balance = 0;
  // }

  return B;
}

template <typename T>
static Node<T> *
rotate_right(Node<T> *const C) noexcept {
  // printf("\trotate_right(%s)\n", std::string(*C).c_str());
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
  std::int8_t root_balance = C->balance;
  Node<T> *const C_parent = C->parent;
  Node<T> *const B = C->left;
  Node<T> *const B_right = B->right;

  //#Rotate
  C->parent = B;
  C->left = B_right;

  if (B_right) {
    B_right->parent = C;
  }

  B->parent = C_parent;
  B->right = C;

  //#Update Balance
  // if (root_balance == 0) {
  //   B->balance = 1;
  //   C->balance = -1;
  // } else {
  //   B->balance = 0;
  //   C->balance = 0;
  // }

  // x = C
  // y = B
  C->balance += 1;
  if (B->balance < 0) {
    C->balance += -B->balance;
  }
  B->balance += 1;
  if (C->balance > 0) {
    B->balance += C->balance;
  }

  return B;
}

// template <typename T>
// static Node<T> *
// rotate_right_left(Node<T> *const C) noexcept {
//   #<{(|
//   *   3                       3                    2
//   *  /         L(1)          /        R(3)        / \
//   * 1          ---->        2         ---->      1   3
//   *  \                     /
//   *   2                   1
//   |)}>#
// }
//
// template <typename T>
// static Node<T> *
// rotate_left_right(Node<T> *const C) noexcept {
//   #<{(|
//   * 1                   1                        2
//   *  \       R(3)        \          L(1)        / \
//   *   3      ---->        2         ---->      1   3
//   *  /                     \
//   * 2                       3
//   |)}>#
// }

template <typename T>
static std::size_t
calc_parent_balance(const Node<T> *child) noexcept {
  Node<T> *parent = child->parent;
  if (parent) {
    Direction d = direction(child);

    if (d == Direction::LEFT) {
      if (parent->right == nullptr) {
        parent->balance--;
      }
    } else {
      if (parent->left == nullptr) {
        parent->balance++;
      }
    }
  }
  return parent ? parent->balance : 0;
}

template <typename T>
static Node<T> *&
set(Node<T> *const root, Node<T> *&child) noexcept {
  assert(child);

  if (!root) {
    return child;
  }

  if (root->left == child) {
    return root->left;
  }

  assert(root->right == child);
  return root->right;
}

// - The retracing can stop if the balance factor becomes 0 implying that the
//   height of that subtree remains unchanged.
// - If balance factor becomes -1 or +2 continue retraceing
// - If balance factor becomes -2 or +2 we need to repair
template <typename T>
static Node<T> *
retrace(Node<T> *it) noexcept {
  Node<T> *current = nullptr;
Lstart:
  if (it) {
    current = it;
    const std::int8_t b = balance(current);
    // printf("\t===============\n");

    // Left heavy
    if (b == -2) {
      if (balance(current->left) == 1) {
        current->left = rotate_left(current->left);
        // dump(current, "\t");
      }

      set(current->parent, current) = rotate_right(current);
    }
    // Right heavy
    else if (b == 2) {
      if (balance(current->right) == -1) {
        current->right = rotate_right(current->right);
        // dump(current, "\t");
      }

      set(current->parent, current) = rotate_left(current);
    }

    if (calc_parent_balance(current) == 0) {
      // return nullptr;
    }
    // dump(current->parent, "\t");

    it = current->parent;
    goto Lstart;
  }

  return current;
}

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

    // TODO make work
    std::int64_t bl = std::int64_t(right) - std::int64_t(left);
    std::int8_t b = bl;
    // if (tree->balance != b) {
    //   std::cout << "right: " << right << "|";
    //   std::cout << "left: " << left << "|";
    //   // std::cout << "bl: " << bl << "|";
    //   std::cout << "b: " << int(b) << "|";
    //   std::cout << "tree: " << std::string(*tree) << "|";
    //   std::cout << "\n";
    // }

    // assert(bl == b);
    // assert(tree->balance == b);

    result += std::max(left, right);
  }
  return result;
}

} // namespace avl
} // namespace impl

template <typename T>
void
dump(sp::Tree<avl::Node<T>> &tree) noexcept {
  return impl::avl::dump(tree.root);
}

template <typename T>
void
verify(sp::Tree<avl::Node<T>> &tree) noexcept {
  impl::avl::verify((Node<T> *)nullptr, tree.root);
}

template <typename T, typename K>
std::tuple<T *, bool>
insert(sp::Tree<avl::Node<T>> &tree, K &&ins) noexcept {
  /*Ordinary Binary Insert*/

  auto set_root = [&](Node<T> *n) {
    if (n) {
      tree.root = n;
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

  Node<T> *it = tree.root;

Lstart:
  if (ins < it->value) {
    if (it->left) {
      it = it->left;

      goto Lstart;
    }

    it->left = new (std::nothrow) Node<T>(std::forward<T>(ins), it);
    if (it->left) {
      set_root(impl::avl::retrace(it->left));

      return std::make_tuple(&it->left->value, true);
    }
  } else if (ins > it->value) {
    if (it->right) {
      it = it->right;

      goto Lstart;
    }

    it->right = new (std::nothrow) Node<T>(std::forward<T>(ins), it);
    if (it->right) {
      set_root(impl::avl::retrace(it->right));

      return std::make_tuple(&it->right->value, true);
    }
  } else {

    return std::make_tuple(&it->value, false);
  }

  return std::make_tuple(nullptr, false);
}

} // namespace avl

#endif
