/*
 * aros_time.c -- mbedtls_ms_time() for AROS. See cmake/mbedtls-user-
 * config.h (MBEDTLS_PLATFORM_MS_TIME_ALT) for background.
 *
 * AROS lacks clock_gettime()/CLOCK_MONOTONIC (the same reason
 * MBEDTLS_TIMING_C had to be disabled), so we can only rely on plain
 * time() -- second resolution, not millisecond. mbedTLS uses
 * mbedtls_ms_time() mainly for coarse time-based decisions (e.g.
 * session ticket lifetime), where second resolution is good enough.
 */

#include <mbedtls/platform_time.h>
#include <time.h>

mbedtls_ms_time_t mbedtls_ms_time(void)
{
    return (mbedtls_ms_time_t) time(NULL) * 1000;
}
