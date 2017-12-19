#ifndef SP_MALLOC_DYN_TREE_H
#define SP_MALLOC_DYN_TREE_H

#include <cassert>
#include <cstdint>
#include <tuple>
#include <utility>

#include <iostream>
#include <string> //debug

namespace sp {
template <typename T>
struct DNode {
  DNode<T> *left;
  DNode<T> *right;
  DNode<T> *parent;
  T value;
  std::int8_t balance;

  template <typename K>
  explicit DNode(K &&v, DNode<T> *p = nullptr)
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
};

namespace impl {
namespace dtree {
// BalanceFactor(N) := Height(RightSubtree(N)) - Height(LeftSubtree(N))
// BalanceFactor(N) = {–1, 0, +1}
//
// BalanceFactor holds for every node N in the tree.
// A node N with BalanceFactor(N) < 0 is called "left-heavy", one with
// BalanceFactor(N) > 0 is called "right-heavy", and one with BalanceFactor(N)
// = 0 is sometimes simply called "balanced".

enum class Direction : bool { LEFT, RIGHT };
// template <typename T>
// static std::int8_t
// balance(const DNode<T> *const node) noexcept {
//   assert(node);
//   return node->balance;
// }

template <typename T>
static Direction
direction(const DNode<T> *const child) noexcept {
  assert(child);
  DNode<T> *const parent = child->parent;
  assert(parent);

  if (parent->left == child) {
    return Direction::LEFT;
  }

  assert(parent->right == child);
  return Direction::RIGHT;
}

// template <typename T>
// static void
// balance_right_to_left(DNode<T> *node) noexcept {
//   DNode<T> *const parent = node->parent;
//
//   // DNode<T> *const lnode = node->left;
//   DNode<T> *const r_node = node->right;
//   assert(r_node);
//   DNode<T> *const rl_node = right->left;
//   assert(rl_node);
// }
//
// template <typename T>
// static void
// balance_left_to_right(DNode<T> *node) noexcept {
// }

// template <typename T>
// static void
// retrace(DNode<T> *node) noexcept {
// Lstart:
//   DNode<T> *const parent = node->parent;
//   if (parent) {
//     Direction d = direction(node);
//     if (d == Direction::LEFT) {
//       parent->balance--;
//     } else {
//       parent->balance++;
//     }
//
//     if (parent->balance > 1) {
//       balance_right_to_left(parent);
//     } else if (parent->balance < -1) {
//       balance_left_to_right(parent);
//     }
//     node = parent;
//     goto Lstart;
//   }
// }

template <typename T>
static std::int8_t
balance(const DNode<T> *const node) {
  return node ? node->balance : 0;
}

template <typename T>
static DNode<T> *
rotate_left(DNode<T> *const A) noexcept {
  printf("\trotate_left(%s)\n", std::string(*A).c_str());
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
  DNode<T> *const A_parent = A->parent;
  DNode<T> *const B = A->right;
  DNode<T> *const B_left = B->left;

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
static DNode<T> *
rotate_right(DNode<T> *const C) noexcept {
  printf("\trotate_right(%s)\n", std::string(*C).c_str());
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
  DNode<T> *const C_parent = C->parent;
  DNode<T> *const B = C->left;
  DNode<T> *const B_right = B->right;

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
// static DNode<T> *
// rotate_right_left(DNode<T> *const C) noexcept {
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
// static DNode<T> *
// rotate_left_right(DNode<T> *const C) noexcept {
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
calc_parent_balance(const DNode<T> *child) noexcept {
  DNode<T> *parent = child->parent;
  if (parent) {
    Direction d = direction(child);

    if (d == Direction::LEFT) {
      parent->balance--;
    } else {
      parent->balance++;
    }
  }
  return parent ? parent->balance : 0;
}

template <typename T>
static DNode<T> *&
set(DNode<T> *const root, DNode<T> *&child) noexcept {
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

template <typename T>
static DNode<T> *
retrace(DNode<T> *it) noexcept {
  DNode<T> *current = nullptr;
Lstart:
  if (it) {
    current = it;
    const std::int8_t b = balance(current);
    printf("\t===============\n");

    // Left heavy
    if (b == -2) {
      if (balance(current->left) == 1) {
        current->left = rotate_left(current->left);
        dump(current, "\t");
      }

      set(current->parent, current) = rotate_right(current);
    }
    // Right heavy
    else if (b == 2) {
      if (balance(current->right) == -1) {
        current->right = rotate_right(current->right);
        dump(current, "\t");
      }

      set(current->parent, current) = rotate_left(current);
    }

    if (calc_parent_balance(current) == 0) {

      return nullptr;
    }
    dump(current->parent, "\t");

    it = current->parent;
    goto Lstart;
  }

  return current;
}

template <typename T>
std::uint32_t
verify(DNode<T> *parent, DNode<T> *tree) noexcept {
  std::uint32_t result = 0;
  if (tree) {
    result += 1;
    assert(tree->parent == parent);

    std::uint32_t left = 0;
    if (tree->left) {
      assert(tree->value > tree->left->value);
      left += verify(tree, tree->left);
    }

    std::uint32_t right = 0;
    if (tree->right) {
      assert(tree->value < tree->right->value);
      right += verify(tree, tree->right);
    }

    std::int64_t bl = std::int64_t(right) - std::int64_t(left);
    std::int8_t b = bl;
    if (tree->balance != b) {
      std::cout << "right: " << right << "|";
      std::cout << "left: " << left << "|";
      // std::cout << "bl: " << bl << "|";
      std::cout << "b: " << int(b) << "|";
      std::cout << "tree->balance: " << int(tree->balance) << "|";
      std::cout << "\n";
    }

    assert(bl == b);
    assert(tree->balance == b);

    result += right;
    result += left;
  }
  return result;
}

} // namespace dtree
} // namespace impl

template <typename T>
void
dump(sp::Tree<sp::DNode<T>> &tree) noexcept {
  return impl::dtree::dump(tree.root);
}

template <typename T>
void
verify(sp::Tree<sp::DNode<T>> &tree) noexcept {
  impl::dtree::verify((DNode<T> *)nullptr, tree.root);
}

template <typename T, typename K>
std::tuple<T *, bool>
insert(sp::Tree<sp::DNode<T>> &tree, K &&ins) noexcept {
  /*Ordinary Binary Insert*/

  auto set_root = [&](DNode<T> *n) {
    if (n) {
      tree.root = n;
    }
  };

  if (!tree.root) {
    /*Insert into empty tree*/
    tree.root = new (std::nothrow) DNode<T>(std::forward<T>(ins));
    if (tree.root) {
      return std::make_tuple(&tree.root->value, true);
    }

    return std::make_tuple(nullptr, false);
  }

  DNode<T> *it = tree.root;

Lstart:
  if (ins < it->value) {
    if (it->left) {
      it = it->left;

      goto Lstart;
    }

    it->left = new (std::nothrow) DNode<T>(std::forward<T>(ins), it);
    if (it->left) {
      set_root(impl::dtree::retrace(it->left));

      return std::make_tuple(&it->left->value, true);
    }
  } else if (ins > it->value) {
    if (it->right) {
      it = it->right;

      goto Lstart;
    }

    it->right = new (std::nothrow) DNode<T>(std::forward<T>(ins), it);
    if (it->right) {
      set_root(impl::dtree::retrace(it->right));

      return std::make_tuple(&it->right->value, true);
    }
  } else {

    return std::make_tuple(&it->value, false);
  }

  return std::make_tuple(nullptr, false);
}

} // namespace sp

#endif
