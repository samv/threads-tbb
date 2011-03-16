#ifdef __cplusplus
extern "C" {
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
}
#endif

#include "tbb.h"

bool perl_interpreter_pool::grab( tbb_interpreter_lock& result) {
    raw_thread_id thread_id = get_raw_thread_id();
    bool rv = tbb_interpreter_pool->find( result, thread_id ); 
    if (!result->second) {
      // boot the interpreter!
      perl_alloc();

      // should also make sure we're up to date with modules etc.  but
      // later!
      result->second = true;
    }
    return rv;
}
