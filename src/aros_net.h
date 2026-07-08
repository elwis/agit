/*
 * aros_net.h -- AROS bsdsocket.library-wrapper for mbedTLS.
 *
 * mbedTLS karnbibliotek (ssl_tls.c m.fl.) ar transportagnostiskt:
 * det pratar aldrig direkt med sockets, utan anropar de tre
 * funktionspekare man registrerar via mbedtls_ssl_set_bio(). Den har
 * filen implementerar de tre pekarna ovanpa bsdsocket.library,
 * eftersom mbedTLS egen net_sockets.c ar avstangd (se
 * cmake/mbedtls-user-config.h -- Unix/Windows-only-antaganden och
 * AROS avvikande setsockopt/getaddrinfo-signaturer).
 *
 * Anvandning (se hello-tls.c):
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
 * Oppnar bsdsocket.library. Maste anropas EN gang innan nagon
 * aros_net_connect(). Returnerar 0 vid lyckat, -1 annars.
 */
int aros_net_init(void);

/*
 * Stanger bsdsocket.library. Anropas nar programmet ar helt klart
 * med allt natverk.
 */
void aros_net_shutdown(void);

/*
 * Oppnar en TCP-anslutning till ip:port. OBS: ip maste vara en
 * dotted-quad-strang ("140.82.121.3"), INTE ett vardnamn --
 * getaddrinfo() har visat sig krava samma Miami-bas-anropskonvention
 * som inet_pton gjorde (se cmake/mbedtls-user-config.h), och
 * namnuppslagning ar darfor ett separat, annu olost delproblem.
 *
 * Returnerar 0 vid lyckad anslutning, -1 vid fel.
 */
int aros_net_connect(aros_net_context *ctx, const char *ip, int port);

/*
 * mbedtls_ssl_send_t / mbedtls_ssl_recv_t -kompatibla callbacks.
 * Registreras direkt via mbedtls_ssl_set_bio(&ssl, ctx, aros_net_send,
 * aros_net_recv, NULL).
 *
 * Returnerar antal bytes skickade/lasta, eller ett negativt
 * MBEDTLS_ERR_NET_*-felkod vid fel (se aros_net.c).
 */
int aros_net_send(void *ctx, const unsigned char *buf, size_t len);
int aros_net_recv(void *ctx, unsigned char *buf, size_t len);

/*
 * Stanger en enskild anslutning (men inte biblioteket sjalvt --
 * anvand aros_net_shutdown() for det, en gang, i slutet av
 * programmet).
 */
void aros_net_close(aros_net_context *ctx);

#endif /* AROS_NET_H */
