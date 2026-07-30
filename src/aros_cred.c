#include "aros_cred.h"
#include <git2/sys/errors.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_config_value(const char *key)
{
    static const char *paths[] = {
        "PROGDIR:agit.config",
        "agit.config",
        NULL
    };
    size_t klen;
    int i;

    if (!key)
        return NULL;
    klen = strlen(key);

    for (i = 0; paths[i]; i++)
    {
        FILE *f;
        char line[512];

        f = fopen(paths[i], "r");
        if (!f)
            continue;

        while (fgets(line, sizeof(line), f))
        {
            char *val;
            size_t vlen;

            if (strncmp(line, key, klen) != 0)
                continue;
            if (line[klen] != '=')
                continue;

            vlen = strlen(line + klen + 1);
            if (vlen > 0 && line[klen + 1 + vlen - 1] == '\n')
                vlen--;
            val = malloc(vlen + 1);
            if (val)
            {
                memcpy(val, line + klen + 1, vlen);
                val[vlen] = '\0';
            }
            fclose(f);
            return val;
        }

        fclose(f);
    }

    return NULL;
}

char *aros_get_config(const char *key)
{
    if (!key)
        return NULL;

    if (strcmp(key, "GITHUB_PAT") == 0)
    {
        const char *env = getenv("AGIT_PAT");
        if (env)
            return strdup(env);
    }

    return read_config_value(key);
}

int aros_cred_acquire_cb(git_credential **out,
                         const char *url,
                         const char *username_from_url,
                         unsigned int allowed_types,
                         void *payload)
{
    char *pat;
    int ret;

    (void)url;
    (void)username_from_url;
    (void)payload;

    if (!(allowed_types & GIT_CREDENTIAL_USERPASS_PLAINTEXT))
        return -1;

    pat = aros_get_config("GITHUB_PAT");
    if (!pat)
    {
        git_error_set(GIT_ERROR_NET,
            "no PAT found: set AGIT_PAT or create PROGDIR:agit.config with GITHUB_PAT=<token>");
        return -1;
    }

    ret = git_credential_userpass_plaintext_new(out, "token", pat);
    free(pat);
    return ret;
}
