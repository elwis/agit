/*
 * mbedtls-user-config.h -- AROS-specifika avvikelser från mbedTLS
 * standardkonfiguration.
 *
 * Inkluderas AUTOMATISKT efter mbedtls_config.h via
 * -DMBEDTLS_USER_CONFIG_FILE (satt i cmake/aros-x86_64.cmake).
 * Redigera INTE deps/mbedtls/include/mbedtls/mbedtls_config.h direkt
 * -- håll alla AROS-avvikelser samlade här, så syns de i en enda
 * commit och överlever framtida mbedTLS-uppgraderingar.
 */

#ifndef AROS_MBEDTLS_USER_CONFIG_H
#define AROS_MBEDTLS_USER_CONFIG_H

/*
 * MBEDTLS_TIMING_C (library/timing.c) är påslagen som standard men
 * kräver Unix eller Windows -- AROS känns inte igen som något av det.
 * Modulen driver DTLS-timers och benchmark/självtest-verktygen; agit
 * pratar vanlig TCP-baserad HTTPS, så vi klarar oss utan den helt.
 */
#undef MBEDTLS_TIMING_C

/*
 * AROS arpa/inet.h skippar HELA sin BSD-stil-deklaration av
 * inet_pton() när __AROS__ är definierat (se
 * #if !defined(__AROS__) runt blocket i headern). Funktionen finns
 * bara via den klassiska Amiga library-bas-anropskonventionen
 * (clib/miami_protos.h + defines/miami.h, kräver en öppnad
 * MiamiBase) -- inte som en vanlig C-funktion mbedTLS kan anropa
 * rakt av.
 *
 * mbedTLS har en egen, officiellt sanktionerad mjukvaruimplementation
 * av IP-adressparsning för just den här situationen (trots det
 * missvisande "TEST_"-namnet -- se kommentaren i library/x509_crt.c
 * ovanför x509_inet_pton_ipv6). Vi tvingar fram den istället för att
 * bygga ut hela Miami-bas-öppningskedjan för en enda funktion.
 */
#define MBEDTLS_TEST_SW_INET_PTON

/*
 * MBEDTLS_NET_C (library/net_sockets.c) är bara en
 * BEKVÄMLIGHETSWRAPPER runt BSD-sockets -- inte något mbedTLS
 * kärnbibliotek (ssl_tls.c m.fl.) faktiskt kräver. Kärnan är
 * transportagnostisk via mbedtls_ssl_set_bio(), där man registrerar
 * egna send/recv-callbacks.
 *
 * net_sockets.c antar dessutom Unix/Windows explicit (#error på allt
 * annat) och AROS bsdsocket.library har pre-POSIX-avvikande
 * funktionssignaturer (t.ex. setsockopt tar void* där mbedTLS
 * förväntar sig const char*, och getaddrinfo kräver troligen samma
 * Miami-bas-anropskonvention som inet_pton gjorde). Att patcha allt
 * det vore ett eget delprojekt för en modul vi inte behöver.
 *
 * Vi skriver istället en egen tunn AROS-specifik socket-glue
 * (src/aros_net.c, kommande) som pratar bsdsocket.library direkt och
 * kopplas in via mbedtls_ssl_set_bio() -- se hello-tls.c.
 */
#undef MBEDTLS_NET_C

#endif /* AROS_MBEDTLS_USER_CONFIG_H */
