/*
 * aros_dns.c -- see src/aros-shims/aros_dns.h for the full rationale.
 *
 * Resolves via gethostbyname() (IPv4 only) and builds a real,
 * AROS-native struct addrinfo chain (as declared in <netdb.h>) so that
 * libgit2's generic socket_connect() code (streams/socket.c) can walk
 * it exactly like it would on any other POSIX platform.
 *
 * Requires bsdsocket.library already open (SocketBase set) -- see
 * aros_net_init()/aros_net_shutdown() in aros_net.c, called from
 * examples/lg2.c via patches/libgit2-aros-lg2-init.patch.
 *
 * Port is always a numeric string in libgit2's actual call sites
 * (git_socket_stream_new() receives it already resolved from the URL,
 * defaulted from the scheme) -- so plain atoi() is enough. No
 * getservbyname() support, deliberately: git never looks up a named
 * service here.
 */

#include "aros-shims/aros_dns.h"

#include <proto/socket.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

int aros_getaddrinfo(const char *host, const char *port,
                      const struct addrinfo *hints, struct addrinfo **res)
{
    struct hostent *he;
    struct addrinfo *head = NULL, *tail = NULL;
    unsigned short port_num;
    int i;

    if (!host || !res)
        return EAI_NONAME;

    if (hints && hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET)
        return EAI_FAMILY;   /* no IPv6 resolution available on AROS */

    port_num = (unsigned short)atoi(port ? port : "0");

    he = gethostbyname(host);
    if (!he || he->h_addrtype != AF_INET)
        return EAI_NONAME;

    for (i = 0; he->h_addr_list[i] != NULL; i++)
    {
        struct addrinfo   *cur = calloc(1, sizeof(*cur));
        struct sockaddr_in *sin = calloc(1, sizeof(*sin));

        if (!cur || !sin)
        {
            free(cur);
            free(sin);
            aros_freeaddrinfo(head);
            return EAI_MEMORY;
        }

        sin->sin_family = AF_INET;
        sin->sin_port   = htons(port_num);
        memcpy(&sin->sin_addr, he->h_addr_list[i], sizeof(sin->sin_addr));

        cur->ai_family   = AF_INET;
        cur->ai_socktype = hints ? hints->ai_socktype : SOCK_STREAM;
        cur->ai_protocol = 0;
        cur->ai_addrlen  = sizeof(*sin);
        cur->ai_addr     = (struct sockaddr *)sin;
        cur->ai_next     = NULL;

        if (tail)
            tail->ai_next = cur;
        else
            head = cur;
        tail = cur;
    }

    if (!head)
        return EAI_NONAME;

    *res = head;
    return 0;
}

void aros_freeaddrinfo(struct addrinfo *res)
{
    while (res)
    {
        struct addrinfo *next = res->ai_next;
        free(res->ai_addr);
        free(res);
        res = next;
    }
}

const char *aros_gai_strerror(int err)
{
    switch (err)
    {
    case EAI_NONAME:  return "name or service not known";
    case EAI_FAMILY:  return "address family not supported (AROS: IPv4 only)";
    case EAI_MEMORY:  return "memory allocation failure";
    default:          return "unknown DNS error";
    }
}
