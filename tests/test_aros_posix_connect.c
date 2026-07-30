/*
 * test_aros_posix_connect.c -- isolate whether <sys/socket.h>'s POSIX
 * socket()/connect() work on AROS with SocketBase set by aros_net_init().
 *
 * This EXACTLY reproduces the include situation of
 * deps/libgit2/src/libgit2/streams/socket.c: it includes <sys/socket.h>
 * (and friends) but deliberately does NOT include <proto/socket.h>.
 *
 * That distinction matters: <proto/socket.h> (defines/bsdsocket.h)
 * #defines socket() -> __socket_WB(SocketBase, ...),
 * connect() -> __connect_WB(SocketBase, ...), etc. -- dispatching
 * directly through the global SocketBase.  Without those macros,
 * socket()/connect() resolve to AROS's posixc C library's POSIX
 * wrapper functions.  The question this test answers: do those POSIX
 * wrappers work with the SocketBase that aros_net_init() opens, or
 * do they need a different initialization path?
 *
 * Build (cross, on Pop!_OS):
 *   x86_64-aros-gcc -Isrc \
 *       tests/test_aros_posix_connect.c src/aros_net.c \
 *       -o tests/test_aros_posix_connect
 *
 * Run on AROS in a directory alongside cacert.pem (or any directory).
 * On success, prints "connect() returned 0".  On failure, prints
 * "connect() returned -1, errno = N (strerror)" -- compare the errno
 * value against what the real lg2 sees.
 *
 * Known-working IP for github.com (confirmed by test_aros_getaddrinfo
 * and the isolated aros_getaddrinfo() test).  If this specific address
 * stops working (Fastly rotates IPs), replace with the output of the
 * getaddrinfo test.
 */

#include <aros_net.h>

/* NOTE: <proto/socket.h> is deliberately NOT included.                  */
/* NOTE: <proto/exec.h> is also NOT included -- we rely on aros_net_init */
/*       instead of calling OpenLibrary directly, matching how lg2 does  */
/*       it (patches/libgit2-aros-lg2-init.patch).                       */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/*
 * IP resolved by aros_getaddrinfo() against github.com during the
 * last on-device test session (see test_aros_getaddrinfo output in
 * the task description).  Update if needed.
 */
#define TARGET_IP   "4.225.11.194"
#define TARGET_PORT 443

int main(void)
{
    int sock;
    struct sockaddr_in addr;
    int ret;

    printf("=== test_aros_posix_connect ===\n");
    printf("(POSIX <sys/socket.h> only, NO <proto/socket.h>)\n\n");

    /*
     * Step 1: open bsdsocket.library via aros_net_init(), exactly as
     * lg2 does (patches/libgit2-aros-lg2-init.patch calls this in
     * main() before any network operation).
     */
    printf("1. Calling aros_net_init() (opens bsdsocket.library)...\n");
    if (aros_net_init() != 0)
    {
        printf("   FAILED: aros_net_init returned non-zero\n");
        return 1;
    }
    printf("   OK\n");

    /*
     * Step 2: socket() via <sys/socket.h> (NOT the <proto/socket.h>
     * macro -- that header is not included).  On AROS's posixc,
     * this links to the C runtime library's POSIX socket wrapper,
     * which needs to find the global SocketBase.
     */
    printf("2. Calling socket(AF_INET, SOCK_STREAM, 0)...\n");
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        printf("   FAILED: socket() returned %d, errno = %d (%s)\n",
               sock, errno, strerror(errno));
        aros_net_shutdown();
        return 1;
    }
    printf("   OK: sock = %d\n", sock);

    /*
     * Step 3: connect() via <sys/socket.h> (also NOT the
     * <proto/socket.h> macro).  This is exactly what
     * streams/socket.c's socket_connect() does at line 150.
     */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(TARGET_PORT);
    addr.sin_addr.s_addr = inet_addr(TARGET_IP);

    printf("3. Calling connect(sock, \"%s\", %d)...\n", TARGET_IP, TARGET_PORT);
    ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
    {
        printf("   FAILED: connect() returned %d, errno = %d (%s)\n",
               ret, errno, strerror(errno));
        printf("\n");
        printf("   This FAILURE reproduces the lg2 bug: <sys/socket.h>'s\n");
        printf("   POSIX connect() fails even though SocketBase was set\n");
        printf("   by aros_net_init().  The fix (adding <proto/socket.h>\n");
        printf("   to streams/socket.c, bypassing the posixc wrapper)\n");
        printf("   is likely correct.\n");
        close(sock);
        aros_net_shutdown();
        return 1;
    }

    printf("   OK: connect() returned 0 (connection succeeded)\n");
    printf("\n");
    printf("   UNEXPECTED: <sys/socket.h>'s POSIX connect() worked.\n");
    printf("   This means the bug is NOT in the POSIX-vs-proto socket\n");
    printf("   dispatch mismatch -- look elsewhere.\n");

    close(sock);
    aros_net_shutdown();

    printf("\nDone.\n");
    return 0;
}
