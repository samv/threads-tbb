#ifndef _perl_tbb_h_
#define _perl_tbb_h_

#include "tbb/task_scheduler_init.h"
#include "tbb/blocked_range.h"
#include "tbb/tbb_stddef.h"
#include "tbb/concurrent_vector.h"
#include <iterator>

class perl_tbb_init : public tbb::task_scheduler_init {
public:
 perl_tbb_init(int num_thr = automatic) :
  threads(num_thr) {
    initialize( threads );
    // check the 
  }
  ~perl_tbb_init() {}

private:
  int threads;
};

class perl_tbb_blocked_int : public tbb::blocked_range<int> {
 public:
 perl_tbb_blocked_int( int min, int max, int grain ) :
  tbb::blocked_range<int>(min, max, grain)
    { };
};

typedef tbb::concurrent_vector<SV*> perl_concurrent_vector;

#endif

