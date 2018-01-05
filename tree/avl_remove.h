#ifndef SP_MALLOC_AVL_REMOVE_TREE_H
#define SP_MALLOC_AVL_REMOVE_TREE_H

#include "avl.h"

namespace avl {

template <typename T, typename K>
bool
remove(Tree<T> &, const K &) noexcept;

namespace impl {
namespace avl {

namespace rem {
template <typename T, typename F>
static Node<T> *
retrace(Node<T> *it, F update_parent_balance) noexcept {
  Node<T> *current = nullptr;
Lstart:
  if (it) {
    current = it;
    {
      printf("---%s\n", std::string(*it).c_str());
      dump_root(it, "retrace|");
    }
    // https://www.geeksforgeeks.org/avl-tree-set-2-deletion/

    it = current->parent;
    goto Lstart;
  }
  // TODO
  return current;
} // avl::impl::retrace()

} // namespace rem

template <typename T>
void
nop(Node<T> *node) noexcept {
  node->parent = nullptr;
  node->left = nullptr;
  node->right = nullptr;
} // avl::impl::nop()

template <typename T>
static std::size_t
remove_parent_balance(Node<T> *const child) noexcept {
  Node<T> *parent = child->parent;
  Direction d = direction(child);

  if (d == Direction::LEFT) {
    if (parent->right)
      parent->balance++;
  } else {
    if (parent->left)
      parent->balance--;
  }

  return parent->balance;
} // avl::impl::remove_parent_balance()

template <typename T>
/*new root*/ Node<T> *
remove(Node<T> *const current, void (*cleanup)(Node<T> *)) noexcept {
  assert(current);

  auto parent_child_link = [](Node<T> *subject, Node<T> *nev) {
    // update parent -> child
    Node<T> *const parent = subject->parent;
    if (parent) {
      if (parent->left == subject) {
        parent->left = nev;
      } else {
        assert(parent->right == subject);
        parent->right = nev;
      }
    }
  };

  // TODO update balance factor when replaceing current(the new should have the
  // current balance since we only replace without changing balance)

  printf("remove(%d)", current->value);
  assert(sp::impl::tree::doubly_linked(current));
  if (current->left && current->right) {
    // two children
    printf(":2->");

    /*
     * find the smallest value in the _right_ (greater) branch which will be
     * removed and inserted in the current position.
     * Since we removed a node from the right branch we have to rebalance the
     * tree, but beacuse we only swap out current with the removed node we do
     * not change the balance of the second step.
     */
    Node<T> *const successor = sp::impl::tree::find_min(current->right);
    assert(sp::impl::tree::doubly_linked(successor));
    Node<T> *const new_root = remove(successor, nop);
    dump_root(current, "lr");
    // assert(verify_root(current));
    {
      /*
       * remove might run a retrace meaning that we can not assume that current
       * have left and right pointers
       */
      parent_child_link(current, successor);
      successor->parent = current->parent;
      if (current->left) {
        successor->left = current->left;
        successor->left->parent = successor;
      }

      if (current->right) {
        successor->right = current->right;
        successor->right->parent = successor;
      }
      successor->balance = current->balance;

      assert(sp::impl::tree::doubly_linked(successor));
      assert(sp::impl::tree::doubly_linked(successor->left));
      assert(sp::impl::tree::doubly_linked(successor->right));
    }
    cleanup(current);
    /*     X
     *    / \
     * min   max
     *      /
     *     min in the max branch
     */
    return new_root;
  } else if (!current->left && !current->right) {
    printf(":0\n");
    // zero children
    auto parent_direction = [](Node<T> *n) {
      if (n->parent) {
        return direction(n);
      }
      return Direction::RIGHT;
    };

    auto has_sibling = [](Node<T> *n) {
      Node<T> *parent = n->parent;
      if (parent) {
        assert(parent->left == n || parent->right == n);
        std::size_t children = 0;
        if (parent->left)
          children++;
        if (parent->right)
          children++;
        return children == 2;
      }
      return false;
    };

    Node<T> *const parent = current->parent;
    assert(verify(parent));
    bool sibling = has_sibling(current);

    Direction d = parent_direction(current);
    {
      parent_child_link(current, (Node<T> *)nullptr);
      assert(sp::impl::tree::doubly_linked(current->parent));
    }
    cleanup(current);

    if (parent) {
      /*
       * Since we remove a leaf we change the balance of the parent node
       */
      if (sibling) {
        if (d == Direction::RIGHT) {
          parent->balance--;
        } else {
          parent->balance++;
        }
      }

      return rem::retrace(parent, [](Node<T> *child) { //
        return remove_parent_balance(child);
      });
    }

    // We just removed the last node in the tree
    return nullptr;
  } else if (current->left) {
    printf(":left\n");
    // one child

    auto *const left = current->left;
    {
      parent_child_link(current, left);
      Node<T> *const parent = current->parent;
      left->parent = parent;

      assert(sp::impl::tree::doubly_linked(parent));
    }
    cleanup(current);

    return rem::retrace(left, [](Node<T> *child) { //
      return remove_parent_balance(child);
    });
  }
  assert(current->right);
  printf(":right\n");
  // one child

  auto *const right = current->right;
  {
    parent_child_link(current, right);
    Node<T> *const parent = current->parent;
    right->parent = parent;

    assert(sp::impl::tree::doubly_linked(parent));
  }
  cleanup(current);

  return rem::retrace(right, [](Node<T> *child) { //
    return remove_parent_balance(child);
  });
} // avl::impl::remove()

template <typename T>
static void
clean(Node<T> *n) noexcept {
  assert(n);
  printf("delete node(%p)\n", n);
  // delete n;
} // avl::impl::clean()

} // namespace avl
} // namespace impl

template <typename T, typename K>
bool
remove(Tree<T> &tree, const K &k) noexcept {
  Node<T> *const node = sp::impl::tree::find_node(tree.root, k);
  if (node) {
    Node<T> *const new_root = impl::avl::remove(node, impl::avl::clean);

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
} // av::remove()

} // namespace avl

#endif
