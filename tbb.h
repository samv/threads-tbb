#ifndef _perl_tbb_h_
#define _perl_tbb_h_

#include "tbb/task_scheduler_init.h"
#include "tbb/blocked_range.h"
#include "tbb/tbb_stddef.h"
#include "tbb/concurrent_vector.h"
#include "tbb/concurrent_hash_map.h"
#include <iterator>

// these classes are bound via XS to user code.
class perl_tbb_blocked_int : public tbb::blocked_range<int> {
 public:
 perl_tbb_blocked_int( int min, int max, int grain ) :
  tbb::blocked_range<int>(min, max, grain)
    { };
};

typedef tbb::concurrent_vector<SV*> perl_concurrent_vector;

class perl_tbb_init : public tbb::task_scheduler_init {
public:
 perl_tbb_init(int num_thr = automatic) : threads(num_thr) {
    initialize( threads );
  }
  ~perl_tbb_init() { }
private:
  int threads;
  int id;
};

/*
 * the following code concerns itself with getting a new Perl
 * interpreter at the beginning of a task body.
 *
 * usage:
 *  tbb_interpreter_lock interp;
 *  tbb_interpreter_pool->grab( interp );
 *
 * it has to:
 *    1. check that this real thread has an interpreter or not
 *       already, and
 *    2. start it, if it does.
 *    3. lock it, for the duration of the thread.
 *
 * A single tbb::concurrent_hash_map (tbb_interpreter_pool) is used
 * for all this.  It is a hash map from the thread ID (a pthread_t on
 * Unix, DWORD on Windows as in tbb itself) to a bool which indicates
 * whether or not the thread has been started.
 *
 * As the accessor 'class' represents an exclusive lock on the slot,
 * we use it for an interpreter mutex as well.  The first time it is
 * read, if its value is false then a PerlInterpreter is created.
 *
 */

#if _WIN32||_WIN64
#define raw_thread_id DWORD
#define get_raw_thread_id() GetCurrentThreadId()
#else
#define raw_thread_id pthread_t
#define get_raw_thread_id() pthread_self()
#endif

// for the concurrent_hash_map: necessary transformation and
// comparison functions.
struct raw_thread_hash_compare {
  static size_t hash( const raw_thread_id& x ) {
    size_t h = 0;
    if (sizeof(raw_thread_id) != sizeof(size_t) ) {
      int i = 0;
      for (const char* s = (char*)x; i<sizeof(raw_thread_id); ++s) {
	h = (h*17) ^ *s;
	i++;
      }
    }
    else {
      h = *( (size_t*)x );
    }
    return h;
  }
  static bool equal( const raw_thread_id& a, const raw_thread_id& b) {
    return (a == b);
  }
};

class perl_interpreter_pool;

// this is our global.  The interpreter pointers themselves are stored
// only in pthread-specific space for now.
static perl_interpreter_pool* tbb_interpreter_pool;

// this class just makes it easier to grab the accessor for the map.
class perl_interpreter_pool : public tbb::concurrent_hash_map<raw_thread_id, bool, raw_thread_hash_compare> {
 public:
  bool grab( perl_interpreter_pool::accessor& result);
};

typedef perl_interpreter_pool::accessor tbb_interpreter_lock;


#endif

