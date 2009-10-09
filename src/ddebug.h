#ifndef DDEBUG_H
#define DDEBUG_H

#if (DDEBUG) && (NGX_HAVE_VARIADIC_MACROS)

#   define DD(...) fprintf(stderr, "*** "); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, " at %s line %d.\n", __FILE__, __LINE__)

#else

#   if (NGX_HAVE_VARIADIC_MACROS)

#       define DD(...)

#   else

static void DD(const char* fmt, ...) {
}

#   endif

#endif

#endif /* DDEBUG_H */

