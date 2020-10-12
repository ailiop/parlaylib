// This file is basically the cache-oblivious sorting algorithm from:
//
// Low depth cache-oblivious algorithms.
// Guy E. Blelloch, Phillip B. Gibbons and  Harsha Vardhan Simhadri.
// Proc. ACM symposium on Parallelism in algorithms and architectures (SPAA),
// 2010

#ifndef PARLAY_SAMPLE_SORT_H_
#define PARLAY_SAMPLE_SORT_H_

#include <cmath>
#include <cstdio>
#include <cstring>

#include "bucket_sort.h"
#include "quicksort.h"
#include "sequence_ops.h"
#include "transpose.h"

#include "../utilities.h"

namespace parlay {
namespace internal {

// the following parameters can be tuned
constexpr const size_t QUICKSORT_THRESHOLD = 16384;
constexpr const size_t OVER_SAMPLE = 8;

// generates counts in Sc for the number of keys in Sa between consecutive
// values of Sb. Sa and Sb must be sorted
template <typename InIterator, typename PivotIterator, typename CountIterator, typename Compare>
void get_bucket_counts(slice<InIterator, InIterator> sA,
                       slice<PivotIterator, PivotIterator> sB,
                       slice<CountIterator, CountIterator> sC,
                       Compare f) {
  if (sA.size() == 0 || sB.size() == 0) return;
  for (auto& c : sC) c = 0;
  auto itA = sA.begin();
  auto itB = sB.begin();
  auto itC = sC.begin();
  while (true) {
    while (f(*itA, *itB)) {
      assert(itC != sC.end());
      (*itC)++;
      if (++itA == sA.end()) return;
    }
    itB++;
    itC++;
    if (itB == sB.end()) break;
    if (!(f(*(itB - 1), *itB))) {
      while (!f(*itB, *itA)) {
        assert(itC != sC.end());
        (*itC)++;
        if (++itA == sA.end()) return;
      }
      itB++;
      itC++;
      if (itB == sB.end()) break;
    }
  }
  assert(itC != sC.end());
  *itC = sA.end() - itA;
}

template <typename Iterator, typename Compare>
void seq_sort_inplace(slice<Iterator, Iterator> A, const Compare& less, bool stable) {
  using value_type = typename slice<Iterator, Iterator>::value_type;
  if (((sizeof(value_type) > 8) || std::is_pointer<value_type>::value) && !stable)
    quicksort(A.begin(), A.size(), less);
  else
    bucket_sort(A, less, stable);
}

template <typename assignment_tag, typename InIterator, typename OutIterator, typename Compare>
void seq_sort_(slice<InIterator, InIterator> In,
               slice<OutIterator, OutIterator> Out,
               const Compare& less,
               bool stable = false) {
  size_t l = In.size();
  for (size_t j = 0; j < l; j++) assign_dispatch(Out[j], In[j], assignment_tag());
  seq_sort_inplace(Out, less, stable);
}


// Fully in place version of sample sort. Makes no copies of any elements
// in the input array. This version can not be stable, unfortunately.
template <typename s_size_t = size_t,
          typename InIterator, typename OutIterator,
          typename Compare>
void sample_sort_inplace_(slice<InIterator, InIterator> In,
                  slice<OutIterator, OutIterator> Out,
                  const Compare& less) {
  using value_type = typename slice<InIterator, InIterator>::value_type;
  size_t n = In.size();

  if (n < QUICKSORT_THRESHOLD) {
    seq_sort_inplace(In, less, false);
  } else {
    // The larger these are, the more comparisons are done but less
    // overhead for the transpose.
    size_t bucket_quotient = 4;
    size_t block_quotient = 4;
    if (std::is_pointer<value_type>::value) {
      bucket_quotient = 2;
      block_quotient = 3;
    } else if (sizeof(value_type) > 8) {
      bucket_quotient = 3;
      block_quotient = 3;
    }
    size_t sqrt = static_cast<size_t>(std::sqrt(n));
    size_t num_blocks = 1 << log2_up((sqrt / block_quotient) + 1);
    size_t block_size = ((n - 1) / num_blocks) + 1;
    size_t num_buckets = (sqrt / bucket_quotient) + 1;
    size_t sample_set_size = block_size;
    size_t m = num_blocks * num_buckets;

    // We want to select evenly spaced pivots from the sorted samples
    // Since we sampled sample_set_size many elements, and we need
    // exactly num_buckets - 1, we sample at stride:
    assert(sample_set_size >= num_buckets - 1);
    size_t stride = sample_set_size / (num_buckets - 1);
    assert(stride >= 1);

    // In place sampling!. Just swap elements to the front of
    // the sequence to be used as the samples. This way, no
    // copying is required! This is essentially just k iterations
    // of a Knuth shuffle. Note that we cleverly copy exactly
    // one block size worth of samples.
    for (size_t i = 0; i < sample_set_size; i++) {
      size_t j = i + hash64(i) % (n - i);
      std::swap(In[i], In[j]);
    }

    // sort the samples
    auto sample_set = make_slice(In.begin(), In.begin() + sample_set_size);
    quicksort(sample_set.begin(), sample_set_size, less);

    // Pivots returns by reference to avoid making copies
    auto pivots = delayed_seq<const value_type&>(num_buckets - 1, [&](size_t i) -> const value_type& {
      assert(stride * i < sample_set_size);
      return sample_set[stride * i];
    });

    // Note that at this point, the samples occupy exactly one block.
    // They are also already sorted, so we don't need to sort the
    // first block, which is good, because we don't want the pivots
    // moving around while we use them to compute bucket counts
    // for all of the other blocks!

    // sort each block and merge with samples to get counts for each bucket
    auto Tmp = uninitialized_sequence<value_type>(n);
    auto counts = sequence<s_size_t>::uninitialized(m + 1);
    counts[m] = 0;

    auto In_sub = In.cut(sample_set_size, n);
    auto Tmp_sub = make_slice(Tmp).cut(sample_set_size, n);
    auto counts_sub = make_slice(counts).cut(num_buckets, m + 1);

    sliced_for(n - sample_set_size, block_size, [&](size_t i, size_t start, size_t end) {
      seq_sort_<uninitialized_relocate_tag>(In_sub.cut(start, end),
                                            Tmp_sub.cut(start, end), less, false);
      get_bucket_counts(Tmp_sub.cut(start, end), make_slice(pivots),
                        counts_sub.cut(i * num_buckets, (i + 1) * num_buckets), less);
    });

    // Pivot block is already sorted, so we don't need to sort it again
    // We can just move it straight over into the other sorted blocks
    // and then compute the bucket counts
    uninitialized_relocate_n(Tmp.begin(), sample_set.begin(), sample_set_size);
    get_bucket_counts(make_slice(Tmp).cut(0, sample_set_size), make_slice(pivots), make_slice(counts).cut(0, num_buckets), less);

    // move data from blocks to buckets
    auto bucket_offsets =
        transpose_buckets<uninitialized_relocate_tag>(Tmp.begin(), Out.begin(), counts, n, block_size,
                          num_blocks, num_buckets);

    // sort within each bucket
    parallel_for(0, num_buckets, [&](size_t i) {
      size_t start = bucket_offsets[i];
      size_t end = bucket_offsets[i + 1];
      // This could be optimized by not sorting a bucket if its two adjacent
      // pivots were equal, since this means that all of the contents of the
      // bucket are equal. But we don't know where the pivots are anymore
      // since we just merged them in. We could chose to precompute flags
      // indicating which adjacent pivots are equal, but is it worth it?
      seq_sort_inplace(Out.cut(start, end), less, false);
     }, 1);
  }
}

// Copying version of sample sort. This one makes copies of the input
// elements when sorting them into the output. Roughly (\sqrt{n})
// additional copies are also made to copy the pivots. This one can
// be stable.
template <typename s_size_t = size_t,
    typename InIterator, typename OutIterator,
    typename Compare>
void sample_sort_(slice<InIterator, InIterator> In,
                          slice<OutIterator, OutIterator> Out,
                          const Compare& less,
                          bool stable = false) {
  using value_type = typename slice<InIterator, InIterator>::value_type;
  size_t n = In.size();

  if (n < QUICKSORT_THRESHOLD) {
    seq_sort_<uninitialized_copy_tag>(In, Out, less, stable);
  } else {
    // The larger these are, the more comparisons are done but less
    // overhead for the transpose.
    size_t bucket_quotient = 4;
    size_t block_quotient = 4;
    if (std::is_pointer<value_type>::value) {
      bucket_quotient = 2;
      block_quotient = 3;
    } else if (sizeof(value_type) > 8) {
      bucket_quotient = 3;
      block_quotient = 3;
    }
    size_t sqrt = static_cast<size_t>(std::sqrt(n));
    size_t num_blocks = 1 << log2_up((sqrt / block_quotient) + 1);
    size_t block_size = ((n - 1) / num_blocks) + 1;
    size_t num_buckets = (sqrt / bucket_quotient) + 1;
    size_t sample_set_size = num_buckets * OVER_SAMPLE;
    size_t m = num_blocks * num_buckets;

    // generate "random" samples with oversampling
    auto sample_set = sequence<value_type>::from_function(sample_set_size,
                                                          [&](size_t i) { return In[hash64(i) % n]; });
    // sort the samples
    quicksort(sample_set.begin(), sample_set_size, less);

    // subselect samples at even stride
    auto pivots = sequence<value_type>::from_function(num_buckets - 1,
                                                      [&](size_t i) { return sample_set[OVER_SAMPLE * i]; });

    auto Tmp = uninitialized_sequence<value_type>(n);

    // sort each block and merge with samples to get counts for each bucket
    auto counts = sequence<s_size_t>::uninitialized(m + 1);
    counts[m] = 0;
    sliced_for(n, block_size, [&](size_t i, size_t start, size_t end) {
      seq_sort_<uninitialized_copy_tag>(In.cut(start, end), make_slice(Tmp).cut(start, end), less, stable);
      get_bucket_counts(make_slice(Tmp).cut(start, end), make_slice(pivots),
                        make_slice(counts).cut(i * num_buckets, (i + 1) * num_buckets), less);
    });

    // move data from blocks to buckets
    auto bucket_offsets =
        transpose_buckets<uninitialized_relocate_tag>(Tmp.begin(), Out.begin(), counts, n, block_size,
                                                      num_blocks, num_buckets);

    // sort within each bucket
    parallel_for(0, num_buckets, [&](size_t i) {
      size_t start = bucket_offsets[i];
      size_t end = bucket_offsets[i + 1];

      // buckets need not be sorted if two consecutive pivots are equal
      if (i == 0 || i == num_buckets - 1 || less(pivots[i - 1], pivots[i])) {
        seq_sort_inplace(Out.cut(start, end), less, stable);
      }
    }, 1);
  }
}

template <typename Iterator, typename Compare>
auto sample_sort(slice<Iterator, Iterator> A,
                 const Compare& less,
                 bool stable = false) {
  using value_type = typename slice<Iterator, Iterator>::value_type;
  sequence<value_type> R = sequence<value_type>::uninitialized(A.size());
  if (A.size() < (std::numeric_limits<unsigned int>::max)()) {
    sample_sort_<unsigned int>(A, make_slice(R), less, stable);
  }
  else {
    sample_sort_<unsigned long long>(A, make_slice(R), less, stable);
  }
  return R;
}

template <class Iterator, typename Compare>
void sample_sort_inplace(slice<Iterator, Iterator> A,
                         const Compare& less) {
  if (A.size() < (std::numeric_limits<unsigned int>::max)()) {
    sample_sort_inplace_<unsigned int>(A, A, less);
  }
  else {
    sample_sort_inplace_<unsigned long long>(A, A, less);
  }
}

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_SAMPLE_SORT_H_
