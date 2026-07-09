/*
 * aros_dns.h -- getaddrinfo() shim for AROS, backed by gethostbyname().
 *
 * AROS's <netdb.h> already declares a POSIX-shaped struct addrinfo
 * (ai_family/ai_socktype/ai_protocol/ai_addrlen/ai_addr/ai_next), but
 * declares no getaddrinfo()/freeaddrinfo()/gai_strerror() functions at
 * all -- those only exist on AROS via the classic library-base calling
 * convention (gethostbyname()/getservbyname(), declared in
 * <clib/bsdsocket_protos.h>, reached through <proto/socket.h>, same
 * pattern that inet_pton()/select() needed). See src/aros_dns.c for
 * the implementation, which resolves via gethostbyname() (IPv4 only --
 * there is no AROS bsdsocket.library entry point for IPv6 resolution)
 * and builds AROS's real struct addrinfo directly.
 *
 * IMPORTANT: libgit2's own built-in NO_ADDRINFO fallback (src/util/
 * posix.c, posix.h) was deliberately NOT used here. It defines its
 * OWN struct addrinfo (with ai_hostent/ai_servent/ai_addr_in members)
 * under the same name -- which conflicts with the struct addrinfo
 * AROS's <netdb.h> already declares unconditionally. Defining
 * NO_ADDRINFO for this build would cause a hard redefinition error.
 * Providing our own getaddrinfo()-shaped functions instead avoids the
 * clash entirely and lets libgit2's generic (non-NO_ADDRINFO) code
 * path -- which already expects a real POSIX-shaped struct addrinfo --
 * run unmodified.
 *
 * Included into deps/libgit2/src/libgit2/streams/socket.c via
 * patches/libgit2-aros-getaddrinfo.patch. Reachable from that
 * compilation via the existing `-Isrc/aros-shims` flag (already passed
 * for sys/mman.h -- see docs/BUILDING.md).
 */

#ifndef AROS_DNS_H
#define AROS_DNS_H

#include <netdb.h>

int aros_getaddrinfo(const char *host, const char *port,
                      const struct addrinfo *hints, struct addrinfo **res);
void aros_freeaddrinfo(struct addrinfo *res);
const char *aros_gai_strerror(int err);

/*
 * libgit2's posix.h maps its internal p_getaddrinfo/p_freeaddrinfo/
 * p_gai_strerror straight onto these plain POSIX names when
 * NO_ADDRINFO is undefined (our case). Neither name is declared
 * anywhere in AROS's own headers, so redefining them as macros here
 * carries no collision risk.
 */
#define getaddrinfo(a, b, c, d) aros_getaddrinfo(a, b, c, d)
#define freeaddrinfo(a)         aros_freeaddrinfo(a)
#define gai_strerror(c)         aros_gai_strerror(c)

#endif /* AROS_DNS_H */
