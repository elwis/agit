/*
 * mbedtls-user-config.h -- AROS-specific deviations from mbedTLS's
 * default configuration.
 *
 * Included AUTOMATICALLY after mbedtls_config.h via
 * -DMBEDTLS_USER_CONFIG_FILE (set in cmake/aros-x86_64.cmake).
 * Do NOT edit deps/mbedtls/include/mbedtls/mbedtls_config.h directly
 * -- keep all AROS deviations collected here, so they show up in a
 * single commit and survive future mbedTLS upgrades.
 */

#ifndef AROS_MBEDTLS_USER_CONFIG_H
#define AROS_MBEDTLS_USER_CONFIG_H

/*
 * MBEDTLS_TIMING_C (library/timing.c) is enabled by default but
 * requires Unix or Windows -- AROS isn't recognized as either.
 * The module drives DTLS timers and the benchmark/self-test tools;
 * agit speaks plain TCP-based HTTPS, so we can do entirely without it.
 */
#undef MBEDTLS_TIMING_C

/*
 * AROS's arpa/inet.h skips its ENTIRE BSD-style declaration of
 * inet_pton() when __AROS__ is defined (see
 * #if !defined(__AROS__) around the block in the header). The
 * function only exists via the classic Amiga library-base calling
 * convention (clib/miami_protos.h + defines/miami.h, requires an
 * opened MiamiBase) -- not as a plain C function mbedTLS can call
 * directly.
 *
 * mbedTLS has its own, officially sanctioned software implementation
 * of IP address parsing for exactly this situation (despite the
 * misleading "TEST_" name -- see the comment in library/x509_crt.c
 * above x509_inet_pton_ipv6). We force it instead of building out the
 * entire Miami-base opening chain for a single function.
 */
#define MBEDTLS_TEST_SW_INET_PTON

/*
 * MBEDTLS_ENTROPY_HARDWARE_ALT -- makes mbedTLS automatically call
 * mbedtls_hardware_poll() (implemented in src/aros_entropy.c) as a
 * built-in source in EVERY entropy context it creates internally,
 * including PSA crypto's hidden global context used by
 * psa_crypto_init(). This is the officially documented way to port
 * mbedTLS entropy to a platform without one -- see comment in
 * mbedtls_config.h. Without this, only entropy contexts an
 * application explicitly registers a source on (via
 * mbedtls_entropy_add_source()) get our source; PSA's internal one
 * does not, which caused PSA_ERROR_INSUFFICIENT_ENTROPY (-148) when
 * psa_crypto_init() was called with only MBEDTLS_NO_PLATFORM_ENTROPY
 * set and no replacement wired in globally.
 */
#define MBEDTLS_ENTROPY_HARDWARE_ALT

/*
 * MBEDTLS_NET_C (library/net_sockets.c) is just a CONVENIENCE WRAPPER
 * around BSD sockets -- not something the mbedTLS core library
 * (ssl_tls.c et al.) actually requires. The core is transport-agnostic
 * via mbedtls_ssl_set_bio(), where you register your own send/recv
 * callbacks.
 *
 * net_sockets.c also explicitly assumes Unix/Windows (#error on
 * anything else), and AROS's bsdsocket.library has pre-POSIX deviating
 * function signatures (e.g. setsockopt takes void* where mbedTLS
 * expects const char*, and getaddrinfo likely requires the same
 * Miami-base calling convention that inet_pton did). Patching all of
 * that would be its own sub-project for a module we don't need.
 *
 * Instead we write our own thin AROS-specific socket glue
 * (src/aros_net.c, upcoming) that talks to bsdsocket.library directly
 * and is wired in via mbedtls_ssl_set_bio() -- see hello-tls.c.
 */
#undef MBEDTLS_NET_C

#endif /* AROS_MBEDTLS_USER_CONFIG_H */
