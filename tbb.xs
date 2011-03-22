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

/* We need one MODULE... line to start the actual XS section of the file.
 * The XS++ preprocessor will output its own MODULE and PACKAGE lines */
MODULE = threads::tbb::init		PACKAGE = threads::tbb::init

PROTOTYPES: DISABLE

perl_tbb_init*
perl_tbb_init::new( thr )
	int thr;

void
perl_tbb_init::initialize( )

void
set_boot_lib( init, boot_lib )
	perl_tbb_init* init;
	AV* boot_lib;
  PREINIT:
	int i;
	STRLEN libname_len;
	const char* libname;
  CODE:
	for (i = 0; i <= av_len(boot_lib); i++) {
		SV** slot = av_fetch(boot_lib, i, 0);
		if (!slot || !SvPOK(*slot))
			continue;
		libname = SvPV( *slot, libname_len );
		IF_DEBUG_INIT("INC includes %s", libname);
		init->boot_lib.push_back( std::string( libname, libname_len ));
	}

void
set_boot_use( init, boot_use )
	perl_tbb_init* init;
	AV* boot_use;
  PREINIT:
	int i;
	STRLEN libname_len;
	const char* libname;
  CODE:
	for (i = 0; i <= av_len(boot_use); i++) {
		SV** slot = av_fetch(boot_use, i, 0);
		if (!slot || !SvPOK(*slot))
			continue;
		libname = SvPV( *slot, libname_len );
		IF_DEBUG_INIT("use list includes %s", libname);
		init->boot_use.push_back( std::string( libname, libname_len ));
	}

void
perl_tbb_init::DESTROY()

void
perl_tbb_init::terminate()


MODULE = threads::tbb::blocked_int		PACKAGE = threads::tbb::blocked_int

perl_tbb_blocked_int*
perl_tbb_blocked_int::new( low, high, grain )
	int low;
	int high;
	int grain;

int
perl_tbb_blocked_int::size( )

int
perl_tbb_blocked_int::grainsize( )

int
perl_tbb_blocked_int::begin( )

int
perl_tbb_blocked_int::end( )

bool
perl_tbb_blocked_int::empty( )

bool
perl_tbb_blocked_int::is_divisible( )

MODULE = threads::tbb::concurrent::array	PACKAGE = threads::tbb::concurrent::array

perl_concurrent_vector *
perl_concurrent_vector::new()

SV *
perl_concurrent_vector::FETCH(i)
	int i;
  PREINIT:
	SV* mysv;
	perl_concurrent_slot* slot;
  CODE:
	if (THIS->size() < i+1) {
		IF_DEBUG_VECTOR("FETCH(%d): not extended to [%d]", i, i+1);
		XSRETURN_EMPTY;
	}
	slot = &(*THIS)[i];
	mysv = slot->thingy;
	if (mysv) {
		if (slot->owner == my_perl) {
			IF_DEBUG_VECTOR("FETCH(%d): returning copy of %x", i, mysv);
			RETVAL = newSVsv(mysv);
		}
		else {
			IF_DEBUG_CLONE("ABOUT TO CLONE SV: %x", mysv);
			RETVAL = clone_other_sv( my_perl, mysv, slot->owner );
		}
	}
	else {
		IF_DEBUG_VECTOR("FETCH(%d): returning undef", i);
		XSRETURN_UNDEF;
	}
  OUTPUT:
	RETVAL
    

void
perl_concurrent_vector::STORE(i, v)
	int i;
	SV* v;
  PREINIT:
        SV* nsv;
  PPCODE:
	IF_DEBUG_VECTOR("STORE (%d, %x)", i, v);
	IF_DEBUG_VECTOR(">grow_to_at_least(%d)", i+1);
	THIS->grow_to_at_least(i+1);
	SV* o = (*THIS)[i].thingy;
	if (o) {
		IF_DEBUG_VECTOR("old = %x", o);
		if (my_perl == (*THIS)[i].owner) {
			SvREFCNT_dec(o);
		}
		else {
			// for now, leak.  if this works,
			// then interpreters will need a freelist for
			// them to decrement when they next run/finish
			// a task.
		}
	}
        nsv = newSVsv(v);
	SvREFCNT_inc(nsv);
        IF_DEBUG_VECTOR("new = %x", nsv);
	(*THIS)[i] = perl_concurrent_slot(my_perl, nsv);

void
perl_concurrent_vector::STORESIZE( i )
	int i;
  PPCODE:
	IF_DEBUG_VECTOR("grow_to_at_least(%d)", i);
	THIS->grow_to_at_least( i );

int
perl_concurrent_vector::size()

int
perl_concurrent_vector::FETCHSIZE()
  CODE:
	int size = THIS->size();
	IF_DEBUG_VECTOR("returning size = %d", size);
        RETVAL = size;
  OUTPUT:
        RETVAL

void
perl_concurrent_vector::PUSH(...)
  PREINIT:
	int i;
	perl_concurrent_vector::iterator idx;
        SV* x;
  PPCODE:
	if (items == 2) {
		x = newSVsv(ST(1));
		THIS->push_back( perl_concurrent_slot(my_perl, x) );
		IF_DEBUG_VECTOR("PUSH (%x)", x);
	}
        else {
		idx = (THIS->grow_by( items-1 ));
		for (i = 1; i < items; i++) {
			x = newSVsv(ST(i));
			IF_DEBUG_VECTOR("PUSH/%d (%x)", i, x);
			idx->thingy = x;
			idx->owner = my_perl;
			idx++;
		}
	}

static perl_concurrent_vector *
TIEARRAY(classname)
	char* classname;
  CODE:
	RETVAL = new perl_concurrent_vector();
        ST(0) = sv_newmortal();
        sv_setref_pv( ST(0), classname, (void*)RETVAL );
	
MODULE = threads::tbb::for_int_func	PACKAGE = threads::tbb::for_int_func

perl_for_int_func*
perl_for_int_func::new( context, funcname, array )
	perl_tbb_init* context;
	std::string funcname;
	perl_concurrent_vector* array;

perl_concurrent_vector*
perl_for_int_func::get_array()

MODULE = threads::tbb		PACKAGE = threads::tbb

void
parallel_for( init, range, body )
	perl_tbb_init* init;
	perl_tbb_blocked_int* range;
	perl_for_int_func* body;

  CODE:
	perl_tbb_blocked_int range_copy = perl_tbb_blocked_int(*range);
	perl_for_int_func body_copy = perl_for_int_func(*body);
	parallel_for( range_copy, body_copy );
