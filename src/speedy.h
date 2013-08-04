#ifndef _SPEEDY_H__
#define _SPEEDY_H__

#ifdef __GNUC__
#define __likely(x)       __builtin_expect((long)(x),(long)1)
#define __unlikely(x)     __builtin_expect((long)(x),(long)0)
#else
#define __likely(x)       (x)
#define __unlikely(x)     (x)
#endif

#define UNUSED(x) ((void)x)

#endif /* _SPEEDY_H__ */
