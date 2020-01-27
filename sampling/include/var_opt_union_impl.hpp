/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef _VAR_OPT_UNION_IMPL_HPP_
#define _VAR_OPT_UNION_IMPL_HPP_

#include "var_opt_union.hpp"

#include <cmath>

namespace datasketches {

template<typename T, typename S, typename A>
var_opt_union<T,S,A>::var_opt_union(uint32_t max_k) :
  n_(0),
  outer_tau_numer_(0),
  outer_tau_denom_(0.0),
  max_k_(max_k),
  gadget_(max_k, var_opt_sketch<T,S,A>::DEFAULT_RESIZE_FACTOR, true)
{}

template<typename T, typename S, typename A>
var_opt_union<T,S,A>::var_opt_union(const var_opt_union& other) :
  n_(other.n_),
  outer_tau_numer_(other.outer_tau_numer_),
  outer_tau_denom_(other.outer_tau_denom_),
  max_k_(other.max_k_),
  gadget_(other.gadget)
{}

template<typename T, typename S, typename A>
var_opt_union<T,S,A>::var_opt_union(var_opt_union&& other) noexcept :
  n_(other.n_),
  outer_tau_numer_(other.outer_tau_numer_),
  outer_tau_denom_(other.outer_tau_denom_),
  max_k_(other.max_k_)
{
  gadget_ = std::move(other.gadget_);
}

template<typename T, typename S, typename A>
var_opt_union<T,S,A>::~var_opt_union() {}

template<typename T, typename S, typename A>
var_opt_union<T,S,A>& var_opt_union<T,S,A>::operator=(const var_opt_union& other) {
  var_opt_union<T,S,A> union_copy(other);
  std::swap(n_, union_copy.n_);
  std::swap(outer_tau_numer_, union_copy.outer_tau_numer_);
  std::swap(outer_tau_numer_, union_copy.outer_tau_denom_);
  std::swap(max_k_, union_copy.max_k_);
  std::swap(gadget_, union_copy.gadget_);
  return *this;
}

template<typename T, typename S, typename A>
var_opt_union<T,S,A>& var_opt_union<T,S,A>::operator=(var_opt_union&& other) {
  std::swap(n_, other.n_);
  std::swap(outer_tau_numer_, other.outer_tau_numer_);
  std::swap(outer_tau_numer_, other.outer_tau_denom_);
  std::swap(max_k_, other.max_k_);
  std::swap(gadget_, other.gadget_);
  return *this;
}

template<typename T, typename S, typename A>
void var_opt_union<T,S,A>::reset() {
  if (gadget_ != nullptr)
    gadget_.reset();
}

template<typename T, typename S, typename A>
std::ostream& var_opt_union<T,S,A>::to_stream(std::ostream& os) const {
  os << "### VarOpt Union SUMMARY: " << std::endl;
  os << " . n             : " << n_ << std::endl;
  os << "   Max k         : " << max_k_ << std::endl;
  os << "   Gadget Summary: " << std::endl;
  gadget_.to_stream(os);
  os << "### END VarOpt Union SUMMARY: " << std::endl;

  return os;
}

template<typename T, typename S, typename A>
std::string var_opt_union<T,S,A>::to_string() const {
  std::ostringstream ss;
  to_stream(ss);
  return ss.str();
}

template<typename T, typename S, typename A>
void var_opt_union<T,S,A>::update(var_opt_sketch<T,S,A>& sk) {
  merge_into(sk);
}

template<typename T, typename S, typename A>
double var_opt_union<T,S,A>::get_outer_tau() const {
  if (outer_tau_denom_ == 0) {
    return 0.0;
  } else {
    return outer_tau_numer_ / outer_tau_denom_;
  }
}

template<typename T, typename S, typename A>
void var_opt_union<T,S,A>::merge_into(const var_opt_sketch<T,S,A>& sketch) {
  if (sketch.n_ == 0) {
    return;
  }

  n_ += sketch.n_;

  // H region iterator
  typename var_opt_sketch<T,S,A>::const_iterator h_itr(sketch, false, false, false);
  typename var_opt_sketch<T,S,A>::const_iterator h_end(sketch, true, false, false);
  while (h_itr != h_end) {
    std::pair<const T&, const double> sample = *h_itr;
    gadget_.update(sample.first, sample.second, false);
    ++h_itr;
  }

  // Weight-correcitng R region iterator
  typename var_opt_sketch<T,S,A>::const_iterator r_itr(sketch, false, true, true);
  typename var_opt_sketch<T,S,A>::const_iterator r_end(sketch, true, true, true);
  while (r_itr != r_end) {
    std::pair<const T&, const double> sample = *r_itr;
    gadget_.update(sample.first, sample.second, true);
    ++r_itr;
  }

  // resolve tau
  if (sketch.r_ > 0) {
    double sketch_tau = sketch.get_tau();
    double outer_tau = get_outer_tau();

    if (outer_tau_denom_ == 0) {
      // detect first estimation mode sketch and grab its tau
      outer_tau_numer_ = sketch.total_wt_r_;
      outer_tau_denom_ = sketch.r_;
    } else if (sketch_tau > outer_tau) {
      // switch to a bigger value of outer_tau
      outer_tau_numer_ = sketch.total_wt_r_;
      outer_tau_denom_ = sketch.r_;
    } else if (sketch_tau == outer_tau) {
      // Ok if previous equality test isn't quite perfect. Mistakes in either direction should
      // be fairly benign.
      // Without conceptually changing outer_tau, update number and denominator. In particular,
      // add the total weight of the incoming reservoir to the running total.
      outer_tau_numer_ += sketch.total_wt_r_;
      outer_tau_denom_ += sketch.r_;
    }

    // do nothing if sketch's tau is smaller than outer_tau
  }
}

template<typename T, typename S, typename A>
var_opt_sketch<T,S,A> var_opt_union<T,S,A>::get_result() const {
  // If no marked items in H, gadget is already valid mathematically. We can return what is
  // basically just a copy of the gadget.
  if (gadget_.num_marks_in_h_ == 0) {
    return simple_gadget_coercer();
  } else {
    // Copy of gadget. This may produce needless copying in the
    // pseudo-exact case below, but should simplify the code without
    // needing to make the gadget a pointer
    var_opt_sketch<T,S,A> gcopy(gadget_, false, n_);

    // At this point, we know that marked items are present in H. So:
    //   1. Result will necessarily be in estimation mode
    //   2. Marked items currently in H need to be absorbed into reservoir (R)
    bool is_pseudo_exact = detect_and_handle_subcase_of_pseudo_exact(gcopy);
    if (!is_pseudo_exact) {
      // continue with main logic
      migrate_marked_items_by_decreasing_k(gcopy);
    }
    // sub-case was already detected and handled, so return the result
    return gcopy;
  }
}

/**
 * When there are no marked items in H, the gadget is mathematically equivalent to a valid
 * varopt sketch. This method simply returns a copy (without perserving marks).
 *
 * @return A shallow copy of the gadget as valid varopt sketch
 */
template<typename T, typename S, typename A>
var_opt_sketch<T,S,A> var_opt_union<T,S,A>::simple_gadget_coercer() const {
  assert(gadget_.num_marks_in_h_ == 0);
  return var_opt_sketch<T,S,A>(gadget_, true, n_);
}

// this is a condition checked in detect_and_handle_subcase_of_pseudo_exact()
template<typename T, typename S, typename A>
bool var_opt_union<T,S,A>::there_exist_unmarked_h_items_lighter_than_target(double threshold) const {
  for (int i = 0; i < gadget_.h_; ++i) {
    if ((gadget_.weights_[i] < threshold) && !gadget_.marks_[i]) {
      return true;
    }
  }
  return false;
}

template<typename T, typename S, typename A>
bool var_opt_union<T,S,A>::detect_and_handle_subcase_of_pseudo_exact(var_opt_sketch<T,S,A>& sk) const {
  // gadget is seemingly exact
  bool condition1 = gadget_.r_ == 0;

  // but there are marked items in H, so only _pseudo_ exact
  bool condition2 = gadget_.num_marks_in_h_ > 0;

  // if gadget is pseudo-exact and the number of marks equals outer_tau_denom, then we can deduce
  // from the bookkeeping logic of mergeInto() that all estimation mode input sketches must
  // have had the same tau, so we can throw all of the marked items into a common reservoir.
  bool condition3 = gadget_.num_marks_in_h_ == outer_tau_denom_;

  if (!(condition1 && condition2 && condition3)) {
    return false;
  } else {

    // explicitly enforce rule that items in H should not be lighter than the sketch's tau
    bool anti_condition4 = there_exist_unmarked_h_items_lighter_than_target(gadget_.get_tau());
    if (anti_condition4) {
      return false;
    } else {
      // conditions 1 through 4 hold
      mark_moving_gadget_coercer(sk);
      return true;
    }
  }
}

/**
 * This coercer directly transfers marked items from the gadget's H into the result's R.
 * Deciding whether that is a valid thing to do is the responsibility of the caller. Currently,
 * this is only used for a subcase of pseudo-exact, but later it might be used by other
 * subcases as well.
 *
 * @param sk Copy of the gadget, modified with marked items moved to the reservoir
 */
template<typename T, typename S, typename A>
void var_opt_union<T,S,A>::mark_moving_gadget_coercer(var_opt_sketch<T,S,A>& sk) const {
  uint32_t result_k = gadget_.h_ + gadget_.r_;

  uint32_t result_h = 0;
  uint32_t result_r = 0;
  size_t next_r_pos = result_k; // = (result_k+1)-1, to fill R region from back to front

  typedef typename std::allocator_traits<A>::template rebind_alloc<double> AllocDouble;
  double* wts = AllocDouble().allocate(result_k + 1);
  T* data     = A().allocate(result_k + 1);
    
  // insert R region items, ignoring weights
  // Currently (May 2017) this next block is unreachable; this coercer is used only in the
  // pseudo-exact case in which case there are no items natively in R, only marked items in H
  // that will be moved into R as part of the coercion process.
  // Addedndum (Jan 2020): Cleanup at end of method assumes R count is 0
  size_t final_idx = gadget_.get_num_samples();
  for (size_t idx = gadget_.h_ + 1; idx <= final_idx; ++idx) {
    data[next_r_pos] = gadget_.data_[idx];
    wts[next_r_pos]  = gadget_.weights_[idx];
    ++result_r;
    --next_r_pos;
  }
  
  double transferred_weight = 0;

  // insert H region items
  for (size_t idx = 0; idx < gadget_.h_; ++idx) {
    if (gadget_.marks_[idx]) {
      data[next_r_pos] = gadget_.data_[idx];
      wts[next_r_pos] = -1.0;
      transferred_weight += gadget_.weights_[idx];
      ++result_r;
      --next_r_pos;
    } else {
      data[result_h] = gadget_.data_[idx];
      wts[result_h] = gadget_.weights_[idx];
      ++result_h;
    }
  }

  assert((result_h + result_r) == result_k);
  assert(fabs(transferred_weight - outer_tau_numer_) < 1e-10);

  double result_r_weight = gadget_.total_wt_r_ + transferred_weight;
  uint64_t result_n = n_;
    
  // explicitly set weight value for the gap
  //data[result_h] = nullptr;  invalid not a pointer
  wts[result_h] = -1.0;

  // clean up arrays in input sketch, replace with new values
  typedef typename std::allocator_traits<A>::template rebind_alloc<bool> AllocBool;
  AllocBool().deallocate(sk.marks_, sk.curr_items_alloc_);
  AllocDouble().deallocate(sk.weights_, sk.curr_items_alloc_);
  for (size_t i = 0; i < result_k; ++i) { A().destroy(sk.data_ + i); } // assumes everything in H region, no gap
  A().deallocate(sk.data_, sk.curr_items_alloc_);

  sk.data_ = data;
  sk.weights_ = wts;
  sk.marks_ = nullptr;
  sk.num_marks_in_h_ = 0;
  sk.curr_items_alloc_ = result_k + 1;
  sk.k_ = result_k;
  sk.n_ = result_n;
  sk.h_ = result_h;
  sk.r_ = result_r;
  sk.total_wt_r_ = result_r_weight;
}

// this is basically a continuation of get_result(), but modifying the input gadget copy
template<typename T, typename S, typename A>
void var_opt_union<T,S,A>::migrate_marked_items_by_decreasing_k(var_opt_sketch<T,S,A>& gcopy) const {
  uint32_t r_count = gcopy.r_;
  uint32_t h_count = gcopy.h_;
  uint32_t k = gcopy.k_;

  assert(gcopy.num_marks_in_h_ > 0); // ensured by caller
  // either full (of samples), or in pseudo-exact mode, or both
  assert((r_count == 0) || (k == (h_count + r_count)));

  // if non-full and pseudo-exact, change k so that gcopy is full
  if ((r_count == 0) && (h_count < k)) {
    gcopy.k_ = h_count; // may leve extra space allocated but that's ok
  }

  // Now k equals the number of samples, so reducing k will increase tau.
  // Also, we know that there are at least 2 samples because 0 or 1 would have been handled
  // by the earlier logic in get_result()
  assert(gcopy.k_ >= 2);
  gcopy.decrease_k_by_1();

  // gcopy is now in estimation mode, just like the final result must be (due to marked items)
  assert(gcopy.r_ > 0);
  assert(gcopy.get_tau() > 0.0);

  // keep reducing k until all marked items have been absorbed into the reservoir
  while (gcopy.num_marks_in_h_ > 0) {
    assert(gcopy.k_ >= 2); // because h_ and r_ are both at least 1
    gcopy.decrease_k_by_1();
  }

  gcopy.strip_marks();
}


} // namespace datasketches

#endif // _VAR_OPT_UNION_IMPL_HPP_
