#ifdef __cplusplus
extern "C" {
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
}
#endif

#include "tbb.h"

void perl_interpreter_pool::grab( perl_interpreter_pool::accessor& lock) {
    raw_thread_id thread_id = get_raw_thread_id();
    if (!tbb_interpreter_pool.find( lock, thread_id )) {
      fprintf(stderr, "thr %x: first post!\n", thread_id);
      tbb_interpreter_pool.insert( lock, thread_id );
      lock->second = true;

      // start an interpreter!  fixme: load some code :)
      //perl_alloc();
    }
}

// a parallel_for body class that works with blocked_int range type
// only.
void perl_map_int_body::operator()( const perl_tbb_blocked_int& r ) const {
  perl_interpreter_pool::accessor interp;
  bool ah_true = true;
    raw_thread_id thread_id = get_raw_thread_id();
    fprintf(stderr, "thr %x: processing range [%d,%d)\n",
	    thread_id, r.begin(), r.end());
    fprintf(stderr, "thr %x: interp is (tbb_interpreter_lock*)%x\n",
	    thread_id, &interp);
    tbb_interpreter_pool.grab( interp );

//   SV *isv, *inv, *range;
//   perl_map_int_body body_copy = *this;
//   perl_tbb_blocked_int r_copy = r;
  
//   // this declares and loads 'my_perl' variables from TLS
//   dTHX;

//   // declare and copy the stack pointer from global
//   dSP;
  
//   // if we are to be creating temporary values, we need these:
//   ENTER;
//   SAVETMPS;

//   // take a mental note of the current stack pointer
//   PUSHMARK(SP);

//   isv = newSV(0);
//   inv = sv_2mortal( sv_setref_pv(isv, "threads::tbb::map_int_body", &body_copy ));
//   mXPUSHs(inv);

//   isv = newSV(0);
//   range = sv_2mortal( sv_setref_pv(isv, "threads::tbb::blocked_int", &r_copy ));
//   mXPUSHs(range);

//   // set the global stack pointer to the same as our local copy
//   PUTBACK;

//   call_pv(this->methname.c_str(), G_VOID|G_EVAL);
//   // in case stack was re-allocated
//   SPAGAIN;

//   // remember to PUTBACK; if we remove values from the stack

//   if (SvTRUE(ERRSV)) {
//     POPs;
//     PUTBACK;
//   }

//   // free up those temps & PV return value
//   FREETMPS;
//   LEAVE;

};

