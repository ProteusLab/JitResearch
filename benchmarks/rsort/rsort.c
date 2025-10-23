// See LICENSE for license details.

//**************************************************************************
// Radix Sort benchmark
//--------------------------------------------------------------------------
//
// This benchmark uses radix sort to sort an array of integers. The
// implementation is largely adapted from Numerical Recipes for C. The
// input data (and reference data) should be generated using the
// qsort_gendata.pl perl script and dumped to a file named
// dataset1.h

//--------------------------------------------------------------------------
// Input/Reference Data

#include "bench/utils.h"

#define type unsigned int
#include "dataset1.h"

#define LOG_BASE 8
#define BASE (1 << LOG_BASE)

#if 0
#define fetch_add(ptr, inc) __sync_fetch_and_add(ptr, inc)
#else
#define fetch_add(ptr, inc)  ((*(ptr) += (inc)) - (inc))
#define fetch_add1(ptr, inc)  (*(ptr) += (inc))
#endif

void memcpy_impl(void *dst, void const *src, unsigned n) {
  for (unsigned i = 0; i < n; ++i)
    ((char *)(dst))[i] = ((char *)(src))[i];
}

void sort(unsigned n, type *arrIn, type *scratchIn) {
  unsigned log_exp = 0;
  unsigned buckets[BASE];
  unsigned *bucket = buckets;
  asm("" : "+r"(bucket));
  type *arr = arrIn, *scratch = scratchIn, *p;
  unsigned *b;

  while (log_exp < 8 * sizeof(type)) {
    for (b = bucket; b < bucket + BASE; b++)
      *b = 0;

    for (p = arr; p < &arr[n - 3]; p += 4) {
      type a0 = p[0];
      type a1 = p[1];
      type a2 = p[2];
      type a3 = p[3];
      fetch_add1(&bucket[(a0 >> log_exp) % BASE], 1);
      fetch_add1(&bucket[(a1 >> log_exp) % BASE], 1);
      fetch_add1(&bucket[(a2 >> log_exp) % BASE], 1);
      fetch_add1(&bucket[(a3 >> log_exp) % BASE], 1);
    }
    for (; p < &arr[n]; p++)
      bucket[(*p >> log_exp) % BASE]++;

    unsigned prev = bucket[0];
    prev += fetch_add(&bucket[1], prev);
    for (b = &bucket[2]; b < bucket + BASE; b += 2) {
      prev += fetch_add(&b[0], prev);
      prev += fetch_add(&b[1], prev);
    }

    for (p = &arr[n - 1]; p >= &arr[3]; p -= 4) {
      type a0 = p[-0];
      type a1 = p[-1];
      type a2 = p[-2];
      type a3 = p[-3];
      unsigned *pb0 = &bucket[(a0 >> log_exp) % BASE];
      unsigned *pb1 = &bucket[(a1 >> log_exp) % BASE];
      unsigned *pb2 = &bucket[(a2 >> log_exp) % BASE];
      unsigned *pb3 = &bucket[(a3 >> log_exp) % BASE];
      type *s0 = scratch + fetch_add(pb0, -1);
      type *s1 = scratch + fetch_add(pb1, -1);
      type *s2 = scratch + fetch_add(pb2, -1);
      type *s3 = scratch + fetch_add(pb3, -1);
      s0[-1] = a0;
      s1[-1] = a1;
      s2[-1] = a2;
      s3[-1] = a3;
    }
    for (; p >= &arr[0]; p--)
      scratch[--bucket[(*p >> log_exp) % BASE]] = *p;

    type *tmp = arr;
    arr = scratch;
    scratch = tmp;

    log_exp += LOG_BASE;
  }
  if (arr != arrIn)
    memcpy_impl(arr, scratch, n * sizeof(type));
}

//--------------------------------------------------------------------------
// Main

int main() {
  static type scratch[DATA_SIZE];

#if PREALLOCATE
  // If needed we preallocate everything in the caches
  sort(DATA_SIZE, verify_data, scratch);
  if (verify(DATA_SIZE, input_data, input_data))
    return 1;
#endif

  // Do the sort
  sort(DATA_SIZE, input_data, scratch);

  // Check the results
  return verify(DATA_SIZE, (const int *)input_data, (const int *)verify_data);
}
