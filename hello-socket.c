/*
 * hello-socket.c -- Step 2a: prove raw TCP connectivity on AROS.
 *
 * Deliberately minimal: no DNS (we've already seen getaddrinfo act up
 * on AROS), no TLS. Just OpenLibrary + socket() + connect() against a
 * hardcoded IP. If this works, we know the bsdsocket.library layer is
 * intact, and the next step (aros_net.c wrapper for mbedTLS) can be
 * built with confidence.
 *
 * Build (cross, on Pop!_OS):
 *   x86_64-aros-gcc --sysroot=$SYSROOT hello-socket.c -o hello-socket
 *
 * The IP address below is one of GitHub's known web server IPs (the
 * 140.82.x.x range). If that specific address doesn't work -- GitHub
 * rotates them sometimes -- swap in any other server you know answers
 * on port 80, e.g. your router's IP, for a purely local network test
 * first.
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

    printf("1. Opening bsdsocket.library...\n");
    SocketBase = OpenLibrary("bsdsocket.library", 4);
    if (!SocketBase)
    {
        printf("   ERROR: OpenLibrary failed. Is the TCP/IP stack "
               "(Poseidon/Miami) running and configured?\n");
        return 1;
    }
    printf("   OK: SocketBase = %p\n", (void *)SocketBase);

    printf("2. Creating socket (AF_INET, SOCK_STREAM)...\n");
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        printf("   ERROR: socket() returned %d\n", sock);
        CloseLibrary(SocketBase);
        return 1;
    }
    printf("   OK: sock = %d\n", sock);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(TEST_PORT);
    addr.sin_addr.s_addr = inet_addr(TEST_IP);

    printf("3. Connecting to %s:%d...\n", TEST_IP, TEST_PORT);
    ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
    {
        printf("   ERROR: connect() returned %d\n", ret);
        CloseSocket(sock);
        CloseLibrary(SocketBase);
        return 1;
    }
    printf("   OK: connection succeeded!\n");

    printf("4. Sending a minimal HTTP request...\n");
    {
        static const char req[] =
            "HEAD / HTTP/1.0\r\nHost: github.com\r\n\r\n";
        ret = send(sock, req, sizeof(req) - 1, 0);
        printf("   send() returned %d (sent %d bytes)\n",
               ret, (int)sizeof(req) - 1);
    }

    printf("5. Reading the response (first 200 bytes)...\n");
    {
        char buf[201];
        ret = recv(sock, buf, sizeof(buf) - 1, 0);
        if (ret > 0)
        {
            buf[ret] = '\0';
            printf("   OK: got %d bytes:\n---\n%s\n---\n", ret, buf);
        }
        else
        {
            printf("   ERROR or no response: recv() = %d\n", ret);
        }
    }

    printf("6. Closing.\n");
    CloseSocket(sock);
    CloseLibrary(SocketBase);

    printf("\nDONE. If you saw an HTTP response in step 5, the entire "
           "TCP chain (OpenLibrary, socket, connect, send, recv) works "
           "on this AROS installation.\n");

    return 0;
}
