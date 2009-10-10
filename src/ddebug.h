#ifndef DDEBUG_H
#define DDEBUG_H

#include <ngx_core.h>

#if (DDEBUG)

#   if (NGX_HAVE_VARIADIC_MACROS)

#       define DD(...) fprintf(stderr, "*** "); \
            fprintf(stderr, __VA_ARGS__); \
            fprintf(stderr, " at %s line %d.\n", __FILE__, __LINE__)

#   else

#include <stdarg.h>
#include <stdio.h>

#include <stdarg.h>

static void DD(const char* fmt, ...) {
}

#    endif

#else

#   if (NGX_HAVE_VARIADIC_MACROS)

#       define DD(...)

#   else

#include <stdarg.h>

static void DD(const char* fmt, ...) {
}

#   endif

#endif

#endif /* DDEBUG_H */

