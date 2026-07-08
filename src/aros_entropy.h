/*
 * aros_entropy.h -- entropikalla for mbedTLS pa AROS.
 *
 * AROS har ingen /dev/urandom (darav MBEDTLS_NO_PLATFORM_ENTROPY i
 * cmake/mbedtls-user-config.h). Den har filen ger mbedTLS en egen
 * entropikalla via mbedtls_entropy_add_source().
 *
 * Strategi: kolla CPUID vid korning (INTE kompileringstid -- samma
 * binar ska fungera pa vilken AROS x86_64-maskin som helst, inte bara
 * den dator den byggdes pa). Finns RDRAND (Ivy Bridge, 2012, och
 * senare) anvands den som riktig hardvaruentropi. Saknas den (t.ex.
 * elwis nuvarande ASUS P8Z68-V LX med i5-2400, Sandy Bridge 2011 --
 * en hel generation for tidig for RDRAND) faller vi tillbaka pa en
 * svag mjukvarukalla.
 *
 * ARLIGT VARNING: fallback-kallan (systemklocka + stackadress +
 * raknare) ar INTE kryptografiskt stark. Den racker for att gora
 * TLS-sessionsnycklar oforutsagbara mot passiv naverksavlyssning i
 * ett hobbyprojekt -- den skyddar inte mot en malmedveten motstandare
 * som forsoker aterskapa entropikallan. Se README TODO.
 */

#ifndef AROS_ENTROPY_H
#define AROS_ENTROPY_H

#include <stddef.h>

/*
 * Returnerar 1 om CPU:n (vid korning, via CPUID) stodjer RDRAND,
 * annars 0. Anvands mest for loggning/diagnostik -- sjalva
 * aros_entropy_source() kollar redan detta internt.
 */
int aros_entropy_has_rdrand(void);

/*
 * mbedtls_entropy_f_source_ptr-kompatibel callback. Registreras via:
 *
 *   mbedtls_entropy_add_source(&entropy, aros_entropy_source, NULL,
 *       32, MBEDTLS_ENTROPY_SOURCE_STRONG);
 *
 * (threshold 32 bytes racker for var enda anropsstorlek mbedTLS
 * begar; STRONG-flaggan ar en vit lognod har -- vi begar den fran
 * mbedTLS eftersom det ar var enda kalla, men se arlighetsvarningen
 * ovan om vad "stark" faktiskt betyder pa maskiner utan RDRAND.)
 */
int aros_entropy_source(void *data, unsigned char *output, size_t len,
                        size_t *olen);

#endif /* AROS_ENTROPY_H */
