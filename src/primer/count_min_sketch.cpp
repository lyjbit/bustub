//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// count_min_sketch.cpp
//
// Identification: src/primer/count_min_sketch.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "primer/count_min_sketch.h"

#include <stdexcept>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <algorithm>
#include <limits>
namespace bustub {

/**
 * Constructor for the count-min sketch.
 *
 * @param width The width of the sketch matrix.
 * @param depth The depth of the sketch matrix.
 * @throws std::invalid_argument if width or depth are zero.
 */
template <typename KeyType>
CountMinSketch<KeyType>::CountMinSketch(uint32_t width, uint32_t depth) : width_(width), depth_(depth) ,counters_(std::make_unique<std::atomic<uint32_t>[]>(static_cast<size_t>(width)*depth)){
  /** @TODO(student) Implement this function! */

  /** @spring2026 PLEASE DO NOT MODIFY THE FOLLOWING */
  // Initialize seeded hash functions
  for(size_t i=0;i<static_cast<size_t>(width_)*depth_;++i)
  {
    counters_[i].store(0);
  }
  hash_functions_.reserve(depth_);
  for (size_t i = 0; i < depth_; i++) { 
    hash_functions_.push_back(this->HashFunction(i));
  }
  
}

template <typename KeyType>
CountMinSketch<KeyType>::CountMinSketch(CountMinSketch &&other) noexcept : width_(other.width_), depth_(other.depth_) {
  /** @TODO(student) Implement this function! */
}

template <typename KeyType>
auto CountMinSketch<KeyType>::operator=(CountMinSketch &&other) noexcept -> CountMinSketch & {
  /** @TODO(student) Implement this function! */
  return *this;
}

template <typename KeyType>
void CountMinSketch<KeyType>::Insert(const KeyType &item) {
  size_t i=0;
  /** @TODO(student) Implement this function! */
  for(const auto &hash_i:hash_functions_)
  {
    size_t index=i*width_+hash_i(item);
    counters_[index].fetch_add(1, std::memory_order_relaxed);
    ++i;
  }
}

template <typename KeyType>
void CountMinSketch<KeyType>::Merge(const CountMinSketch<KeyType> &other) {
  if (width_ != other.width_ || depth_ != other.depth_) {
    throw std::invalid_argument("Incompatible CountMinSketch dimensions for merge.");
  }
  /** @TODO(student) Implement this function! */
  for(size_t i=0;i<static_cast<size_t>(width_)*depth_;++i)
  {
    counters_[i].fetch_add(other.counters_[i].load(std::memory_order_relaxed),std::memory_order_relaxed);
  }
}

template <typename KeyType>
auto CountMinSketch<KeyType>::Count(const KeyType &item) const -> uint32_t {
  uint32_t min_count=std::numeric_limits<uint32_t>::max();
  for(size_t i=0;i<depth_;++i)
  {
    size_t index=i*width_+hash_functions_[i](item);
    min_count=std::min(min_count,counters_[index].load(std::memory_order_relaxed));
  }
  return min_count;
}

template <typename KeyType>
void CountMinSketch<KeyType>::Clear() {
  /** @TODO(student) Implement this function! */
  for(size_t i=0;i<depth_;++i)
  {
    for(size_t j=0;j<width_;++j)
    {
      counters_[i*width_+j].store(0,std::memory_order_relaxed);
    }
  }
}

template <typename KeyType>
auto CountMinSketch<KeyType>::TopK(uint16_t k, const std::vector<KeyType> &candidates)
    -> std::vector<std::pair<KeyType, uint32_t>> {
  /** @TODO(student) Implement this function! */
  return {};
}

// Explicit instantiations for all types used in tests
template class CountMinSketch<std::string>;
template class CountMinSketch<int64_t>;  // For int64_t tests
template class CountMinSketch<int>;      // This covers both int and int32_t
}  // namespace bustub
