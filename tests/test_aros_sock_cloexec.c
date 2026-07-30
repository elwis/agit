#include "aros-shims/aros_dns.h"
#include "aros_net.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

int main(void)
{
    int s;

    if (aros_net_init() != 0) {
        printf("aros_net_init() failed\n");
        return 1;
    }

    s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    printf("socket(SOCK_STREAM | SOCK_CLOEXEC) = %d", s);
    if (s == -1)
        printf("  errno=%d (%s)", errno, strerror(errno));
    printf("\n");

    if (s != -1) {
        close(s);
        aros_net_shutdown();
        return 0;
    }

    s = socket(AF_INET, SOCK_STREAM, 0);
    printf("socket(SOCK_STREAM)              = %d", s);
    if (s == -1)
        printf("  errno=%d (%s)", errno, strerror(errno));
    printf("\n");

    if (s != -1)
        close(s);

    aros_net_shutdown();
    return 0;
}
