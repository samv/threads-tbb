#ifdef __cplusplus
extern "C" {
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
}
#endif

#include "tbb.h"

#ifdef __cplusplus
extern "C" {
#endif
  int array_length(AV* array) {
    return AvMAX(array)+1;
  }
  AV* array_split(AV* array, int m, AV**new_self) {
    int i;
    int len = av_len(array);
    int n = len-m;
    int o;
    AV* rv;
    AV* item;
    SV** chunk = (SV**)malloc( (n>m?n:m)*sizeof(SV*) );
    o = 0;
    for (i=0; i < len; i++) {
      if (o == 0 && i == m-1) {
	rv = av_make(m, chunk);
	o = m;
      }
      chunk[i-o] = *av_fetch(array, i, 0);
    }
    *new_self = av_make(n, chunk);
    av_undef(array);
    return rv;
  }
#ifdef __cplusplus
}

#endif
