
#ifndef _LOGO_DECODE_H_
#define _LOGO_DECODE_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


int logo_decode(void *buffer, size_t buflen, const void *input, void **endptr);


#ifdef __cplusplus
}
#endif

#endif /* _LOGO_DECODE_H_ */
