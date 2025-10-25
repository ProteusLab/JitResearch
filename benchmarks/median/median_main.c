// See LICENSE for license details.

//**************************************************************************
// Median filter bencmark
//--------------------------------------------------------------------------
//
// This benchmark performs a 1D three element median filter. The
// input data (and reference data) should be generated using the
// median_gendata.pl perl script and dumped to a file named
// dataset1.h.

#include "median.h"

//--------------------------------------------------------------------------
// Input/Reference Data

#include "dataset1.h"

//--------------------------------------------------------------------------
// Main

static int verify(int n, const int *test, const int *verify) {
  int i;
  // Unrolled for faster verification
  for (i = 0; i < n / 2 * 2; i += 2) {
    int t0 = test[i], t1 = test[i + 1];
    int v0 = verify[i], v1 = verify[i + 1];
    if (t0 != v0)
      return i + 1;
    if (t1 != v1)
      return i + 2;
  }
  if (n % 2 != 0 && test[n - 1] != verify[n - 1])
    return n;
  return 0;
}


int start()
{
  int results_data[DATA_SIZE];

#if PREALLOCATE
  // If needed we preallocate everything in the caches
  median( DATA_SIZE, input_data, results_data );
#endif

  // Do the filter
  median( DATA_SIZE, input_data, results_data );

  // Check the results
  return verify( DATA_SIZE, results_data, verify_data );
}

int main() {
  start();
  asm("addi x17, x0,  93; ecall");
}
