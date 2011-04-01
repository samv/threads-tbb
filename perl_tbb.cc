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
		tbb_interpreter_pool.insert( lock, thread_id );

		{
			// grab a number!
			mutex_t::scoped_lock x(perl_tbb_worker_mutex);
			lock->second = ++perl_tbb_worker;
		}

		// start an interpreter!  fixme: load some code :)
		my_perl = perl_alloc();
#ifdef DEBUG_PERLCALL
		IF_DEBUG(fprintf(stderr, "thr %x: allocated an interpreter for worker %d\n", thread_id, lock->second));
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
		sv_setiv(worker_sv, lock->second);

		// setup the @INC
		init->setup_worker_inc(aTHX);

		ENTER;
		load_module(PERL_LOADMOD_NOIMPORT, newSVpv("threads::tbb", 0), NULL, NULL);
		LEAVE;
		IF_DEBUG_INIT("loaded threads::tbb");
#if IF_DEBUG(1)+0
		ENTER;
		load_module(PERL_LOADMOD_NOIMPORT, newSVpv("Devel::Peek", 0), NULL, NULL);
		IF_DEBUG_INIT("loaded Devel::Peek");
		LEAVE;
#endif
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
		if ( lock->second != 0 ) {
			IF_DEBUG_INIT("setting up worker %d for work", lock->second);
			if (!fresh) {
				init->setup_worker_inc(aTHX);
			}
			init->load_modules(my_perl);
		}
		else {
			IF_DEBUG_INIT("not setting up worker %d for work, master thread", lock->second);
		}
		sv_setiv(flag, 1);
	}
}

void perl_tbb_init::mark_master_thread_ok() {
	if (tbb_interpreter_pool.size() == 0) {
		perl_interpreter_pool::accessor lock;
		raw_thread_id thread_id = get_raw_thread_id();
		IF_DEBUG_INIT( "I am the master thread");
		tbb_interpreter_pool.insert( lock, thread_id );
		lock->second = 0;
	}
}

void perl_tbb_init::setup_worker_inc( pTHX ) {
	// first, set up the boot_lib
	list<string>::const_reverse_iterator lrit;
	
	// grab @INC
	AV* INC_a = get_av("INC", GV_ADD|GV_ADDWARN);

	// add all the lib paths to our INC
	for ( lrit = boot_lib.rbegin(); lrit != boot_lib.rend(); lrit++ ) {
		bool found = false;
		//IF_DEBUG(fprintf(stderr, "thr %x: checking @INC for %s\n", get_raw_thread_id(),lrit->c_str() ));
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
			//IF_DEBUG(fprintf(stderr, "thr %x: %s in @INC already\n", get_raw_thread_id(),lrit->c_str() ));
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
	// get %INC
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
	raw_thread_id thread_id = get_raw_thread_id();
	tbb_interpreter_pool.grab( interp, this->context );

	SV *isv, *inv, *range;
	perl_for_int_array_func body_copy = *this;
	perl_tbb_blocked_int r_copy = r;

	// this declares and loads 'my_perl' variables from TLS
	dTHX;

	// declare and copy the stack pointer from global
	dSP;

	// if we are to be creating temporary values, we need these:
	ENTER;
	SAVETMPS;

	//   // take a mental note of the current stack pointer
	PUSHMARK(SP);

	isv = newSV(0);
	inv = sv_2mortal( sv_setref_pv(isv, "threads::tbb::for_int_array_func", &body_copy ));
	SvREFCNT_inc(inv);
	XPUSHs(inv);

	isv = newSV(0);
	range = sv_2mortal( sv_setref_pv(isv, "threads::tbb::blocked_int", &r_copy ));
	SvREFCNT_inc(range);
	XPUSHs(range);

	//   // set the global stack pointer to the same as our local copy
	PUTBACK;

	IF_DEBUG_THR("calling %s", this->funcname.c_str() );
	call_pv(this->funcname.c_str(), G_VOID|G_EVAL);
	//   // in case stack was re-allocated
	SPAGAIN;

	//   // remember to PUTBACK; if we remove values from the stack

	if (SvTRUE(ERRSV)) {
		warn( "error processing range [%d,%d); %s",
		      r.begin(), r.end(), SvPV_nolen(ERRSV) );
		POPs;
		PUTBACK;
	}
	IF_DEBUG_PERLCALL( "($@ ok)" );

	//   // free up those temps & PV return value
	FREETMPS;
	IF_DEBUG_PERLCALL( "(FREETMPS ok)" );
	LEAVE;
	IF_DEBUG_PERLCALL( "(LEAVE ok)" );

	IF_DEBUG_PERLCALL( "done processing range [%d,%d)",
			   r.begin(), r.end() );
};

// this function might be made into a helper / base class at some point...
SV* perl_for_int_method::get_invocant( pTHX_ int worker ) {
	IF_DEBUG_PERLCALL( "getting invocant for worker %d", worker );
	copied->grow_to_at_least(worker+1);
	perl_concurrent_item x = (*copied)[worker];
	if (!x.thingy || (x.owner != my_perl)) {
		x = perl_concurrent_item( my_perl, invocant.clone( my_perl ) );
	}
	return x.dup( my_perl );
}

// body function for for_int_method
void perl_for_int_method::operator()( const perl_tbb_blocked_int& r ) const {

	perl_interpreter_pool::accessor interp;
	tbb_interpreter_pool.grab( interp, this->context );
	IF_DEBUG_PERLCALL("processing range [%d,%d)", r.begin(), r.end());

	SV *isv, *inv, *range;
	perl_for_int_method body_copy = *this;
	perl_tbb_blocked_int r_copy = r;

	// this declares and loads 'my_perl' variables from TLS
	dTHX;

	// declare and copy the stack pointer from global
	dSP;
	IF_DEBUG_PERLCALL( "(dSP ok)" );

	// if we are to be creating temporary values, we need these:
	ENTER;
	SAVETMPS;
	IF_DEBUG_PERLCALL( "(ENTER/SAVETMPS ok)" );

	// take a mental note of the current stack pointer
	PUSHMARK(SP);
	IF_DEBUG_PERLCALL( "(PUSHMARK ok)" );

	isv = newSV(0);
	inv = body_copy.get_invocant( my_perl, interp->second );
	IF_DEBUG_PERLCALL( "got invocant: %x", inv );
	sv_2mortal(inv);
	SvREFCNT_inc(inv);
	XPUSHs(inv);
#ifdef DEBUG_PERLCALL_PEEK
	PUTBACK;
	call_pv("Devel::Peek::Dump", G_VOID);
#else
	IF_DEBUG_PERLCALL( "(map_int_body ok)" );

	isv = newSV(0);
	range = sv_2mortal( sv_setref_pv(isv, "threads::tbb::blocked_int", &r_copy ));
	SvREFCNT_inc(range);
	XPUSHs(range);
	IF_DEBUG_PERLCALL( "(blocked_int ok)" );

	//   // set the global stack pointer to the same as our local copy
	PUTBACK;
	IF_DEBUG_PERLCALL( "(PUTBACK ok)" );

	IF_DEBUG_PERLCALL("calling method %s", this->methodname.c_str() );
	call_method(this->methodname.c_str(), G_VOID|G_EVAL);
	//   // in case stack was re-allocated
#endif
	SPAGAIN;
	IF_DEBUG_PERLCALL( "(SPAGAIN ok)" );

	//   // remember to PUTBACK; if we remove values from the stack

	if (SvTRUE(ERRSV)) {
		warn( "error processing range [%d,%d); %s",
		      r.begin(), r.end(), SvPV_nolen(ERRSV) );
		POPs;
		PUTBACK;
	}
	IF_DEBUG_PERLCALL( "($@ ok)" );

	//   // free up those temps & PV return value
	FREETMPS;
	IF_DEBUG_PERLCALL( "(FREETMPS ok)" );
	LEAVE;
	IF_DEBUG_PERLCALL( "(LEAVE ok)" );

	IF_DEBUG_PERLCALL( "done processing range [%d,%d)",
			   r.begin(), r.end() );
};

