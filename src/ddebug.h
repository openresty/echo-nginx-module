#ifndef DDEBUG_H
#define DDEBUG_H

#if (DDEBUG)

#define DD(...) fprintf(stderr, "*** "); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, " at %s line %d.\n", __FILE__, __LINE__)

#else

#define DD (void)

#endif /* (DDEBUG) */

#endif /* DDEBUG_H */

