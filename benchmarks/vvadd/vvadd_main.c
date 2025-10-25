// See LICENSE for license details.

//**************************************************************************
// Vector-vector add benchmark
//--------------------------------------------------------------------------
//
// This benchmark uses adds to vectors and writes the results to a
// third vector. The input data (and reference data) should be
// generated using the vvadd_gendata.pl perl script and dumped
// to a file named dataset1.h.


//--------------------------------------------------------------------------
// Input/Reference Data

#include "dataset1.h"

//--------------------------------------------------------------------------
// vvadd function

void vvadd( int n, int a[], int b[], int c[] )
{
  int i;
  for ( i = 0; i < n; i++ )
    c[i] = a[i] + b[i];
}

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
  vvadd( DATA_SIZE, input1_data, input2_data, results_data );
#endif

  // Do the vvadd
  vvadd( DATA_SIZE, input1_data, input2_data, results_data );

  // Check the results
  return verify( DATA_SIZE, results_data, verify_data );
}

int main() {
  start();
  asm("addi x17, x0,  93; ecall");
}