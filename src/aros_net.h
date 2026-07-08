/*
 * aros_net.h -- AROS bsdsocket.library wrapper for mbedTLS.
 *
 * The mbedTLS core library (ssl_tls.c et al.) is transport-agnostic:
 * it never talks directly to sockets, but instead calls the three
 * function pointers you register via mbedtls_ssl_set_bio(). This file
 * implements those three pointers on top of bsdsocket.library, since
 * mbedTLS's own net_sockets.c is disabled (see
 * cmake/mbedtls-user-config.h -- Unix/Windows-only assumptions and
 * AROS's deviating setsockopt/getaddrinfo signatures).
 *
 * Usage (see hello-tls.c):
 *   aros_net_context ctx;
 *   aros_net_init();
 *   aros_net_connect(&ctx, "140.82.121.3", 443);
 *   mbedtls_ssl_set_bio(&ssl, &ctx, aros_net_send, aros_net_recv, NULL);
 *   ...
 *   aros_net_close(&ctx);
 *   aros_net_shutdown();
 */

#ifndef AROS_NET_H
#define AROS_NET_H

#include <stddef.h>

typedef struct
{
    int fd;
} aros_net_context;

/*
 * Opens bsdsocket.library. Must be called ONCE before any
 * aros_net_connect(). Returns 0 on success, -1 otherwise.
 */
int aros_net_init(void);

/*
 * Closes bsdsocket.library. Called when the program is completely
 * done with all networking.
 */
void aros_net_shutdown(void);

/*
 * Opens a TCP connection to ip:port. NOTE: ip must be a dotted-quad
 * string ("140.82.121.3"), NOT a hostname -- getaddrinfo() has turned
 * out to require the same Miami-base calling convention that
 * inet_pton did (see cmake/mbedtls-user-config.h), so name resolution
 * is therefore a separate, still-unsolved sub-problem.
 *
 * Returns 0 on successful connection, -1 on error.
 */
int aros_net_connect(aros_net_context *ctx, const char *ip, int port);

/*
 * mbedtls_ssl_send_t / mbedtls_ssl_recv_t -compatible callbacks.
 * Registered directly via mbedtls_ssl_set_bio(&ssl, ctx, aros_net_send,
 * aros_net_recv, NULL).
 *
 * Returns the number of bytes sent/read, or a negative
 * MBEDTLS_ERR_NET_* error code on failure (see aros_net.c).
 */
int aros_net_send(void *ctx, const unsigned char *buf, size_t len);
int aros_net_recv(void *ctx, unsigned char *buf, size_t len);

/*
 * Closes a single connection (but not the library itself -- use
 * aros_net_shutdown() for that, once, at the end of the program).
 */
void aros_net_close(aros_net_context *ctx);

#endif /* AROS_NET_H */
