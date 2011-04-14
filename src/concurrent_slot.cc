
extern "C" {
#define PERL_NO_GET_CONTEXT /* we want efficiency! */
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
}

#include "tbb.h"

SV* perl_concurrent_slot::dup( pTHX ) const {
	SV* rsv;
	if (this->owner == my_perl) {
		rsv = newSV(0);
		SvSetSV_nosteal(rsv, this->thingy);
		IF_DEBUG_CLONE("dup'd %x to %x (refcnt = %d)", this->thingy, rsv, SvREFCNT(rsv));
	}
	else {
		IF_DEBUG_CLONE("CLONING %x (refcnt = %d)", this->thingy, SvREFCNT(this->thingy));
		rsv = clone_other_sv( my_perl, this->thingy, this->owner );
		SvREFCNT_inc(rsv);
	}
	return rsv;
}

SV* perl_concurrent_slot::clone( pTHX ) {
	IF_DEBUG_CLONE("CLONING %x (refcnt = %d)", this->thingy, SvREFCNT(this->thingy));
	SV* rsv = clone_other_sv( my_perl, this->thingy, this->owner );
	SvREFCNT_inc(rsv);
	return rsv;
}
