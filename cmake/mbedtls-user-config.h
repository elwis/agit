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

#endif /* AROS_MBEDTLS_USER_CONFIG_H */
