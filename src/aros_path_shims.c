/*
 * aros_path_shims.c -- see src/aros-shims/aros_path_shims.h for the
 * full rationale.
 */

#include "aros-shims/aros_path_shims.h"

const char *aros_strip_dotslash(const char *path)
{
    if (!path)
        return path;

    while (path[0] == '.' && path[1] == '/')
        path += 2;

    return path;
}
