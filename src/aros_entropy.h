/*
 * aros_entropy.h -- entropy source for mbedTLS on AROS.
 *
 * AROS has no /dev/urandom (hence MBEDTLS_NO_PLATFORM_ENTROPY in
 * cmake/mbedtls-user-config.h). This file gives mbedTLS its own
 * entropy source via mbedtls_entropy_add_source().
 *
 * Strategy: check CPUID at runtime (NOT compile time -- the same
 * binary should work on any AROS x86_64 machine, not just the one it
 * was built on). If RDRAND is present (Ivy Bridge, 2012, and later)
 * it's used as real hardware entropy. If it's missing (like mine
 * current ASUS P8Z68-V LX with an i5-2400, Sandy Bridge 2011 -- a
 * whole generation too early for RDRAND) we fall back to a weak
 * software source.
 *
 * HONEST WARNING: the fallback source (system clock + stack address +
 * counter) is NOT cryptographically strong. It's enough to make TLS
 * session keys unpredictable against passive network eavesdropping in
 * a hobby project -- it does not protect against a determined
 * adversary trying to reconstruct the entropy source. See README TODO.
 */

#ifndef AROS_ENTROPY_H
#define AROS_ENTROPY_H

#include <stddef.h>

/*
 * Returns 1 if the CPU (at runtime, via CPUID) supports RDRAND,
 * otherwise 0. Mostly used for logging/diagnostics -- the actual
 * aros_entropy_source() already checks this internally.
 */
int aros_entropy_has_rdrand(void);

/*
 * mbedtls_entropy_f_source_ptr-compatible callback -- but note the
 * name. When MBEDTLS_ENTROPY_HARDWARE_ALT is set (see
 * cmake/mbedtls-user-config.h), mbedTLS requires this EXACT function
 * name and wires it automatically into every mbedtls_entropy_context
 * the library ever creates, including PSA crypto's internal hidden
 * context. This is different from mbedtls_entropy_add_source(),
 * which only registers a source on a context YOU explicitly created
 * and doesn't reach PSA's internal one -- that mismatch is what
 * caused the earlier PSA_ERROR_INSUFFICIENT_ENTROPY (-148).
 */
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len,
                          size_t *olen);

#endif /* AROS_ENTROPY_H */
