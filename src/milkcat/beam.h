//
// The MIT License (MIT)
//
// Copyright 2013-2014 The MilkCat Project Developers
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// beam.h --- Created at 2014-02-25
//

#ifndef SRC_MILKCAT_BEAM_H_
#define SRC_MILKCAT_BEAM_H_

#include <algorithm>

template<class Node>
class NodePool {
 public:
  NodePool(): alloc_index_(0) {}

  ~NodePool() {
    for (auto &node : nodes_) delete node;
  }

  // Alloc a node
  Node *Alloc() {
    Node *node;
    if (alloc_index_ == nodes_.size()) {
      nodes_.push_back(new Node());
    }
    return nodes_[alloc_index_++];
  }

  // Release all node alloced before
  void ReleaseAll() {
    alloc_index_ = 0;
  }

 private:
  std::vector<Node *> nodes_;
  int alloc_index_;
};

template<class Node>
class Beam {
 public:
  Beam(int n_best, 
       NodePool<Node> *node_pool, 
       int beam_id,
       bool (* node_ptr_cmp)(Node *n1, Node *n2)):
      n_best_(n_best),
      capability_(n_best * 10),
      beam_id_(beam_id),
      size_(0),
      node_pool_(node_pool),
      node_ptr_cmp_(node_ptr_cmp) {
    nodes_ = new Node *[capability_];
  }

  ~Beam() {
    delete[] nodes_;
  }

  int size() const { return size_; }
  const Node *node_at(int index) const { return nodes_[index]; }

  // Shrink nodes_ array and remain top n_best elements
  void Shrink() {
    if (size_ <= n_best_) return;
    std::nth_element(nodes_, nodes_ + n_best_, nodes_ + size_, node_ptr_cmp_);
    size_ = n_best_;
  }

  // Clear all elements in bucket
  void Clear() {
    size_ = 0;
  }

  // Get min node in the bucket
  const Node *MinimalNode() {
    return *std::min_element(nodes_, nodes_ + size_, node_ptr_cmp_);
  }

  // Add an arc to decode graph
  void Add(Node *node) {
    nodes_[size_] = node;
    size_++;
    if (size_ >= capability_) Shrink();
  }

 private:
  Node **nodes_;
  int capability_;
  int beam_id_;
  NodePool<Node> *node_pool_;
  bool (* node_ptr_cmp_)(Node *n1, Node *n2);
  int n_best_;
  int size_;
};

#endif  // SRC_MILKCAT_BEAM_H_
