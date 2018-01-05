#include "crc.h"

#include "tree/avl.h"
#include "tree/avl_insert.h"
#include "tree/avl_remove.h"

#include <algorithm>
#include <random>
// #include "dyn_tree.h"
#include "global.h"
#include "global_debug.h"
#include "tree/bst.h"
#include "tree/static_tree.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdio.h>
#include <string>
#include <tuple>
#include <vector>

template <typename Function>
static void
time(const char *msg, Function f) {
  auto t1 = std::chrono::high_resolution_clock::now();
  f();
  auto t2 = std::chrono::high_resolution_clock::now();
  std::cout
      << msg << "|Delta t2-t1: "
      << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()
      << "ms" << std::endl;
}

template <typename T>
static bool
in_range(T &b, T &e) {
  uintptr_t strtB = reinterpret_cast<uintptr_t>(std::get<0>(b));
  std::size_t lenB = std::get<1>(b);

  uintptr_t strtE = reinterpret_cast<uintptr_t>(std::get<0>(e));
  std::size_t lenE = std::get<1>(e);

  return (strtB >= strtE && strtB < (strtE + lenE)) ||
         (strtE >= strtB && strtE < (strtB + lenB));
}

template <typename C>
static void
check_overlap(C &ptrs) {
  std::sort(ptrs.begin(), ptrs.end());
  auto current = ptrs.begin();
  while (current != ptrs.end()) {
    auto it = current;
    while (it++ != ptrs.end()) {
      if (in_range(*current, *it)) {
        printf("    current[%p, %zu]\n\t it[%p, %zu]\n",     //
               std::get<0>(*current), std::get<1>(*current), //
               std::get<0>(*it), std::get<1>(*it));
        assert(false);
      }
    }
    // printf("%p\n", std::get<0>(*current));
    current++;
  }
}

template <typename T>
T *
pmath(T *v, ptrdiff_t diff) {
  uintptr_t result = reinterpret_cast<uintptr_t>(v);
  result = result + diff;
  return reinterpret_cast<T *>(result);
}

// template <typename Points>
// static void assert_consecutive_range(Points &free, Range range) {
//   // printf("assert_consecutive_range\n");
//   uint8_t *const startR = range.start;
//
//   auto cmp = [](const auto &first, const auto &second) -> bool {
//     return std::get<0>(first) < std::get<0>(second);
//   };
//   std::sort(free.begin(), free.end(), cmp);
//
//   void *first = startR;
//   for (auto it = free.begin(); it != free.end(); ++it) {
//     assert(std::get<0>(*it) = first);
//
//     first = pmath(std::get<0>(*it), std::get<1>(*it));
//   }
//   assert(first == pmath(range.start, +range.length));
// }
//
//
struct Data {
  using T = int;
  bool present;
  T data;
  Data()
      : present(false)
      , data(0) {
  }
  explicit Data(T d)
      : present(true)
      , data(d) {
  }

  explicit operator bool() const noexcept {
    return present;
  }

  Data &
  operator=(const T &o) noexcept {
    present = true;
    data = o;
    return *this;
  }

  operator T() const noexcept {
    return data;
  }

  bool
  operator==(const T &o) const noexcept {
    return data == o;
  }

  bool
  operator==(const Data &o) const noexcept {
    return operator==(o.data);
  }

  bool
  operator>(const T &o) const noexcept {
    return data > o;
  }

  bool
  operator>(const Data &o) const noexcept {
    return operator>(o.data);
  }

  bool
  operator<(const T &o) const noexcept {
    return data < o;
  }

  bool
  operator<(const Data &o) const noexcept {
    return operator<(o.data);
  }

  int
  cmp(const int &o) const noexcept {
    if (data == o) {
      return 0;
    }
    if (data > o) {
      // TODO wrong?
      return -1;
    }
    return 1;
  }

  int
  cmp(const Data &o) const noexcept {
    return cmp(o.data);
  }
};

static void
test_avl();

static void
test_static_tree();

template <template <typename> class Tree_t>
static void
test_tree();

int
main() {
  // sp::crc_test();
  // test_static_tree();
  test_avl();//TODO
  // test_tree<bst::Tree>();
  // test_tree<avl::Tree>();

  return 0;
}

static void
test_static_tree() {
  // TODO the size calculations gives level+1 capacity which is wrong
  /*insert & search*/ {
    constexpr std::size_t levels = 10;
    using Type = sp::static_tree<Data, levels>;
    static_assert(Type::levels == levels, "");
    static_assert(Type::capacity == 2047, "");
    {
      Type tree;
      for (int key = 0; key < int(levels) + 1; ++key) {
        for (int i = 0; i < key; ++i) {
          Data *res = sp::search(tree, Data(i));
          assert(res);
          assert(res->data == i);
        }
        Data d(key);
        assert(sp::search(tree, d) == nullptr);
        assert(sp::insert(tree, d));
        printf("============\n");
        assert(sp::search(tree, d) != nullptr);
      }
      printf("done insert\n");
    }
    {
      Type tree;
      for (int key = levels + 1; key > 0; --key) {
        Data d(key);
        assert(sp::search(tree, d) == nullptr);
        assert(sp::insert(tree, d));
        assert(sp::search(tree, d) != nullptr);
      }
      printf("done reverse insert\n");
    }
  }

  {
    const sp::Direction left = sp::Direction::LEFT;
    const sp::Direction right = sp::Direction::RIGHT;
    {
      sp::relative_idx parent_idx(0);
      for (std::size_t level = 0; level < 63; ++level) {
        {
          sp::relative_idx idx = sp::impl::lookup_relative(parent_idx, left);
          auto a_id = sp::impl::parent_relative(idx);
          assert(a_id == parent_idx);
        }
        {
          sp::relative_idx idx = sp::impl::lookup_relative(parent_idx, right);
          // printf("lookup :: parent[%zu] -> %s -> r_idx[%zu]\n", //
          //        std::size_t(parent_idx), "right", std::size_t(idx));

          auto a_id = sp::impl::parent_relative(idx);
          // printf("parent :: level[%zu] -> r_idx[%zu] -> parent[%zu]\n", //
          //        level + 1, std::size_t(idx), std::size_t(a_id));
          assert(a_id == parent_idx);
          parent_idx = idx;
        }
      }
    }
    printf("--------\n");
    {
      sp::relative_idx parent_idx(0);
      for (std::size_t level = 0; level < 63; ++level) {
        {
          sp::relative_idx idx = sp::impl::lookup_relative(parent_idx, right);
          auto a_id = sp::impl::parent_relative(idx);
          assert(a_id == parent_idx);
        }
        {
          sp::relative_idx idx = sp::impl::lookup_relative(parent_idx, left);
          // printf("lookup :: parent[%zu] -> %s -> r_idx[%zu]\n", //
          //        std::size_t(parent_idx), "left", std::size_t(idx));

          auto a_id = sp::impl::parent_relative(idx);
          // printf("parent :: level[%zu] -> r_idx[%zu] -> parent[%zu]\n", //
          //        level + 1, std::size_t(idx), std::size_t(a_id));
          assert(a_id == parent_idx);

          parent_idx = idx;
        }
      }
    }
    printf("--------\n");
  }
  {
    printf("in_order\n");
    constexpr std::size_t levels = 2;
    using Type = sp::static_tree<Data, levels>;
    {
      Type tree;
      sp::insert(tree, Data(3));

      sp::insert(tree, Data(1));
      sp::insert(tree, Data(2));
      sp::insert(tree, Data(0));

      sp::insert(tree, Data(5));
      sp::insert(tree, Data(4));
      sp::insert(tree, Data(6));

      printf("insert: ");
      for (std::size_t i = 0; i < Type::capacity; ++i) {
        printf("%d,", tree.storagex[i].data);
      }
      printf("\n");
      for (std::size_t i = 0; i < Type::capacity; ++i) {
        assert(sp::search(tree, i));
      }
    }
    {
      Type tree;
      int i = 0;
      sp::in_order_for_each(tree, [&i](Data &d) {
        assert(!bool(d));
        printf("in_order(%d)", i);
        printf("in_order(%d)", d.data);
        d = Data(i);
        printf("in_order(%d)\n", d.data);
        assert(bool(d));
        ++i;
      });
      // printf("inserted[%d],capacity[%zu]\n", i, Type::capacity);
      assert(i == Type::capacity);

      printf("in_order: ");
      for (std::size_t i = 0; i < Type::capacity; ++i) {
        printf("%d,", tree.storagex[i].data);
      }
      printf("\n");
      for (std::size_t i = 0; i < Type::capacity; ++i) {
        assert(sp::search(tree, i));
      }
    }
    printf("--------\n");
  }
  {
    constexpr std::size_t levels = 9;
    using Type = sp::static_tree<Data, levels>;
    static_assert(Type::levels == levels, "");
    // static_assert(Type::capacity == 1023, "");
    Type tree;
    for (std::size_t i = 0; i < Type::capacity; ++i) {
      Data d((int)i);
      assert(sp::search(tree, d) == nullptr);
    }
    {
      int i = 0;
      sp::in_order_for_each(tree, [&i](Data &d) {
        assert(!bool(d));
        d = Data(i);
        ++i;
      });
      printf("inserted[%d],capacity[%zu]\n", i, Type::capacity);
      assert(i == Type::capacity);
    }
    {
      int i = 0;
      sp::in_order_for_each(tree, [&i](Data &d) {
        assert(bool(d));
        assert(d.data == i);
        ++i;
      });
      printf("present[%d],capacity[%zu]\n", i, Type::capacity);
      assert(i == Type::capacity);
    }
    for (std::size_t i = 0; i < Type::capacity; ++i) {
      Data d((int)i);
      assert(sp::search(tree, d) != nullptr);
    }
    printf("search ok!\n");
  }
  {
    constexpr std::size_t levels = 9;
    using Type = sp::static_tree<sp::SortedNode<Data>, levels>;
    Type tree;
    Data d(1);
    insert(tree, d);
  }
  {
    constexpr std::size_t levels = 2;
    using Type = sp::static_tree<Data, levels>;
    Type tree;
    int i = 0;
    sp::in_order_for_each(tree, [&i](auto &node) {
      node = i++;
      //
    });
    {
      int cmp = 0;
      sp::in_order_for_each(tree, [&cmp](auto node) {
        assert(node == cmp);
        cmp++;
      });
      assert(cmp == i);
      assert(cmp == Type::capacity);
      printf("cmp: %d\n", cmp);
    }
    {
      printf("in_order: ");
      for (std::size_t i = 0; i < Type::capacity; ++i) {
        printf("%d,", tree.storagex[i]);
      }
      printf("\n");
    }
    for (int k = 0; k < Type::capacity; ++k) {
      auto *r = sp::search(tree, k);
      printf("k[%d] < i[%d]\n", k, i);
      assert(r);
      if (r) {
        assert(*r == k);
      }
    }
  }
}

static void
test_avl() {
  printf("#rotate left\n");
  {
    auto a = new avl::Node<Data>(Data(1));
    a->balance = 2;
    auto b = a->right = new avl::Node<Data>(Data(2), a);
    b->balance = 1;
    auto c = b->right = new avl::Node<Data>(Data(3), b);
    c->balance = 0;

    auto reb = avl::impl::avl::rotate_left(a);
    assert(reb->value == b->value);
    assert(reb->parent == nullptr);
    assert(reb->balance == 0);

    assert(reb->left->value == a->value);
    assert(reb->left->parent == b);
    assert(reb->left->left == nullptr);
    assert(reb->left->right == nullptr);
    assert(reb->left->balance == 0);

    assert(reb->right->value == c->value);
    assert(reb->right->parent == b);
    assert(reb->right->left == nullptr);
    assert(reb->right->right == nullptr);
    assert(reb->right->balance == 0);
  }
  printf("#rotate right\n");
  {
    auto c = new avl::Node<Data>(Data(3));
    c->balance = -2;
    auto b = c->left = new avl::Node<Data>(Data(2), c);
    b->balance = -1;
    auto a = b->left = new avl::Node<Data>(Data(1), b);
    a->balance = 0;

    auto reb = avl::impl::avl::rotate_right(c);
    assert(reb->value == b->value);
    assert(reb->parent == nullptr);
    assert(reb->balance == 0);

    assert(reb->left->value == a->value);
    assert(reb->left->parent == b);
    assert(reb->left->left == nullptr);
    assert(reb->left->right == nullptr);
    assert(reb->left->balance == 0);

    assert(reb->right->value == c->value);
    assert(reb->right->parent == b);
    assert(reb->right->left == nullptr);
    assert(reb->right->right == nullptr);
    assert(reb->right->balance == 0);
  }
  printf("#Double right-left rotation\n");
  {
    auto c = new avl::Node<Data>(Data(3));
    c->balance = -2;
    auto b = c->left = new avl::Node<Data>(Data(1), c);
    b->balance = 1;
    auto a = b->right = new avl::Node<Data>(Data(2), b);
    a->balance = 0;

    c->left = avl::impl::avl::rotate_left(c->left);
    auto reb = avl::impl::avl::rotate_right(c);
    assert(reb->value == 2);
    assert(reb->parent == nullptr);
    assert(reb->balance == 0);

    assert(reb->left->value == 1);
    assert(reb->left->parent);
    assert(reb->left->parent == a);
    assert(reb->left->left == nullptr);
    assert(reb->left->right == nullptr);
    assert(reb->left->balance == 0);

    assert(reb->right->value == 3);
    assert(reb->right->parent);
    assert(reb->right->parent == a);
    assert(reb->right->left == nullptr);
    assert(reb->right->right == nullptr);
    assert(reb->right->balance == 0);
  }
  printf("#Double left-right rotation\n");
  {
    auto c = new avl::Node<Data>(Data(1));
    c->balance = 2;
    auto b = c->right = new avl::Node<Data>(Data(3), c);
    b->balance = -1;
    auto a = b->left = new avl::Node<Data>(Data(2), b);
    a->balance = 0;

    c->right = avl::impl::avl::rotate_right(c->right);
    auto reb = avl::impl::avl::rotate_left(c);
    assert(reb->value == 2);
    assert(reb->parent == nullptr);
    assert(reb->balance == 0);

    assert(reb->left->value == 1);
    assert(reb->left->parent);
    assert(reb->left->parent == a);
    assert(reb->left->left == nullptr);
    assert(reb->left->right == nullptr);
    assert(reb->left->balance == 0);

    assert(reb->right->value == 3);
    assert(reb->right->parent);
    assert(reb->right->parent == a);
    assert(reb->right->left == nullptr);
    assert(reb->right->right == nullptr);
    assert(reb->right->balance == 0);
  }

  /**/ {
    // ├── gt:[v:2|b:2]
    // │   └── gt:[v:3|b:1]
    // │       └── gt:[v:4|b:0]

    auto c = new avl::Node<Data>(Data(2));
    c->balance = 2;
    auto b = c->right = new avl::Node<Data>(Data(3), c);
    b->balance = 1;
    auto a = b->right = new avl::Node<Data>(Data(4), b);
    a->balance = 0;
    auto reb = avl::impl::avl::rotate_left(c);

    assert(reb->value == 3);
    assert(reb->parent == nullptr);
    assert(reb->balance == 0);

    assert(reb->left->value == 2);
    assert(reb->left->parent);
    assert(reb->left->parent == b);
    assert(reb->left->left == nullptr);
    assert(reb->left->right == nullptr);
    assert(reb->left->balance == 0);

    assert(reb->right->value == 4);
    assert(reb->right->parent);
    assert(reb->right->parent == b);
    assert(reb->right->left == nullptr);
    assert(reb->right->right == nullptr);
    assert(reb->right->balance == 0);
  }
  /*test increasing order*/ {
    avl::Tree<Data> tree;
    int i = 0;
    for (; i < 10; ++i) {
      printf(".%d <- ", i);
      Data ins(i);
      auto res = avl::insert(tree, ins);
      assert(std::get<1>(res));
      assert(std::get<0>(res) != nullptr);
      if (!avl::verify(tree)) {
        dump(tree);
        assert(false);
      }
      printf("\n--------------\n");
    }
  }
  /*test 1a*/
  {
    avl::Tree<int> tree;
    {
      avl::insert(tree, 20);
      auto root = tree.root;
      assert(root);
      assert(root->value == 20);
      assert(root->balance == 0);

      assert(root->right == nullptr);
      assert(root->left == nullptr);
    }

    {
      avl::insert(tree, 4);
      auto root = tree.root;
      assert(root);
      assert(root->value == 20);
      assert(root->balance == -1);

      assert(root->right == nullptr);

      assert(root->left);
      assert(root->left->value == 4);
      assert(root->left->balance == 0);
      assert(root->left->left == nullptr);
      assert(root->left->right == nullptr);
    }

    {
      avl::insert(tree, 15);
      auto root = tree.root;
      assert(root);
      assert(root->value == 15);
      assert(root->balance == 0);

      assert(root->right);
      assert(root->right->value == 20);
      assert(root->right->balance == 0);
      assert(root->right->left == nullptr);
      assert(root->right->right == nullptr);

      assert(root->left);
      assert(root->left->value == 4);
      assert(root->left->balance == 0);
      assert(root->left->left == nullptr);
      assert(root->left->right == nullptr);
    }
  }

  /*test 2a*/
  /*test 3a*/
  /*test 1b*/ {
    avl::Tree<int> tree;
    avl::insert(tree, 20);
    {
      avl::insert(tree, 8);
      auto root = tree.root;
      assert(root);
      assert(root->value == 20);
      assert(root->balance == -1);

      assert(root->right == nullptr);

      assert(root->left);
      assert(root->left->value == 8);
      assert(root->left->balance == 0);
      assert(root->left->left == nullptr);
      assert(root->left->right == nullptr);
    }
    {
      avl::insert(tree, 4);
      auto root = tree.root;
      assert(root);
      assert(root->value == 8);
      assert(root->balance == 0);

      assert(root->right);
      assert(root->right->value == 20);
      assert(root->right->balance == 0);
      assert(root->right->left == nullptr);
      assert(root->right->right == nullptr);

      assert(root->left);
      assert(root->left->value == 4);
      assert(root->left->balance == 0);
      assert(root->left->left == nullptr);
      assert(root->left->right == nullptr);
    }
  }
  // #<{(|test inc insert dec remove|)}># {
  //   avl::Tree<int> tree;
  //   int i = 0;
  //   for (; i < 10; ++i) {
  //     printf(".%d <- \n", i);
  //     auto res = avl::insert(tree, i);
  //     assert(std::get<1>(res));
  //     assert(std::get<0>(res) != nullptr);
  //     dump(tree);
  //     avl::verify(tree);
  //     printf("\n--------------\n");
  //   }
  //   --i;
  //   for (; i > 0; --i) {
  //     printf(".%d -> \n", i);
  //     auto res = avl::remove(tree, i);
  //     printf("%d = av::remove(tree, %d)\n", res, i);
  //     assert(res);
  //     dump(tree);
  //     avl::verify(tree);
  //     printf("\n--------------\n");
  //   }
  // }
  {
    for (int i = 0; i < 20; ++i) {
      printf("##remove all[%d]\n", i);
      avl::Tree<int> tree;
      for (int d = 0; d <= i; ++d) {
        auto res = avl::insert(tree, d);
        /*assert*/ {
          int *const iptr = std::get<0>(res);
          assert(std::get<1>(res) == true);
          assert(iptr);
          assert(*iptr == d);
          if (!avl::verify(tree)) {
            avl::dump(tree, "\033[92minsert\033[0m|");
            assert(false);
          }
        }
      }
      avl::dump(tree, "\033[92mbefore\033[0m|");

      for (int d = 0; d <= i; ++d) {
        auto res = avl::remove(tree, d);
        /*assert*/ {
          assert(res);
          if (!avl::verify(tree)) {
            avl::dump(tree, "\033[91mremove\033[0m|");
            assert(false);
          }
          assert(sp::find(tree, d) == nullptr);
        }
      }
      assert(tree.root == nullptr);
      printf("------\n\n");
    }
  }
  /*test random*/ {
    std::random_device rd;
    std::mt19937 g(rd());
    std::size_t counter = 0;
    while (counter < 10) {
      if (counter % 100 == 0) {
        printf("cnt: %zu\n", counter);
      }
      avl::Tree<int> tree;
      constexpr int in_size = 1000;
      int in[in_size];
      for (int i = 0; i < in_size; ++i) {
        in[i] = i;
      }

      std::shuffle(in, in + in_size, g);
      for (int i = 0; i < in_size; ++i) {
        for (int k = 0; k < i; ++k) {
          auto f = sp::find(tree, in[k]);
          assert(f);
          assert(*f == in[k]);
        }
        // printf(".%d <- ", i);
        auto res = avl::insert(tree, in[i]);
        int *const iptr = std::get<0>(res);
        assert(std::get<1>(res) == true);
        assert(iptr);
        assert(*iptr == in[i]);

        int *const fptr = sp::find(tree, in[i]);
        assert(fptr);
        assert(fptr == iptr);
        assert(*fptr == *iptr);

        if (!avl::verify(tree)) {
          avl::dump(tree, "after|");
          assert(false);
        }
      }

      counter++;
    }
    // TODO insert duplicate
    // TODO delete non existing

    printf("done\n");
  }
  printf("==========\n");
}

template <class Tree_t, typename T, std::size_t in_size>
static void
find_stuff(Tree_t &tree, std::size_t deleted, T (&in)[in_size]) {
  for (int k = 0; k < in_size; ++k) {
    auto *f = sp::find(tree, in[k]);
    if (k < deleted) {
      assert(f == nullptr);
    } else {
      if (!f) {
        printf("%p = sp::find(tree, %d)\n", f, in[k]);
        dump(tree, "find|");
      }
      assert(f);
      assert(*f == in[k]);
    }
  }
}

template <template <typename> class Tree_t>
static void
test_tree() {
  std::size_t counter = 0;
  // std::random_device rd;
  // std::mt19937 g(rd());
  std::mt19937 g(0);

  while (true) {
    if (counter % 10 == 0) {
      printf("cnt: %zu\n", counter);
    }
    Tree_t<int> tree;
    constexpr int in_size = 10;
    int in[in_size];
    for (int i = 0; i < in_size; ++i) {
      in[i] = i;
    }

    std::shuffle(in, in + in_size, g);
    for (int i = 0; i < in_size; ++i) {
      for (int k = 0; k < i; ++k) {
        auto f = sp::find(tree, in[k]);
        assert(f);
        assert(*f == in[k]);
      }
      // printf(".%d <- %d\n", i, in[i]);
      auto res = insert(tree, in[i]);
      int *const iptr = std::get<0>(res);
      assert(std::get<1>(res) == true);
      assert(iptr);
      assert(*iptr == in[i]);
      {
        int *const fptr = sp::find(tree, in[i]);
        assert(fptr);
        assert(fptr == iptr);
        assert(*fptr == *iptr);
      }

      if (!verify(tree)) {
        dump(tree, "ins|");
        assert(false);
      }
    }

    std::shuffle(in, in + in_size, g);
    for (int i = 0; i < in_size; ++i) {
      find_stuff(tree, i, in);

      dump(tree, "before|");
      printf("--\n");
      printf("remove(tree,%d)\n", in[i]);
      bool rb = remove(tree, in[i]);
      printf(" = %s\n", rb ? "true" : "false");
      assert(rb);
      if (!verify(tree)) {
        printf("\n");
        dump(tree, "rem|");
        assert(false);
      } else {
        dump(tree, "rem|");
      }

      find_stuff(tree, i + 1, in);
    }
    counter++;
  }
  // TODO insert duplicate
  // TODO delete non existing

  printf("done\n");
}
