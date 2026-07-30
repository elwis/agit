/*
 * test_aros_getaddrinfo.c -- isolated test of aros_dns.c's
 * aros_getaddrinfo(), bypassing lg2/libgit2 entirely.
 *
 * We already know DNS resolution itself works on this system (native
 * "Ping github.com" resolves fine via bsdsocket.library). This test
 * isolates whether OUR getaddrinfo() implementation (src/aros_dns.c)
 * has a bug, independent of the underlying resolver.
 *
 * Build (cross, on Pop!_OS):
 *   x86_64-aros-gcc -Isrc -Isrc/aros-shims \
 *       test_aros_getaddrinfo.c src/aros_net.c src/aros_dns.c \
 *       -o test_aros_getaddrinfo
 *
 * (aros_net.c is needed because aros_dns.c likely relies on the
 * shared SocketBase that aros_net_init() opens -- same pattern as
 * every other AROS glue file in this project. If aros_dns.c opens
 * its own SocketBase independently, aros_net.c isn't strictly
 * needed, but linking it is harmless either way.)
 */

#include "aros_net.h"
#include "aros_dns.h"   /* declares aros_getaddrinfo/aros_freeaddrinfo,
                          * or #defines getaddrinfo/freeaddrinfo onto
                          * them -- check the actual header for the
                          * exact names in scope */

#include <stdio.h>
#include <string.h>
#include <netdb.h>      /* struct addrinfo -- AROS's real, unconditionally
                          * declared native struct */

static void print_addrinfo_chain(struct addrinfo *res)
{
    struct addrinfo *p;
    int count = 0;

    for (p = res; p != NULL; p = p->ai_next)
    {
        count++;
        printf("  [%d] family=%d socktype=%d protocol=%d addrlen=%d\n",
               count, p->ai_family, p->ai_socktype, p->ai_protocol,
               (int)p->ai_addrlen);

        if (p->ai_family == 2 /* AF_INET */ && p->ai_addr != NULL)
        {
            /* Manually unpack sockaddr_in to avoid pulling in more
             * headers than necessary for this isolated test -- offsets
             * per standard struct sockaddr_in layout (sin_family,
             * sin_port, sin_addr as 4 bytes). */
            unsigned char *raw = (unsigned char *)p->ai_addr;
            unsigned char *ip  = raw + 4;  /* skip sin_family (2) + sin_port (2) */
            printf("      -> IPv4: %d.%d.%d.%d\n",
                   ip[0], ip[1], ip[2], ip[3]);
        }
    }

    if (count == 0)
        printf("  (empty chain -- getaddrinfo returned success but no results?)\n");
}

int main(void)
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    int ret;

    printf("=== test_aros_getaddrinfo ===\n\n");

    printf("1. aros_net_init() (opens bsdsocket.library)...\n");
    if (aros_net_init() != 0)
    {
        printf("   FAILED: could not open bsdsocket.library\n");
        return 1;
    }
    printf("   OK\n");

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = 0;   /* AF_UNSPEC */
    hints.ai_socktype = 1;   /* SOCK_STREAM */

    printf("2. Calling getaddrinfo(\"github.com\", \"443\", ...)...\n");
    ret = getaddrinfo("github.com", "443", &hints, &res);

    if (ret != 0)
    {
        printf("   FAILED: getaddrinfo returned %d\n", ret);
        printf("   (check aros_dns.h/aros_dns.c for gai_strerror-\n");
        printf("    equivalent to decode this, or print it directly\n");
        printf("    if aros_gai_strerror is linked in)\n");
        aros_net_shutdown();
        return 1;
    }

    printf("   OK: got results:\n");
    print_addrinfo_chain(res);

    printf("3. freeaddrinfo()...\n");
    freeaddrinfo(res);
    printf("   OK\n");

    aros_net_shutdown();

    printf("\nDone. If step 2 showed a real 4.x.x.x-style IPv4 address\n");
    printf("matching what native Ping resolved, aros_dns.c is fine and\n");
    printf("the bug is elsewhere in lg2's connection code. If step 2\n");
    printf("failed or showed garbage, the bug is in aros_dns.c itself.\n");

    return 0;
}
