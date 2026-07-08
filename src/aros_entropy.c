/*
 * aros_entropy.c -- se aros_entropy.h for oversikt och arlighets-
 * varning om fallback-kallans styrka.
 */

#include "aros_entropy.h"

#include <cpuid.h>      /* GCC-bunt, oberoende av AROS sysroot */
#include <stdint.h>
#include <string.h>
#include <time.h>

int aros_entropy_has_rdrand(void)
{
    unsigned int eax, ebx, ecx, edx;

    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx))
        return 0;

    /* RDRAND = bit 30 i ECX, CPUID-leaf 1. Ivy Bridge (2012) och
     * senare. Elwis nuvarande AROS-maskin (i5-2400, Sandy Bridge,
     * 2011) saknar detta -- en hel generation for tidig. */
    return (ecx & (1u << 30)) ? 1 : 0;
}

/*
 * En enda RDRAND-instruktion kan misslyckas aven pa CPU:er som
 * stodjer den (delad entropikalla mellan alla karnor). Intels
 * dokumentation rekommenderar upp till 10 forsok innan man ger upp.
 */
static int rdrand64(uint64_t *out)
{
    unsigned char ok;
    int tries;

    for (tries = 0; tries < 10; tries++)
    {
        __asm__ volatile("rdrand %0; setc %1"
                         : "=r"(*out), "=qm"(ok));
        if (ok)
            return 1;
    }
    return 0;
}

/*
 * SVAG fallback -- se arlighetsvarningen i aros_entropy.h. Blandar
 * systemklocka, en stackadress (ASLR/korningsspecifik pa de flesta
 * moderna OS, men AROS har troligen ingen ASLR -- varde begransat)
 * och en statisk raknare genom en enkel PCG-liknande LCG. Racker for
 * att gora TLS-sessionsnycklar ovanliga i praktiken, INTE for att
 * sta emot en malmedveten motstandare.
 */
static void weak_fill(unsigned char *output, size_t len)
{
    static uint64_t counter = 0;
    uint64_t state;
    int stack_marker;
    size_t i;

    counter++;
    state  = (uint64_t)(time_t) time(NULL);
    state ^= (uint64_t)(size_t) &stack_marker;
    state ^= counter * 2654435761u;   /* Knuth multiplikativ hash */

    for (i = 0; i < len; i++)
    {
        /* PCG-liknande LCG-konstanter (Knuth MMIX) */
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        output[i] = (unsigned char)(state >> 24);
    }
}

int aros_entropy_source(void *data, unsigned char *output, size_t len,
                        size_t *olen)
{
    (void)data;

    if (aros_entropy_has_rdrand())
    {
        size_t i = 0;

        while (i + sizeof(uint64_t) <= len)
        {
            uint64_t v;
            if (!rdrand64(&v))
            {
                /* Ovantat: CPU:n sa ja i CPUID men RDRAND ger upp
                 * efter 10 forsok. Fyll resten med fallback hellre
                 * an att misslyckas helt. */
                weak_fill(output + i, len - i);
                *olen = len;
                return 0;
            }
            memcpy(output + i, &v, sizeof(v));
            i += sizeof(v);
        }

        if (i < len)
        {
            uint64_t v;
            if (rdrand64(&v))
                memcpy(output + i, &v, len - i);
            else
                weak_fill(output + i, len - i);
        }
    }
    else
    {
        weak_fill(output, len);
    }

    *olen = len;
    return 0;
}
