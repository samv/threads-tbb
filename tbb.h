#ifndef _perl_tbb_h_
#define _perl_tbb_h_

#include "tbb/task_scheduler_init.h"

class perl_tbb_init : public tbb::task_scheduler_init {
public:
  perl_tbb_init() {};
  ~perl_tbb_init() {}

private:
  int threads;
};

#endif

