/*
 * aros_net.c -- se aros_net.h for oversikt.
 *
 * Sjalva socket-anropen ar identiska med de som redan bevisats
 * fungera i hello-socket.c (OpenLibrary/socket/connect/send/recv mot
 * github.com:80) -- den har filen paketerar bara samma anrop i
 * mbedTLS-kompatibel form. Ingen ny AROS-riskyta introduceras har;
 * all okand mark tacktes redan av hello-socket-testet.
 */

#include "aros_net.h"

#include <proto/exec.h>
#include <proto/socket.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * OBS: vi lutar oss INTE mot mbedtls/net_sockets.h har for
 * MBEDTLS_ERR_NET_*-konstanterna, trots att det hade gett snyggare
 * felmeddelanden via mbedtls_strerror(). Den modulen ar avstangd
 * (MBEDTLS_NET_C, se cmake/mbedtls-user-config.h), och headerns
 * felkodsdefines kan mycket val vara inlindade i samma #ifdef --
 * ooverblickat annu. mbedtls_ssl_send_t/recv_t kraver bara "negativt
 * varde = fel", sa vanliga -1 racker helt for vart syfte.
 */

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
        return -1;   /* aros_net_init() glomd */

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
        return -1;

    return ret;
}

int aros_net_recv(void *ctx_ptr, unsigned char *buf, size_t len)
{
    aros_net_context *ctx = (aros_net_context *)ctx_ptr;
    int ret;

    ret = recv(ctx->fd, (char *)buf, (int)len, 0);
    if (ret < 0)
        return -1;
    if (ret == 0)
        return -1;   /* motparten stangde -- ocksa ett fel for oss */

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
