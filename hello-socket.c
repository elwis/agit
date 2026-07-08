/*
 * hello-socket.c -- Steg 2a: bevisa rå TCP-konnektivitet på AROS.
 *
 * Medvetet minimalt: ingen DNS (getaddrinfo har vi redan sett strular
 * pa AROS), ingen TLS. Bara OpenLibrary + socket() + connect() mot en
 * hardkodad IP. Om det har fungerar vet vi att bsdsocket.library-lagret
 * ar helt, och nasta steg (aros_net.c wrapper for mbedTLS) kan byggas
 * med tillforsikt.
 *
 * Bygg (cross, pa Pop!_OS):
 *   x86_64-aros-gcc --sysroot=$SYSROOT hello-socket.c -o hello-socket
 *
 * IP-adressen nedan ar en av GitHubs kanda webbserver-IP:n (140.82.x.x
 * -spannet). Fungerar inte den specifika adressen -- GitHub roterar
 * ibland -- byt till valfri annan server du vet svarar pa port 80,
 * t.ex. din routers IP for ett rent lokalt natverkstest forst.
 */

#include <proto/exec.h>
#include <proto/socket.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <string.h>

struct Library *SocketBase = NULL;

#define TEST_IP   "140.82.121.3"   /* github.com, port 80 */
#define TEST_PORT 80

int main(void)
{
    int sock;
    struct sockaddr_in addr;
    int ret;

    printf("1. Oppnar bsdsocket.library...\n");
    SocketBase = OpenLibrary("bsdsocket.library", 4);
    if (!SocketBase)
    {
        printf("   FEL: OpenLibrary misslyckades. Ar TCP/IP-stacken "
               "(Poseidon/Miami) igang och konfigurerad?\n");
        return 1;
    }
    printf("   OK: SocketBase = %p\n", (void *)SocketBase);

    printf("2. Skapar socket (AF_INET, SOCK_STREAM)...\n");
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        printf("   FEL: socket() returnerade %d\n", sock);
        CloseLibrary(SocketBase);
        return 1;
    }
    printf("   OK: sock = %d\n", sock);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(TEST_PORT);
    addr.sin_addr.s_addr = inet_addr(TEST_IP);

    printf("3. Ansluter till %s:%d...\n", TEST_IP, TEST_PORT);
    ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
    {
        printf("   FEL: connect() returnerade %d\n", ret);
        CloseSocket(sock);
        CloseLibrary(SocketBase);
        return 1;
    }
    printf("   OK: anslutningen lyckades!\n");

    printf("4. Skickar en minimal HTTP-request...\n");
    {
        static const char req[] =
            "HEAD / HTTP/1.0\r\nHost: github.com\r\n\r\n";
        ret = send(sock, req, sizeof(req) - 1, 0);
        printf("   send() returnerade %d (skickade %d bytes)\n",
               ret, (int)sizeof(req) - 1);
    }

    printf("5. Laser svaret (forsta 200 bytes)...\n");
    {
        char buf[201];
        ret = recv(sock, buf, sizeof(buf) - 1, 0);
        if (ret > 0)
        {
            buf[ret] = '\0';
            printf("   OK: fick %d bytes:\n---\n%s\n---\n", ret, buf);
        }
        else
        {
            printf("   FEL eller inget svar: recv() = %d\n", ret);
        }
    }

    printf("6. Stanger.\n");
    CloseSocket(sock);
    CloseLibrary(SocketBase);

    printf("\nKLART. Om du sag ett HTTP-svar i steg 5 fungerar hela "
           "TCP-kedjan (OpenLibrary, socket, connect, send, recv) pa "
           "den har AROS-installationen.\n");

    return 0;
}
