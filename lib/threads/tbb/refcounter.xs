#ifdef __cplusplus
extern "C" {
#define PERL_NO_GET_CONTEXT /* we want efficiency! */
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
}
#endif

/* include your class headers here */
#include "tbb.h"
#include "interpreter_pool.h"

static ptr_to_int tbb_refcounter_count;

MODULE = threads::tbb::refcounter	PACKAGE = threads::tbb::refcounter

PROTOTYPES: DISABLE

void
pvmg_rc_inc( thingy )
	SV* thingy;
CODE:
	if (SvROK(thingy) && SvTYPE(SvRV(thingy)) == SVt_PVMG) {
		void* xs_ptr = (void*)SvIV((SV*)SvRV(thingy));
		ptr_to_int::accessor objlock;
		bool found = tbb_refcounter_count.find( objlock, xs_ptr );
		if (!found) {
			tbb_refcounter_count.insert( objlock, xs_ptr );
			(*objlock).second = 0;
		}
		(*objlock).second++;
	}

void
pvmg_rc_dec( thingy )
	SV* thingy;
PREINIT:
	bool call_destroy = true;
CODE:
	if (SvROK(thingy) && SvTYPE(SvRV(thingy)) == SVt_PVMG) {
		void* xs_ptr = (void*)SvIV((SV*)SvRV(thingy));
		ptr_to_int::accessor objlock;
		bool found = tbb_refcounter_count.find( objlock, xs_ptr );
		int rc = 0;
		if (found) {
			rc = --( (*objlock).second );
			if (!rc) {
				tbb_refcounter_count.erase( objlock );
			}
		}
		if (rc != 0) {
			call_destroy = false;
		}
	}
	if (call_destroy) {
		PUSHMARK(SP);
		XPUSHs( thingy );
		PUTBACK;

		call_method("_DESTROY_tbbrc", G_DISCARD);
	}
