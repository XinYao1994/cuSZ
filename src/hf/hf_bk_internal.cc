/**
 * @file hfserial_book2.cc
 * @author Jiannan Tian
 * @brief
 * @version 0.4
 * @date 2023-08-17
 *
 * (C) 2023 by Indiana University, Argonne National Laboratory
 *
 */

#include <queue>

#include "busyheader.hh"
#include "hf/hf_bk_impl.hh"
#include "hf/hf_word.hh"
#include "utils/timer.hh"

// internal data structure

#define NodeStackTpl template <class NodeType, int Width>
#define NodeStack __pszhf_stack<NodeType, Width>

NodeStackTpl NodeType* NodeStack::top(NodeStack* s)
{
  return s->_a[s->depth - 1];
}

NodeStackTpl template <typename T>
void NodeStack::push(NodeStack* s, NodeType* n, T path, T len)
{
  if (s->depth + 1 <= NodeStack::MAX_DEPTH) {
    s->depth += 1;

    s->_a[s->depth - 1] = n;
    s->saved_path[s->depth - 1] = path;
    s->saved_length[s->depth - 1] = len;
  }
  else
    throw std::runtime_error("[psz::err::hf::traverse_stack]: exceeding MAX_DEPTH, stack overflow.");
}

NodeStackTpl template <typename T>
NodeType* NodeStack::pop(
    NodeStack* s, T* path_to_restore, T* length_to_restore)
{
  auto is_empty = [&](NodeStack* s) -> bool { return (s->depth == 0); };

  NodeType* n;

  if (is_empty(s)) {
    printf("Error: stack underflow, exiting...\n");
    return nullptr;
  }
  else {
    // TODO holding array -> __a
    n = s->_a[s->depth - 1];
    s->_a[s->depth - 1] = nullptr;

    *length_to_restore = s->saved_length[s->depth - 1];
    *path_to_restore = s->saved_path[s->depth - 1];
    s->depth -= 1;

    return n;
  }
}

NodeStackTpl template <typename H>
void NodeStack::inorder_traverse(NodeType* root, H* book)
{
  auto is_empty = [&](NodeStack* s) -> bool { return (s->depth == 0); };

  auto s = new NodeStack();

  bool done = 0;
  H out1 = 0, len = 0;

  while (not done) {
    if (root->left or root->right) {
      push(s, root, out1, len);
      root = root->left;
      out1 <<= 1u;
      out1 |= 0u;
      len += 1;
    }
    else {
      u4 symbol = root->symbol;
      book[symbol] =
          out1 |
          ((len & (H)0xffu)
           << (sizeof(H) * 8 - PackedWordByWidth<sizeof(H)>::FIELDWIDTH_bits));
      if (!is_empty(s)) {
        root = pop(s, &out1, &len);
        root = root->right;
        out1 <<= 1u;
        out1 |= 1u;
        len += 1;
      }
      else
        done = true;
    }
  }

  delete s;
  /* end of function */
}

template class __pszhf_stack<node_t, 4>;
template void __pszhf_stack<node_t, 4>::inorder_traverse<u4>(node_t*, u4*);
template class __pszhf_stack<node_t, 8>;
template void __pszhf_stack<node_t, 8>::inorder_traverse<u8>(node_t*, u8*);
template void __pszhf_stack<node_t, 8>::inorder_traverse<ull>(node_t*, ull*);

template class __pszhf_stack<NodeCxx, 4>;
template void __pszhf_stack<NodeCxx, 4>::inorder_traverse<u4>(NodeCxx*, u4*);
template class __pszhf_stack<NodeCxx, 8>;
template void __pszhf_stack<NodeCxx, 8>::inorder_traverse<u8>(NodeCxx*, u8*);
template void __pszhf_stack<NodeCxx, 8>::inorder_traverse<ull>(NodeCxx*, ull*);

#undef NodeStackTpl
#undef NodeStack