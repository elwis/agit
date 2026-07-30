#ifndef AROS_CRED_H
#define AROS_CRED_H

#include <git2.h>

int aros_cred_acquire_cb(git_credential **out,
                         const char *url,
                         const char *username_from_url,
                         unsigned int allowed_types,
                         void *payload);

#endif
