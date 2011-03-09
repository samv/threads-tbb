#ifndef _perl_tbb_h_
#define _perl_tbb_h_

#include "tbb/task_scheduler_init.h"
#include "tbb/blocked_range.h"
#include "tbb/tbb_stddef.h"

extern "C" {
  int array_length(AV* array);
  AV* array_split(AV* array, int m, AV**new_self);
}

class perl_tbb_init : public tbb::task_scheduler_init {
public:
 perl_tbb_init(int num_thr = automatic) :
  threads(num_thr) {
    initialize( threads );
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

class perl_tbb_blocked_array {
 public:
  perl_tbb_blocked_array( AV* array, int grain ) :
  array(array), grain_size(grain)
        {};
  bool is_divisible() {
    return ( array_length(array) > grain_size);
  }
  int grainsize() {
    return grain_size;
  }
  int size() {
    return array_length(array);
  }
  perl_tbb_blocked_array( perl_tbb_blocked_array& r, tbb::split )
    {
      int m = r.size() / 2;
      array_split(r.array, m, &array);
    };
  AV* _get_array() {
    return array;
  }
 private:
  AV *array;
  int grain_size;
};

#endif

