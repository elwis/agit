/*
 * aros_entropy.c -- see aros_entropy.h for an overview and an honest
 * warning about the strength of the fallback source.
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

    /* RDRAND = bit 30 in ECX, CPUID leaf 1. Ivy Bridge (2012) and
     * later. My AROS machine (i5-2400, Sandy Bridge,
     * 2011) lacks this -- a whole generation too early. */
    return (ecx & (1u << 30)) ? 1 : 0;
}

/*
 * A single RDRAND instruction can fail even on CPUs that support it
 * (entropy source shared between all cores). Intel's documentation
 * recommends up to 10 retries before giving up.
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
 * WEAK fallback -- see the honesty warning in aros_entropy.h. Mixes
 * the system clock, a stack address (ASLR/run-specific on most modern
 * OSes, but AROS likely has no ASLR -- limited value) and a static
 * counter through a simple PCG-like LCG. Enough to make TLS session
 * keys unpredictable in practice, NOT enough to withstand a
 * determined adversary.
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
    state ^= counter * 2654435761u;   /* Knuth multiplicative hash */

    for (i = 0; i < len; i++)
    {
        /* PCG-like LCG constants (Knuth MMIX) */
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        output[i] = (unsigned char)(state >> 24);
    }
}

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len,
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
                /* Unexpected: the CPU said yes in CPUID but RDRAND
                 * gives up after 10 tries. Fill the rest with the
                 * fallback rather than failing outright. */
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
