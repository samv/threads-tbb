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

MODULE = threads::tbb::concurrent::item    PACKAGE = threads::tbb::concurrent::item

SV *
new( classname )
	char* classname;
  PREINIT:
	perl_concurrent_item* self;
  CODE:
	self = new perl_concurrent_item( my_perl, &PL_sv_undef );
        RETVAL = sv_newmortal();
	SvREFCNT_inc(RETVAL);
        sv_setref_pv( RETVAL, classname, (void*)self );
  OUTPUT:
	RETVAL

SV*
TIESCALAR(classname)
	char* classname;
  PREINIT:
        perl_concurrent_item* rv;
  CODE:
	rv = new perl_concurrent_item( my_perl, &PL_sv_undef );
        ST(0) = sv_newmortal();
        sv_setref_pv( ST(0), classname, (void*)rv );

SV*
FETCH(self)
  perl_concurrent_item* self;
  CODE:
    RETVAL = self->dup(my_perl);
  OUTPUT:
    RETVAL

void
STORE(self, value)
  perl_concurrent_item* self;
  SV* value;
  CODE:
  /* FIXME this leaks :( */
    (*self) = perl_concurrent_item( my_perl, value );

MODULE = threads::tbb::concurrent::array    PACKAGE = threads::tbb::concurrent::array

perl_concurrent_vector *
perl_concurrent_vector::new()

SV *
perl_concurrent_vector::FETCH(i)
	int i;
  PREINIT:
	SV* mysv;
	perl_concurrent_item* slot;
  CODE:
	if (THIS->size() < i+1) {
		IF_DEBUG_VECTOR("FETCH(%d): not extended to [%d]", i, i+1);
		XSRETURN_EMPTY;
	}
	slot = &(*THIS)[i];
	mysv = slot->thingy;
	if (mysv) {
		RETVAL = slot->dup( my_perl );
		IF_DEBUG_VECTOR("FETCH(%d): returning %x: copied to %x (refcnt = %d)", i, mysv, RETVAL, SvREFCNT(RETVAL));
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
	IF_DEBUG_VECTOR("STORE (%d, %x) (refcnt = %d)", i, v, SvREFCNT(v));
	IF_DEBUG_VECTOR("%x->grow_to_at_least(%d)", THIS, i+1);
	THIS->grow_to_at_least( i+1 );
	perl_concurrent_item* slot = &((*THIS)[i]);
	SV* o = slot->thingy;
	if (o) {
		IF_DEBUG_VECTOR("old = %x", o);
		if (my_perl == slot->owner) {
			IF_DEBUG_VECTOR("SvREFCNT_dec(%x) (refcnt = %d)", o, SvREFCNT(o));
			SvREFCNT_dec(o);
		}
		else {
			// for now, leak.  if this works,
			// then interpreters will need a freelist for
			// them to decrement when they next run/finish
			// a task.
		}
	}
        nsv = newSV(0);
	SvSetSV_nosteal(nsv, v);
	//SvREFCNT_inc(nsv);
	IF_DEBUG_VECTOR("new = %x (refcnt = %d)", nsv, SvREFCNT(nsv));
	slot->owner = my_perl;
	slot->thingy = nsv;
	

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
		x = newSV(0);
		SvSetSV_nosteal(x, ST(1));
		THIS->push_back( perl_concurrent_item(my_perl, x) );
		IF_DEBUG_VECTOR("PUSH (%x)", x);
	}
        else {
		idx = (THIS->grow_by( items-1 ));
		for (i = 1; i < items; i++) {
			x = newSV(0);
			SvSetSV_nosteal(x, ST(i));
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
	
MODULE = threads::tbb::for_int_array_func	PACKAGE = threads::tbb::for_int_array_func

perl_for_int_array_func*
perl_for_int_array_func::new( context, array, funcname )
	perl_tbb_init* context;
	perl_concurrent_vector* array;
	std::string funcname;

perl_concurrent_vector*
perl_for_int_array_func::get_array()

void
parallel_for(self, range)
        perl_for_int_array_func* self;
        perl_tbb_blocked_int* range;
  CODE:
        perl_tbb_blocked_int range_copy = perl_tbb_blocked_int(*range);
        perl_for_int_array_func body_copy = perl_for_int_array_func(*self);
        parallel_for( range_copy, body_copy );

MODULE = threads::tbb::for_int_method	PACKAGE = threads::tbb::for_int_method

perl_for_int_method*
new( CLASS, context, inv_sv, methodname )
	char* CLASS;
	perl_tbb_init* context;
	SV* inv_sv;
	std::string methodname;
  CODE:
	RETVAL = new perl_for_int_method( my_perl, context, inv_sv, methodname );
  OUTPUT:
	RETVAL

void
parallel_for(self, range)
	perl_for_int_method* self;
	perl_tbb_blocked_int* range;
  CODE:
	perl_tbb_blocked_int range_copy = perl_tbb_blocked_int(*range);
	perl_for_int_method body_copy = perl_for_int_method(*self);
	parallel_for( range_copy, body_copy );

MODULE = threads::tbb		PACKAGE = threads::tbb

