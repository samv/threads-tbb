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


MODULE = threads::tbb		PACKAGE = threads::tbb

