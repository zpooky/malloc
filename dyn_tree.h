#ifndef SP_MALLOC_DYN_TREE_H
#define SP_MALLOC_DYN_TREE_H

#include <cassert>
#include <cstdint>
#include <tuple>
#include <utility>

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
};

template <typename T>
struct DTree {
  DNode<T> *root;

  DTree()
      : root(nullptr) {
  }

  ~DTree() {
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
  return node ? node->balance : -1;
}

template <typename T>
static DNode<T> *
rotate_left(DNode<T> *const A) noexcept {
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
  DNode<T> *const A_parent = A->parent;
  DNode<T> *const B = A->right;
  DNode<T> *const B_left = B->left;

  // Rotate
  A->parent = B;
  A->right = B_left;

  if (B_left) {
    B_left->parent = A;
  }

  B->parent = A_parent;
  B->left = A;
  // TODO recalculate balance
  // {
  //   if (B->balance == 0) {
  //     A->balance = 1;
  //     B->balance = -1;
  //   } else {
  //     A->balance = 0;
  //     B->balance = 0;
  //   }
  // }

  return B;
}

template <typename T>
static DNode<T> *
rotate_right(DNode<T> *const C) noexcept {
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
  DNode<T> *const C_parent = C->parent;
  DNode<T> *const B = C->left;
  DNode<T> *const B_right = B->right;

  // Rotate
  C->parent = B;
  C->left = B_right;

  if (B_right) {
    B_right->parent = C;
  }

  B->parent = C_parent;
  B->right = C;
  // TODO recalculate balance

  return B;
}

template <typename T>
static DNode<T> *
rotate_right_left(DNode<T> *const C) noexcept {
  /*
  *   3                       3                    2
  *  /         L(1)          /        R(3)        / \
  * 1          ---->        2         ---->      1   3
  *  \                     /
  *   2                   1
  */
}

template <typename T>
static DNode<T> *
rotate_left_right(DNode<T> *const C) noexcept {
  /*
  * 1                   1                        2
  *  \       R(3)        \          L(1)        / \
  *   3      ---->        2         ---->      1   3
  *  /                     \
  * 2                       3
  */
}

template <typename T>
static void
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
// - The retracing can stop if the balance factor becomes 0 implying that the
//   height of that subtree remains unchanged.
// - If balance factor becomes -1 or +2 continue retraceing
// - If balance factor becomes -2 or +2 we need to repair
template <typename T>
static DNode<T> *
retrace(DNode<T> *node) noexcept {
  DNode<T> *root = nullptr;

Lstart:
  if (node) {
    root = node;
    const std::int8_t b = balance(node);

    // Left heavy
    if (b < -1) {
      if (balance(node->left) == 1) {
        node->left = rotate_left(node->left);
      }

      set(node->parent, root) = rotate_right(node);
    }
    // Right heavy
    else if (b > 1) {
      if (balance(node->right) == -1) {
        node->right = rotate_right(node->left);
      }

      set(node->parent, root) = rotate_left(node);
    }

    calc_parent_balance(node);
    node = node->parent;
    goto Lstart;
  }

  return root;
}

template <typename T>
void
dump(DNode<T> *tree, std::string prefix = "", bool isTail = true,
     const char *ctx = "") noexcept {
  if (tree) {
    char name[256] = {0};
    sprintf(name, "%s%d", ctx, int(tree->value));

    printf("%s%s%s\n", prefix.c_str(), (isTail ? "└── " : "├── "), name);

    const char *ls = (isTail ? "    " : "│   ");
    if (tree->right && tree->left) {
      dump(tree->left, prefix + ls, false, "lt:");
      dump(tree->right, prefix + ls, true, "gt:");
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
void
verify(DNode<T> *parent, DNode<T> *tree) noexcept {
  if (tree) {
    assert(tree->parent == parent);
    if (tree->left) {
      assert(tree->value > tree->left->value);
      verify(tree, tree->left);
    }
    if (tree->right) {
      assert(tree->value < tree->left->value);
      verify(tree, tree->right);
    }
  }
}

} // namespace dtree
} // namespace impl

template <typename T>
void
dump(DTree<T> &tree) noexcept {
  return impl::dtree::dump(tree.root);
}

template <typename T>
void
verify(DTree<T> &tree) noexcept {
  impl::dtree::verify((DNode<T> *)nullptr, tree.root);
}

template <typename T, typename K>
std::tuple<T *, bool>
insert(DTree<T> &tree, K &&v) noexcept {
  if (!tree.root) {
    /*Insert into empty tree*/
    tree.root = new (std::nothrow) DNode<T>(std::forward<T>(v));
    if (tree.root) {
      return std::make_tuple(&tree.root->value, true);
    }

    return std::make_tuple(nullptr, false);
  }

  DNode<T> *it = tree.root;

Lstart:
  if (it->value < v) {
    if (it->left) {
      it = it->left;

      goto Lstart;
    }

    it->left = new (std::nothrow) DNode<T>(std::forward<T>(v), it);
    if (it->left) {
      tree.root = impl::dtree::retrace(it->left);

      return std::make_tuple(&it->left->value, true);
    }
  } else if (it->value > v) {
    if (it->right) {
      it = it->right;

      goto Lstart;
    }

    it->right = new (std::nothrow) DNode<T>(std::forward<T>(v), it);
    if (it->right) {
      tree.root = impl::dtree::retrace(it->right);

      return std::make_tuple(&it->right->value, true);
    }
  } else {

    return std::make_tuple(&it->value, false);
  }

  return std::make_tuple(nullptr, false);
}

} // namespace sp

#endif
