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

MODULE = threads::tbb::filter_method	PACKAGE = threads::tbb::filter_method

PROTOTYPES: DISABLE

perl_filter_method*
new( CLASS, context, inv_sv, methodname, is_serial )
