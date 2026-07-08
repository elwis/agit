/*
 * aros_net.c -- see aros_net.h for an overview.
 *
 * The socket calls themselves are identical to the ones already proven
 * to work in hello-socket.c (OpenLibrary/socket/connect/send/recv
 * against github.com:80) -- this file just wraps the same calls in
 * mbedTLS-compatible form. No new AROS risk surface is introduced
 * here; all unknown territory was already covered by the
 * hello-socket test.
 */

#include "aros_net.h"

#include <proto/exec.h>
#include <proto/socket.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

/*
 * Safe to include even though MBEDTLS_NET_C is disabled: the guard
 * "#if defined(MBEDTLS_NET_C) ... #error" sits in net_sockets.c
 * (the implementation), NOT in net_sockets.h (which only contains the
 * error code constants below, defined unconditionally). Verified
 * against the mbedTLS source -- see also GitHub issue
 * Mbed-TLS/mbedtls#1997, where the mbedTLS team confirms this is the
 * intended path for custom BIO implementations: the SSL layer
 * internally compares against these specific values (e.g.
 * MBEDTLS_ERR_SSL_WANT_READ), so arbitrary error codes outside
 * mbedTLS's own numeric range produce an unpredictable generic error
 * instead of a handleable specific status code.
 */
#include <mbedtls/net_sockets.h>

struct Library *SocketBase = NULL;

int aros_net_init(void)
{
    SocketBase = OpenLibrary("bsdsocket.library", 4);
    if (!SocketBase)
        return -1;
    return 0;
}

void aros_net_shutdown(void)
{
    if (SocketBase)
    {
        CloseLibrary(SocketBase);
        SocketBase = NULL;
    }
}

int aros_net_connect(aros_net_context *ctx, const char *ip, int port)
{
    struct sockaddr_in addr;
    int fd;

    if (!SocketBase)
        return -1;   /* aros_net_init() forgotten */

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((unsigned short)port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        CloseSocket(fd);
        return -1;
    }

    ctx->fd = fd;
    return 0;
}

int aros_net_send(void *ctx_ptr, const unsigned char *buf, size_t len)
{
    aros_net_context *ctx = (aros_net_context *)ctx_ptr;
    int ret;

    ret = send(ctx->fd, (const char *)buf, (int)len, 0);
    if (ret < 0)
        return MBEDTLS_ERR_NET_SEND_FAILED;

    return ret;
}

int aros_net_recv(void *ctx_ptr, unsigned char *buf, size_t len)
{
    aros_net_context *ctx = (aros_net_context *)ctx_ptr;
    int ret;

    ret = recv(ctx->fd, (char *)buf, (int)len, 0);
    if (ret < 0)
        return MBEDTLS_ERR_NET_RECV_FAILED;
    if (ret == 0)
        return MBEDTLS_ERR_NET_CONN_RESET;   /* peer closed the connection */

    return ret;
}

void aros_net_close(aros_net_context *ctx)
{
    if (ctx->fd >= 0)
    {
        CloseSocket(ctx->fd);
        ctx->fd = -1;
    }
}
