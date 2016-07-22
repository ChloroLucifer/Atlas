#ifndef _MAKALU_UTIL_H
#define _MAKALU_UTIL_H

#include <string.h>

#ifdef MAK_ASSERTIONS
#  define MAK_ASSERT(expr) \
                if (!(expr)) { \
                  MAK_err_printf("Assertion failure: %s:%d\n", \
                                __FILE__, __LINE__); \
                  ABORT("assertion failure"); \
                }
#else
#  define MAK_ASSERT(expr)
#endif

#ifndef MAK_ATTR_FORMAT_PRINTF
# if defined(__GNUC__) && __GNUC__ >= 3
#   define MAK_ATTR_FORMAT_PRINTF(spec_argnum, first_checked) \
        __attribute__((__format__(__printf__, spec_argnum, first_checked)))
# else
#   define MAK_ATTR_FORMAT_PRINTF(spec_argnum, first_checked)
# endif
#endif

#define BUFSZ 1024

void MAK_printf(const char * format, ...)
                        MAK_ATTR_FORMAT_PRINTF(1, 2);
                        /* A version of printf that doesn't allocate,   */
                        /* 1K total output length.                      */
                        /* (We use sprintf.  Hopefully that doesn't     */
                        /* allocate for long arguments.)                */
void MAK_err_printf(const char * format, ...)
                        MAK_ATTR_FORMAT_PRINTF(1, 2);


#if defined(__GNUC__) && __GNUC__ >= 3
# define EXPECT(expr, outcome) __builtin_expect(expr,outcome)
  /* Equivalent to (expr), but predict that usually (expr)==outcome. */
#else
# define EXPECT(expr, outcome) (expr)
#endif /* __GNUC__ */

# define GC_STATIC_ASSERT(expr) (void)sizeof(char[(expr)? 1 : -1])



/* Print warning message, e.g. almost out of memory.    */
#define WARN(msg, arg) MAK_warn_proc("MAK Warning: " msg, \
                                               (MAK_word)(arg))

MAK_INNER void MAK_warn_proc(char* msg, MAK_word arg);

#define ABORT(msg) MAK_abort(msg)
MAK_INNER void MAK_abort(const char *msg);


#   define BZERO(x,n)  memset(x, 0, (size_t)(n))


#endif
