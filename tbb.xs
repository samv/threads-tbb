#ifdef __cplusplus
extern "C" {
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
}
#endif

/* include your class headers here */
#include "tbb.h"

#if __GNUC__ >= 3   /* I guess. */
#define _warn(msg, e...) warn("# (" __FILE__ ":%d): " msg, __LINE__, ##e)
#else
#define _warn warn
#endif

#define IF_DEBUG(e)

/* We need one MODULE... line to start the actual XS section of the file.
 * The XS++ preprocessor will output its own MODULE and PACKAGE lines */
MODULE = threads::tbb::init		PACKAGE = threads::tbb::init

## The include line executes xspp with the supplied typemap and the
## xsp interface code for our class.
## It will include the output of the xsubplusplus run.
## INCLUDE_COMMAND: $^X -MExtUtils::XSpp::Cmd -e xspp -- --typemap=typemap.xsp task_scheduler_init.xsp

PROTOTYPES: DISABLE

perl_tbb_init*
perl_tbb_init::new()

void
perl_tbb_init::initialize( nthr )
      int nthr;

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
		IF_DEBUG(_warn("FETCH(%d): not extended to [%d]", i, i+1));
		XSRETURN_EMPTY;
	}
	mysv = (*THIS)[i];
	if (mysv) {
		IF_DEBUG(_warn("FETCH(%d): returning copy of %x", i, mysv));
		RETVAL = newSVsv(mysv);
	}
	else {
		IF_DEBUG(_warn("FETCH(%d): returning undef", i));
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
	IF_DEBUG(_warn("STORE (%d, %x)", i, v));
	IF_DEBUG(_warn(">grow_to_at_least(%d)", i+1));
	THIS->grow_to_at_least(i+1);
	SV* o = (*THIS)[i];
	if (o) {
		IF_DEBUG(_warn("old = %x", o));
		SvREFCNT_dec(o);
	}
        nsv = newSVsv(v);
        IF_DEBUG(_warn("new = %x", nsv));
        (*THIS)[i] = nsv;

void
perl_concurrent_vector::STORESIZE( i )
	int i;
  PPCODE:
	IF_DEBUG(_warn("grow_to_at_least(%d)", i));
	THIS->grow_to_at_least( i );

int
perl_concurrent_vector::size()

int
perl_concurrent_vector::FETCHSIZE()
  CODE:
	int size = THIS->size();
	IF_DEBUG(_warn("returning size = %d", size));
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
		IF_DEBUG(_warn("PUSH (%x)", x));
	}
        else {
		idx = (THIS->grow_by( items-1 ));
		for (i = 1; i < items; i++) {
			x = newSVsv(ST(i));
			IF_DEBUG(_warn("PUSH/%d (%x)", i, x));
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
PREINIT:
  perl_tbb_blocked_int range_copy = perl_tbb_blocked_int(*range);
  perl_map_int_body body_copy = perl_map_int_body(*body);
CODE:
  
  parallel_for( range_copy, body_copy );
