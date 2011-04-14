
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
//typedef struct raw_thread_hash_compare;

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

class perl_interpreter_pool : public tbb::concurrent_hash_map<raw_thread_id, int, raw_thread_hash_compare> {
public:
	void grab( perl_interpreter_pool::accessor& result, perl_tbb_init*init);
};

// the global pointer to the interpreter locks
extern perl_interpreter_pool tbb_interpreter_pool;

