#ifdef __cplusplus
extern "C" {
#define PERL_NO_GET_CONTEXT /* we want efficiency! */
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"

void boot_DynaLoader(pTHX_ CV* cv);

}
#endif

#include "tbb.h"

using namespace std;
using namespace tbb;

static void xs_init(pTHX) {
	dXSUB_SYS;
	newXS((char*)"DynaLoader::boot_DynaLoader", boot_DynaLoader, (char*)__FILE__);
}

static const char* argv[] = {"", "-e", "0"};
static int argc = sizeof argv / sizeof *argv;

void perl_interpreter_pool::grab( perl_interpreter_pool::accessor& lock, perl_tbb_init* init ) {
	raw_thread_id thread_id = get_raw_thread_id();
	PerlInterpreter* my_perl;
	bool fresh = false;
	if (!tbb_interpreter_pool.find( lock, thread_id )) {
#ifdef DEBUG_PERLCALL
		IF_DEBUG(fprintf(stderr, "thr %x: first post!\n", thread_id));
#endif
		tbb_interpreter_pool.insert( lock, thread_id );
		lock->second = true;

		// start an interpreter!  fixme: load some code :)
		my_perl = perl_alloc();
#ifdef DEBUG_PERLCALL
		IF_DEBUG(fprintf(stderr, "thr %x: allocated an interpreter\n", thread_id));
#endif
		// probably unnecessary
		PERL_SET_CONTEXT(my_perl);
		perl_construct(my_perl);

		// execute END blocks in perl_destruct
		PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
		perl_parse(my_perl, xs_init, argc, (char**)argv, NULL);

		// signal to the threads::tbb module that it's a child
		SV* worker_sv = get_sv("threads::tbb::worker", GV_ADD|GV_ADDMULTI);
		//SvUPGRADE(worker_sv, SVt_IV);
		sv_setiv(worker_sv, 1);

		// setup the @INC
		init->setup_worker_inc(aTHX);

		ENTER;
		load_module(PERL_LOADMOD_NOIMPORT, newSVpv("threads::tbb", 0), NULL, NULL);
		LEAVE;
		fresh = true;
	}
	else {
		my_perl = PERL_GET_THX;
	}
	
	AV* worker_av = get_av("threads::tbb::worker", GV_ADD|GV_ADDMULTI);
	// maybe required...
	//av_extend(worker_av, init->seq);
	SV* flag = *av_fetch( worker_av, init->seq, 1 );
	if (!SvOK(flag)) {
		if (!fresh) {
			init->setup_worker_inc(aTHX);
		}
		init->load_modules(my_perl);
		sv_setiv(flag, 1);
	}
}

void perl_tbb_init::mark_master_thread_ok() {
	perl_interpreter_pool::accessor lock;
	raw_thread_id thread_id = get_raw_thread_id();
	IF_DEBUG_INIT( "thr %x: am master thread\n", thread_id);
	tbb_interpreter_pool.insert( lock, thread_id );
	lock->second = true;
}

void perl_tbb_init::setup_worker_inc( pTHX ) {
	// first, set up the boot_lib
	list<string>::const_reverse_iterator lrit;
	
	// grab @INC and %INC
	AV* INC_a = get_av("INC", GV_ADD|GV_ADDWARN);

	// add all the lib paths to our INC
	for ( lrit = boot_lib.rbegin(); lrit != boot_lib.rend(); lrit++ ) {
		bool found = false;
		IF_DEBUG(fprintf(stderr, "thr %x: checking @INC for %s\n", get_raw_thread_id(),lrit->c_str() ));
		for ( int i = 0; i <= av_len(INC_a); i++ ) {
			SV** slot = av_fetch(INC_a, i, 0);
			if (!slot || !SvPOK(*slot))
				continue;
			if ( lrit->compare( SvPV_nolen(*slot) ) )
				continue;

			found = true;
			break;
		}
		if (found) {
			IF_DEBUG(fprintf(stderr, "thr %x: %s in @INC already\n", get_raw_thread_id(),lrit->c_str() ));
		}
		else {
			av_unshift( INC_a, 1 );
			SV* new_path = newSVpv(lrit->c_str(), 0);
			IF_DEBUG(fprintf(stderr, "thr %x: added %s to @INC\n", get_raw_thread_id(),lrit->c_str() ));
			SvREFCNT_inc(new_path);
			av_store( INC_a, 0, new_path );
		}
	}
}

void perl_tbb_init::load_modules( pTHX ) {
	HV* INC_h = get_hv("INC", GV_ADD|GV_ADDWARN);

	std::list<std::string>::const_iterator mod;

	for ( mod = boot_use.begin(); mod != boot_use.end(); mod++ ) {
		// skip if already in INC
		const char* modfilename = (*mod).c_str();
		size_t modfilename_len = strlen(modfilename);
		SV** slot = hv_fetch( INC_h, modfilename, modfilename_len, 0 );
		if (slot) {
			IF_DEBUG_INIT("skipping %s; already loaded", modfilename);
		}
		else {
			IF_DEBUG_INIT("require '%s'", modfilename);
			ENTER;
			require_pv(modfilename);
			LEAVE;
		}
	}
}

#ifdef PERL_IMPLICIT_CONTEXT
static int yar_implicit_context;
#else
static int no_implicit_context;
#endif

static bool aTHX;

// a parallel_for body class that works with blocked_int range type
// only.
void perl_for_int_array_func::operator()( const perl_tbb_blocked_int& r ) const {
	perl_interpreter_pool::accessor interp;
	bool ah_true = true;
	raw_thread_id thread_id = get_raw_thread_id();
	tbb_interpreter_pool.grab( interp, this->context );
	IF_DEBUG(_warn("thr %x: processing range [%d,%d)\n", thread_id, r.begin(), r.end() ));

	SV *isv, *inv, *range;
	perl_for_int_array_func body_copy = *this;
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
	inv = sv_2mortal( sv_setref_pv(isv, "threads::tbb::for_int_array_func", &body_copy ));
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

	IF_DEBUG(_warn("thr %x: calling %s\n", thread_id, this->funcname.c_str() ));
	call_pv(this->funcname.c_str(), G_VOID|G_EVAL);
	//   // in case stack was re-allocated
	SPAGAIN;
	IF_DEBUG_PERLCALL( "thr %x: (SPAGAIN ok)\n", thread_id );

	//   // remember to PUTBACK; if we remove values from the stack

	if (SvTRUE(ERRSV)) {
		warn( "error processing range [%d,%d); %s",
		      r.begin(), r.end(), SvPV_nolen(ERRSV) );
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

