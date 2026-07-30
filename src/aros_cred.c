#include "aros_cred.h"
#include <git2/sys/errors.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_pat_from_file(void)
{
    static const char *paths[] = {
        "PROGDIR:agit.config",
        "agit.config",
        NULL
    };
    int i;

    for (i = 0; paths[i]; i++)
    {
        FILE *f;
        char line[512];
        char *pat = NULL;
        const char prefix[] = "GITHUB_PAT=";
        size_t plen = strlen(prefix);

        f = fopen(paths[i], "r");
        if (!f)
            continue;

        while (fgets(line, sizeof(line), f))
        {
            if (strncmp(line, prefix, plen) == 0)
            {
                size_t vlen = strlen(line + plen);

                if (vlen > 0 && line[plen + vlen - 1] == '\n')
                    vlen--;
                pat = malloc(vlen + 1);
                if (pat)
                {
                    memcpy(pat, line + plen, vlen);
                    pat[vlen] = '\0';
                }
                break;
            }
        }

        fclose(f);
        if (pat)
            return pat;
    }

    return NULL;
}

int aros_cred_acquire_cb(git_credential **out,
                         const char *url,
                         const char *username_from_url,
                         unsigned int allowed_types,
                         void *payload)
{
    const char *pat;
    char *pat_from_file = NULL;
    int ret;

    (void)url;
    (void)username_from_url;
    (void)payload;

    if (!(allowed_types & GIT_CREDENTIAL_USERPASS_PLAINTEXT))
        return -1;

    pat = getenv("AGIT_PAT");
    if (!pat)
    {
        pat_from_file = read_pat_from_file();
        pat = pat_from_file;
    }

    if (!pat)
    {
        git_error_set(GIT_ERROR_NET,
            "no PAT found: set AGIT_PAT or create PROGDIR:agit.config with GITHUB_PAT=<token>");
        return -1;
    }

    ret = git_credential_userpass_plaintext_new(out, "token", pat);
    free(pat_from_file);
    return ret;
}
