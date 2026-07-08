/*
 * hello-tls.c -- Step 2d: full TLS handshake against github.com:443
 * from AROS x86_64.
 *
 * Ties together:
 *   - aros_entropy.c  (entropy source for CTR-DRBG)
 *   - aros_net.c      (bsdsocket.library transport)
 *   - cacert.pem      (CA chain for certificate verification)
 *   - libmbedtls.a / libmbedx509.a / libmbedcrypto.a
 *
 * IMPORTANT about IP + SNI: we connect to a hardcoded IP (no
 * DNS/getaddrinfo yet, see aros_net.h), but GitHub runs behind Fastly,
 * which routes on the TLS SNI field. That means we MUST still set the
 * correct hostname via mbedtls_ssl_set_hostname() -- otherwise the
 * handshake can go to the wrong backend or fail outright, even though
 * the TCP connection to the IP itself succeeds.
 *
 * IMPORTANT about the system clock: certificate verification checks
 * the validity period against the AROS machine's clock. If it's wrong
 * (remember "SSL failures from clock stuck at 2005" from the hardware
 * build) the handshake will fail on date validation even if everything
 * else is correct. Check `Date` in the Shell first if something odd
 * happens.
 *
 * Build (cross, on Pop!_OS):
 *   x86_64-aros-gcc --sysroot=$SYSROOT \
 *       -Ideps/mbedtls/include -Isrc \
 *       hello-tls.c src/aros_net.c src/aros_entropy.c \
 *       build-mbedtls/library/libmbedtls.a \
 *       build-mbedtls/library/libmbedx509.a \
 *       build-mbedtls/library/libmbedcrypto.a \
 *       -o hello-tls
 *
 * Run on AROS: put hello-tls + cacert.pem in the same directory, run
 * from the Shell.
 */

#include "src/aros_net.h"
#include "src/aros_entropy.h"

#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/error.h>
#include <psa/crypto.h>

#include <stdio.h>
#include <string.h>

#define TARGET_IP       "140.82.121.3"   /* github.com, see hello-socket.c */
#define TARGET_PORT     443
#define TARGET_HOSTNAME "github.com"     /* for SNI + cert verification */
#define CACERT_PATH     "PROGDIR:cacert.pem"

static void print_mbedtls_error(const char *where, int ret)
{
    char buf[100];
    mbedtls_strerror(ret, buf, sizeof(buf));
    printf("   ERROR in %s: -0x%04x (%s)\n", where, (unsigned int)-ret, buf);
}

int main(void)
{
    int ret;

    mbedtls_entropy_context   entropy;
    mbedtls_ctr_drbg_context  ctr_drbg;
    mbedtls_x509_crt          cacert;
    mbedtls_ssl_context       ssl;
    mbedtls_ssl_config        conf;
    aros_net_context          net_ctx;

    static const char pers[] = "agit-hello-tls";

    printf("=== hello-tls: AROS x86_64 TLS handshake ===\n\n");

    /* --- 0. PSA crypto init --------------------------------------
     * mbedTLS >= 3.6.0 enables TLS 1.3 by default, and the TLS 1.3
     * code path uses the PSA crypto subsystem internally (ECDH
     * computations etc during the handshake). Skipping this call is
     * an EXTREMELY common gotcha after upgrading to 3.6.x -- without
     * it, mbedtls_ssl_handshake() fails with an unhelpful generic
     * internal error deep inside the handshake, with no indication
     * that psa_crypto_init() is the missing piece. Must be called
     * exactly once, before any SSL/X.509/PK function. */
    printf("0. Initializing the PSA crypto subsystem...\n");
    ret = psa_crypto_init();
    if (ret != PSA_SUCCESS)
    {
        printf("   ERROR: psa_crypto_init returned %d\n", (int)ret);
        return 1;
    }
    printf("   OK\n");

    /* --- 1. Entropy + CTR-DRBG ---------------------------------
     * With MBEDTLS_ENTROPY_HARDWARE_ALT set (see
     * cmake/mbedtls-user-config.h), mbedtls_entropy_init() already
     * wires in our mbedtls_hardware_poll() (src/aros_entropy.c)
     * automatically -- no manual mbedtls_entropy_add_source() call
     * needed here. */
    printf("1. Initializing entropy (RDRAND: %s)...\n",
           aros_entropy_has_rdrand() ? "YES" : "NO, using fallback");

    mbedtls_entropy_init(&entropy);

    mbedtls_ctr_drbg_init(&ctr_drbg);
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                (const unsigned char *)pers, sizeof(pers) - 1);
    if (ret != 0)
    {
        print_mbedtls_error("ctr_drbg_seed", ret);
        return 1;
    }
    printf("   OK\n");

    /* --- 2. CA chain ---------------------------------------------- */
    printf("2. Reading CA chain from %s...\n", CACERT_PATH);
    mbedtls_x509_crt_init(&cacert);
    ret = mbedtls_x509_crt_parse_file(&cacert, CACERT_PATH);
    if (ret != 0)
    {
        print_mbedtls_error("x509_crt_parse_file", ret);
        printf("   (is cacert.pem in the same directory as the binary?)\n");
        return 1;
    }
    printf("   OK\n");

    /* --- 3. SSL configuration --------------------------------------- */
    printf("3. Building SSL configuration...\n");
    mbedtls_ssl_config_init(&conf);
    ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0)
    {
        print_mbedtls_error("ssl_config_defaults", ret);
        return 1;
    }
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    printf("   OK\n");

    /* --- 4. SSL context + SNI --------------------------------------- */
    printf("4. Initializing SSL context, setting SNI hostname '%s'...\n",
           TARGET_HOSTNAME);
    mbedtls_ssl_init(&ssl);
    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret != 0)
    {
        print_mbedtls_error("ssl_setup", ret);
        return 1;
    }
    ret = mbedtls_ssl_set_hostname(&ssl, TARGET_HOSTNAME);
    if (ret != 0)
    {
        print_mbedtls_error("ssl_set_hostname", ret);
        return 1;
    }
    printf("   OK\n");

    /* --- 5. TCP connection via aros_net.c --------------------------- */
    printf("5. Opening TCP connection to %s:%d...\n",
           TARGET_IP, TARGET_PORT);
    if (aros_net_init() != 0)
    {
        printf("   ERROR: aros_net_init (bsdsocket.library)\n");
        return 1;
    }
    if (aros_net_connect(&net_ctx, TARGET_IP, TARGET_PORT) != 0)
    {
        printf("   ERROR: aros_net_connect\n");
        aros_net_shutdown();
        return 1;
    }
    mbedtls_ssl_set_bio(&ssl, &net_ctx, aros_net_send, aros_net_recv, NULL);
    printf("   OK\n");

    /* --- 6. TLS handshake --------------------------------------- */
    printf("6. Performing TLS handshake...\n");
    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
    {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            print_mbedtls_error("ssl_handshake", ret);
            aros_net_close(&net_ctx);
            aros_net_shutdown();
            return 1;
        }
        /* blocking sockets -- this should in practice never need to
         * loop, but we're being defensive */
    }
    printf("   OK: handshake succeeded! Cipher suite: %s\n",
           mbedtls_ssl_get_ciphersuite(&ssl));

    /* --- 7. Verify the certificate ----------------------------------- */
    printf("7. Verifying server certificate...\n");
    {
        uint32_t flags = mbedtls_ssl_get_verify_result(&ssl);
        if (flags != 0)
        {
            char vbuf[512];
            mbedtls_x509_crt_verify_info(vbuf, sizeof(vbuf), "   ", flags);
            printf("   WARNING -- certificate problem:\n%s", vbuf);
            printf("   (check the AROS machine's system clock -- Date in Shell)\n");
        }
        else
        {
            printf("   OK: the certificate is valid.\n");
        }
    }

    /* --- 8. Send HTTP request over TLS -------------------------------- */
    printf("8. Sending HTTPS request...\n");
    {
        char req[128];
        int  req_len;

        req_len = snprintf(req, sizeof(req),
                           "HEAD / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                           TARGET_HOSTNAME);

        ret = mbedtls_ssl_write(&ssl, (const unsigned char *)req, req_len);
        if (ret < 0)
        {
            print_mbedtls_error("ssl_write", ret);
        }
        else
        {
            printf("   OK: sent %d bytes\n", ret);
        }
    }

    /* --- 9. Read the encrypted response -------------------------------- */
    printf("9. Reading the response...\n");
    {
        unsigned char buf[512];
        ret = mbedtls_ssl_read(&ssl, buf, sizeof(buf) - 1);
        if (ret > 0)
        {
            buf[ret] = '\0';
            printf("   OK: got %d bytes of encrypted (and now decrypted) "
                   "response:\n---\n%s\n---\n", ret, buf);
        }
        else
        {
            print_mbedtls_error("ssl_read", ret);
        }
    }

    /* --- 10. Clean up --------------------------------------------------- */
    printf("10. Closing.\n");
    mbedtls_ssl_close_notify(&ssl);
    aros_net_close(&net_ctx);
    aros_net_shutdown();

    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_x509_crt_free(&cacert);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    mbedtls_psa_crypto_free();

    printf("\nDONE. If you saw a decrypted HTTP response in step 9, the "
           "entire TLS chain works on AROS x86_64.\n");

    return 0;
}
