#ifdef __cplusplus
extern "C" {
#define PERL_NO_GET_CONTEXT /* we want efficiency! */
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"

void boot_DynaLoader(pTHX_ CV* cv);

static void xs_init(pTHX) {
	dXSUB_SYS;
	newXS((char*)"DynaLoader::boot_DynaLoader", boot_DynaLoader, (char*)__FILE__);
}

static const char* argv[] = {"", "-e", "0"};
static int argc = sizeof argv / sizeof *argv;

}
#endif

#include "tbb.h"


void perl_interpreter_pool::grab( perl_interpreter_pool::accessor& lock) {
	raw_thread_id thread_id = get_raw_thread_id();
	if (!tbb_interpreter_pool.find( lock, thread_id )) {
#ifdef DEBUG_PERLCALL
		IF_DEBUG(fprintf(stderr, "thr %x: first post!\n", thread_id));
#endif
		tbb_interpreter_pool.insert( lock, thread_id );
		lock->second = true;

		// start an interpreter!  fixme: load some code :)
		PerlInterpreter* my_perl = perl_alloc();
#ifdef DEBUG_PERLCALL
		IF_DEBUG(fprintf(stderr, "thr %x: allocated an interpreter\n", thread_id));
#endif
		PERL_SET_CONTEXT(my_perl);
		perl_construct(my_perl);
		IF_DEBUG_PERLCALL("thr %x: perl_construct\n", thread_id);
		PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
		perl_parse(my_perl, xs_init, argc, (char**)argv, NULL);
		IF_DEBUG_PERLCALL( "thr %x: perl_parse\n", thread_id);

		// we also need to synchronise this perl's @INC with ours...
		ENTER;
		load_module(PERL_LOADMOD_NOIMPORT, newSVpv("threads::tbb", 0), NULL, NULL);
		LEAVE;
		IF_DEBUG_PERLCALL( "thr %x: load_module\n", thread_id);
	}
}

void perl_tbb_init::mark_master_thread_ok() {
	perl_interpreter_pool::accessor lock;
	raw_thread_id thread_id = get_raw_thread_id();
	IF_DEBUG_PERLCALL( "thr %x: am master thread\n", thread_id);
	tbb_interpreter_pool.insert( lock, thread_id );
	lock->second = true;
}

#ifdef PERL_IMPLICIT_CONTEXT
static int yar_implicit_context;
#else
static int no_implicit_context;
#endif

static bool aTHX;

// a parallel_for body class that works with blocked_int range type
// only.
void perl_map_int_body::operator()( const perl_tbb_blocked_int& r ) const {
	perl_interpreter_pool::accessor interp;
	bool ah_true = true;
	raw_thread_id thread_id = get_raw_thread_id();
	tbb_interpreter_pool.grab( interp );
	IF_DEBUG(_warn("thr %x: processing range [%d,%d)\n", thread_id, r.begin(), r.end() ));

	SV *isv, *inv, *range;
	perl_map_int_body body_copy = *this;
	perl_tbb_blocked_int r_copy = r;

	// this declares and loads 'my_perl' variables from TLS
	dTHX;
	IF_DEBUG_PERLCALL( "thr %x: my_perl = %x\n", thread_id, my_perl );

	// declare and copy the stack pointer from global
	dSP;
	IF_DEBUG_PERLCALL( "thr %x: (dSP ok)\n", thread_id );

	// if we are to be creating temporary values, we need these:
	ENTER;
	SAVETMPS;
	IF_DEBUG_PERLCALL( "thr %x: (ENTER/SAVETMPS ok)\n", thread_id );

	//   // take a mental note of the current stack pointer
	PUSHMARK(SP);
	IF_DEBUG_PERLCALL( "thr %x: (PUSHMARK ok)\n", thread_id );

	isv = newSV(0);
	inv = sv_2mortal( sv_setref_pv(isv, "threads::tbb::map_int_body", &body_copy ));
	SvREFCNT_inc(inv);
	XPUSHs(inv);
	IF_DEBUG_PERLCALL( "thr %x: (map_int_body ok)\n", thread_id );

	isv = newSV(0);
	range = sv_2mortal( sv_setref_pv(isv, "threads::tbb::blocked_int", &r_copy ));
	SvREFCNT_inc(range);
	XPUSHs(range);
	IF_DEBUG_PERLCALL( "thr %x: (blocked_int ok)\n", thread_id );

	//   // set the global stack pointer to the same as our local copy
	PUTBACK;
	IF_DEBUG_PERLCALL( "thr %x: (PUTBACK ok)\n", thread_id );

	//call_pv(this->methname.c_str(), G_VOID|G_EVAL);
	//   // in case stack was re-allocated
	SPAGAIN;
	IF_DEBUG_PERLCALL( "thr %x: (SPAGAIN ok)\n", thread_id );

	//   // remember to PUTBACK; if we remove values from the stack

	if (SvTRUE(ERRSV)) {
		IF_DEBUG_PERLCALL( "thr %x: ($@ thrown; %s)\n", thread_id, SvPV_nolen(ERRSV) );
		POPs;
		PUTBACK;
	}
	IF_DEBUG_PERLCALL( "thr %x: ($@ ok)\n", thread_id );

	//   // free up those temps & PV return value
	FREETMPS;
	IF_DEBUG_PERLCALL( "thr %x: (FREETMPS ok)\n", thread_id );
	LEAVE;
	IF_DEBUG_PERLCALL( "thr %x: (LEAVE ok)\n", thread_id );

	IF_DEBUG_PERLCALL( "thr %x: done processing range [%d,%d)\n",
			   thread_id, r.begin(), r.end() );
};

