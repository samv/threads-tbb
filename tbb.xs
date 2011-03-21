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
set_boot_inc( init, boot_inc_sv )
	perl_tbb_init* init;
	SV* boot_inc_sv;
  PREINIT:
	HV* boot_inc;
	HE* he;
	int libname_len;
	const char* libname;
  CODE:
	if (!SvROK(boot_inc_sv)) {
		croak("bad SV!");
	}
	boot_inc = (HV*)(SvRV(boot_inc_sv));
	hv_iterinit(boot_inc);
	while (he = hv_iternext(boot_inc)) {
		libname = hv_iterkey(he, &libname_len);
		IF_DEBUG_INIT("children will load %s", libname);
		init->boot_inc.insert( std::string( libname, libname_len ));
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
  CODE:
	if (THIS->size() < i+1) {
		IF_DEBUG_VECTOR("FETCH(%d): not extended to [%d]", i, i+1);
		XSRETURN_EMPTY;
	}
	mysv = (*THIS)[i];
	if (mysv) {
		IF_DEBUG_VECTOR("FETCH(%d): returning copy of %x", i, mysv);
		RETVAL = newSVsv(mysv);
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
	SV* o = (*THIS)[i];
	if (o) {
		IF_DEBUG_VECTOR("old = %x", o);
		SvREFCNT_dec(o);
	}
        nsv = newSVsv(v);
        IF_DEBUG_VECTOR("new = %x", nsv);
        (*THIS)[i] = nsv;

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
		THIS->push_back( x );
		IF_DEBUG_VECTOR("PUSH (%x)", x);
	}
        else {
		idx = (THIS->grow_by( items-1 ));
		for (i = 1; i < items; i++) {
			x = newSVsv(ST(i));
			IF_DEBUG_VECTOR("PUSH/%d (%x)", i, x);
			*idx = x;
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
	
MODULE = threads::tbb::map_int_body	PACKAGE = threads::tbb::map_int_body

perl_map_int_body*
perl_map_int_body::new( methname )
	std::string methname;

MODULE = threads::tbb		PACKAGE = threads::tbb

void
parallel_for( range, body )
	perl_tbb_blocked_int* range;
	perl_map_int_body* body;

  CODE:
	perl_tbb_blocked_int range_copy = perl_tbb_blocked_int(*range);
	perl_map_int_body body_copy = perl_map_int_body(*body);
	parallel_for( range_copy, body_copy );
