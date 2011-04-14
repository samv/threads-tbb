#ifndef _perl_tbb_h_
#define _perl_tbb_h_

#include "tbb/task_scheduler_init.h"
#include "tbb/blocked_range.h"
#include "tbb/tbb_stddef.h"
#include "tbb/concurrent_queue.h"
#include "tbb/concurrent_vector.h"
#include "tbb/concurrent_hash_map.h"
#include "tbb/parallel_for.h"
#include <iterator>
#include <set>
#include <list>

#include "tbb/spin_mutex.h"
typedef tbb::spin_mutex      mutex_t;

#if __GNUC__ >= 3   /* I guess. */
#define _warn(msg, e...) warn("# (" __FILE__ ":%d): " msg, __LINE__, ##e)
#else
#define _warn warn
#endif

// see this source file for enabling IF_DEBUG_* macros
#include "debug.h"

//**
//*  Perl-mapped classes
//*/

// threads::tbb::blocked_int
class perl_tbb_blocked_int : public tbb::blocked_range<int> {
public:
perl_tbb_blocked_int( int min, int max, int grain ) :
	tbb::blocked_range<int>(min, max, grain)
	{ };
perl_tbb_blocked_int( perl_tbb_blocked_int& oth, tbb::split sp )
	: tbb::blocked_range<int>( oth, sp )
	{ };
};

// threads::tbb::concurrent::array
class perl_concurrent_slot;
class perl_concurrent_vector : public tbb::concurrent_vector<perl_concurrent_slot> {
public:
	int refcnt;
	perl_concurrent_vector() : refcnt(0) {}
};

class perl_concurrent_slot {
public:
	SV* thingy;
	PerlInterpreter* owner;
        perl_concurrent_slot( ) : thingy(0) {};
	perl_concurrent_slot( PerlInterpreter* owner, SV* thingy )
		: thingy(thingy), owner(owner) {};
	SV* dup( pTHX );    // get if same interpreter, clone otherwise
	SV* clone( pTHX );  // always clone
};

// same as perl_concurrent_slot, but with refcounting
class perl_concurrent_item : public perl_concurrent_slot {
public:
	int refcnt;
	perl_concurrent_item( ) : refcnt(0), perl_concurrent_slot() {};
	perl_concurrent_item( PerlInterpreter* owner, SV* thingy )
		: refcnt(0), perl_concurrent_slot(owner, thingy) {};
};

// threads::tbb::init
static int perl_tbb_init_seq = 0;
static mutex_t perl_tbb_init_seq_mutex;
class perl_tbb_init : public tbb::task_scheduler_init {
public:
	std::list<std::string> boot_lib;
	std::list<std::string> boot_use;
	int seq;  // process-unique ID

	perl_tbb_init( int num_thr ) : tbb::task_scheduler_init(num_thr) {
		mark_master_thread_ok();
		mutex_t::scoped_lock lock(perl_tbb_init_seq_mutex);
		seq = perl_tbb_init_seq++;
	}
	~perl_tbb_init() { }
	void mark_master_thread_ok();

	void setup_worker_inc( pTHX );
	void load_modules( pTHX );

private:
	int id;
};

// these are the types passed to parallel_for et al

// threads::tbb::for_int_array_func
// first a very simple one that allows an entry-point function to be called by
// name, with a sub-dividing integer range.

class perl_for_int_array_func {
	const std::string funcname;
	perl_tbb_init* context;
	perl_concurrent_vector* xarray;
public:
        perl_for_int_array_func( perl_tbb_init* context, perl_concurrent_vector* xarray, std::string funcname ) :
	funcname(funcname), context(context), xarray(xarray) { };
	perl_concurrent_vector* get_array() { return xarray; };
	void operator()( const perl_tbb_blocked_int& r ) const;
};

// threads::tbb::for_int_method
// this one allows a SV to be passed
class perl_for_int_method {
	perl_tbb_init* context;
	perl_concurrent_slot invocant;
        perl_concurrent_vector* copied;
public:
	std::string methodname;
perl_for_int_method( pTHX_ perl_tbb_init* context, SV* inv_sv, std::string methodname ) :
	context(context), methodname(methodname) {
		copied = new perl_concurrent_vector();
		SV* newsv = newSV(0);
		SvSetSV_nosteal(newsv, inv_sv);
		IF_DEBUG_PERLCALL("copied %x to %x (refcnt = %d)", inv_sv, newsv, SvREFCNT(newsv));
		invocant = perl_concurrent_slot(my_perl, newsv); 
	};
	SV* get_invocant( pTHX_ int worker );
	void operator()( const perl_tbb_blocked_int& r ) const;
};

/*
 * the following code concerns itself with getting a new Perl
 * interpreter at the beginning of a task body.
 *
 * usage:
 *  perl_interpreter_pool::accessor interp;
 *  tbb_interpreter_pool->grab( interp, init );
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
 * As the accessor 'class' represents an exclusive lock on the item,
 * we use it for an interpreter mutex as well.  The first time it is
 * read, if its value is false then a PerlInterpreter is created.
 *
 */


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

// threads::tbb::init
static int perl_tbb_worker = 0;
static mutex_t perl_tbb_worker_mutex;

class perl_interpreter_pool : public tbb::concurrent_hash_map<raw_thread_id, int, raw_thread_hash_compare> {
public:
	void grab( perl_interpreter_pool::accessor& result, perl_tbb_init*init);
};

// the global pointer to the interpreter locks
extern perl_interpreter_pool tbb_interpreter_pool;

struct ptr_compare {
	static size_t hash( void* const& x ) {
		size_t h = 0;
		if (sizeof(void*) != sizeof(size_t) ) {
			int i = 0;
			for (const char* s = (char*)x; i<sizeof(void*); ++s) {
				h = (h*17) ^ *s;
				i++;
			}
		}
		else {
			h = *( (size_t*)x );
		}
		return h;
	}
	
	static bool equal( void* const& a, void* const& b) {
		return (a == b);
	}
};

typedef tbb::concurrent_hash_map<void*, int, ptr_compare> ptr_to_worker;
extern ptr_to_worker tbb_interpreter_numbers;

// freelist; next time interpreter wakes, it will free the items in this
// list.
class perl_interpreter_freelist : public tbb::concurrent_vector<tbb::strict_ppl::concurrent_queue<perl_concurrent_slot> > {
public:
	void free( PerlInterpreter* owner, SV *sv );
	void free( const perl_concurrent_slot item );
	perl_concurrent_slot* next( pTHX );
};

// the global pointer to the interpreter locks
extern perl_interpreter_freelist tbb_interpreter_freelist;

//typedef perl_interpreter_pool::accessor tbb_interpreter_lock;

// the crazy clone function :)
SV* clone_other_sv(PerlInterpreter* my_perl, SV* sv, PerlInterpreter* other_perl);


#endif

