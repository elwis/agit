#include "aros_cred.h"
#include "aros_net.h"
#include <git2.h>
#include <stdio.h>
#include <string.h>

static void print_usage(void)
{
    fprintf(stderr, "USAGE: agit <clone|add|commit|push> [args...]\n");
}

static int sideband_progress(const char *str, int len, void *payload)
{
    (void)payload;
    printf("remote: %.*s", len, str);
    fflush(stdout);
    return 0;
}

static int fetch_progress(const git_indexer_progress *stats, void *payload)
{
    (void)payload;
    if (stats->total_objects > 0)
    {
        int pct = (int)(100LL * stats->received_objects / stats->total_objects);
        size_t kb = stats->received_bytes / 1024;
        printf("\rfetch: %3d%% (%zu kb, %u/%u objects)  ", pct, kb,
               stats->received_objects, stats->total_objects);
        fflush(stdout);
    }
    return 0;
}

static int cmd_clone(int argc, char **argv)
{
    git_repository *cloned_repo = NULL;
    git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
    const char *url, *path;
    int error;

    if (argc < 3)
    {
        fprintf(stderr, "USAGE: %s <url> <path>\n", argv[0]);
        return 1;
    }
    url = argv[1];
    path = argv[2];

    checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
    clone_opts.checkout_opts = checkout_opts;
    clone_opts.fetch_opts.callbacks.sideband_progress = sideband_progress;
    clone_opts.fetch_opts.callbacks.transfer_progress = fetch_progress;
    clone_opts.fetch_opts.callbacks.credentials = aros_cred_acquire_cb;

    printf("Cloning into '%s'...\n", path);
    error = git_clone(&cloned_repo, url, path, &clone_opts);
    printf("\n");

    if (error != 0)
    {
        const git_error *err = git_error_last();
        if (err)
            fprintf(stderr, "ERROR %d: %s\n", err->klass, err->message);
        else
            fprintf(stderr, "ERROR %d: no detailed info\n", error);
        return 1;
    }

    printf("Clone complete.\n");
    git_repository_free(cloned_repo);
    return 0;
}

static int cmd_add(int argc, char **argv)
{
    git_repository *repo = NULL;
    git_index *index = NULL;
    int error, i;

    if (argc < 2)
    {
        fprintf(stderr, "USAGE: %s <path> [<path>...]\n", argv[0]);
        return 1;
    }

    error = git_repository_open_ext(&repo, ".", 0, NULL);
    if (error)
    {
        const git_error *err = git_error_last();
        fprintf(stderr, "ERROR: %s\n", err ? err->message : "could not open repo");
        return 1;
    }

    error = git_repository_index(&index, repo);
    if (error)
    {
        const git_error *err = git_error_last();
        fprintf(stderr, "ERROR: %s\n", err ? err->message : "could not open index");
        git_repository_free(repo);
        return 1;
    }

    for (i = 1; i < argc; i++)
    {
        error = git_index_add_bypath(index, argv[i]);
        if (error)
        {
            const git_error *err = git_error_last();
            fprintf(stderr, "ERROR adding '%s': %s\n", argv[i],
                    err ? err->message : "unknown error");
        }
        else
        {
            printf("add '%s'\n", argv[i]);
        }
    }

    git_index_write(index);
    git_index_free(index);
    git_repository_free(repo);
    return 0;
}

static int cmd_commit(int argc, char **argv)
{
    git_repository *repo = NULL;
    git_index *index = NULL;
    git_tree *tree = NULL;
    git_commit *parent = NULL;
    git_reference *ref = NULL;
    git_signature *sig = NULL;
    git_oid tree_oid, commit_oid;
    char *name, *email;
    const char *comment;
    int error, nparents;

    if (argc < 3 || strcmp(argv[1], "-m") != 0)
    {
        fprintf(stderr, "USAGE: %s -m <message>\n", argv[0]);
        return 1;
    }
    comment = argv[2];

    name = aros_get_config("GIT_USER_NAME");
    email = aros_get_config("GIT_USER_EMAIL");
    if (!name || !email)
    {
        fprintf(stderr, "ERROR: GIT_USER_NAME and GIT_USER_EMAIL must be set in agit.config\n");
        free(name);
        free(email);
        return 1;
    }

    error = git_signature_now(&sig, name, email);
    free(name);
    free(email);
    if (error)
    {
        const git_error *err = git_error_last();
        fprintf(stderr, "ERROR creating signature: %s\n",
                err ? err->message : "unknown error");
        return 1;
    }

    error = git_repository_open_ext(&repo, ".", 0, NULL);
    if (error)
    {
        const git_error *err = git_error_last();
        fprintf(stderr, "ERROR: %s\n", err ? err->message : "could not open repo");
        git_signature_free(sig);
        return 1;
    }

    error = git_revparse_ext((git_object **)&parent, &ref, repo, "HEAD");
    nparents = 0;
    if (error == GIT_ENOTFOUND)
    {
        printf("First commit\n");
    }
    else if (error != 0)
    {
        const git_error *err = git_error_last();
        fprintf(stderr, "ERROR: %s\n", err ? err->message : "could not resolve HEAD");
        goto out;
    }
    else
    {
        nparents = 1;
    }

    error = git_repository_index(&index, repo);
    if (error)
    {
        const git_error *err = git_error_last();
        fprintf(stderr, "ERROR: %s\n", err ? err->message : "could not open index");
        goto out;
    }

    error = git_index_write_tree(&tree_oid, index);
    if (error)
    {
        const git_error *err = git_error_last();
        fprintf(stderr, "ERROR writing tree: %s\n",
                err ? err->message : "unknown error");
        goto out;
    }

    git_index_write(index);

    error = git_tree_lookup(&tree, repo, &tree_oid);
    if (error)
    {
        const git_error *err = git_error_last();
        fprintf(stderr, "ERROR looking up tree: %s\n",
                err ? err->message : "unknown error");
        goto out;
    }

    error = git_commit_create_v(
        &commit_oid, repo, "HEAD",
        sig, sig, NULL, comment,
        tree, nparents, parent);
    if (error)
    {
        const git_error *err = git_error_last();
        fprintf(stderr, "ERROR creating commit: %s\n",
                err ? err->message : "unknown error");
        goto out;
    }

    {
        char oid_str[GIT_OID_HEXSZ + 1];
        git_oid_tostr(oid_str, sizeof(oid_str), &commit_oid);
        printf("[%s] %s\n", oid_str, comment);
    }

out:
    git_signature_free(sig);
    git_index_free(index);
    git_tree_free(tree);
    git_commit_free(parent);
    git_reference_free(ref);
    git_repository_free(repo);
    return error;
}

static int cmd_push(int argc, char **argv)
{
    git_repository *repo = NULL;
    git_reference *head = NULL;
    git_remote *remote = NULL;
    git_remote_callbacks callbacks = GIT_REMOTE_CALLBACKS_INIT;
    git_push_options options = GIT_PUSH_OPTIONS_INIT;
    char *refspec;
    git_strarray refspecs;
    int error;

    if (argc > 1)
    {
        fprintf(stderr, "USAGE: %s\n", argv[0]);
        return 1;
    }

    error = git_repository_open_ext(&repo, ".", 0, NULL);
    if (error)
    {
        const git_error *err = git_error_last();
        fprintf(stderr, "ERROR: %s\n", err ? err->message : "could not open repo");
        return 1;
    }

    error = git_repository_head(&head, repo);
    if (error)
    {
        const git_error *err = git_error_last();
        fprintf(stderr, "ERROR: %s\n", err ? err->message : "could not get HEAD");
        git_repository_free(repo);
        return 1;
    }

    refspec = (char *)git_reference_name(head);
    refspecs.count = 1;
    refspecs.strings = &refspec;

    error = git_remote_lookup(&remote, repo, "origin");
    if (error)
    {
        const git_error *err = git_error_last();
        fprintf(stderr, "ERROR: %s\n", err ? err->message : "could not find remote 'origin'");
        git_reference_free(head);
        git_repository_free(repo);
        return 1;
    }

    callbacks.credentials = aros_cred_acquire_cb;
    options.callbacks = callbacks;

    printf("Pushing %s to origin...\n", refspec);
    error = git_remote_push(remote, &refspecs, &options);
    if (error)
    {
        const git_error *err = git_error_last();
        fprintf(stderr, "ERROR: %s\n", err ? err->message : "push failed");
    }
    else
    {
        printf("Pushed.\n");
    }

    git_remote_free(remote);
    git_reference_free(head);
    git_repository_free(repo);
    return error;
}

int main(int argc, char **argv)
{
    int ret = 1;

    if (argc < 2)
    {
        print_usage();
        return 1;
    }

    if (aros_net_init() != 0)
    {
        fprintf(stderr, "Unable to open bsdsocket.library\n");
        return 1;
    }

    git_libgit2_init();
    git_libgit2_opts(GIT_OPT_SET_SSL_CERT_LOCATIONS, "PROGDIR:cacert.pem", NULL);
    git_libgit2_opts(GIT_OPT_SET_OWNER_VALIDATION, 0);

    {
        char *name = aros_get_config("GIT_USER_NAME");
        char *email = aros_get_config("GIT_USER_EMAIL");
        char *pat = aros_get_config("GITHUB_PAT");
        const char *pat_status = pat ? "found" : "missing";

        printf("GIT_USER_NAME: %s\n", name ? name : "(not set)");
        printf("GIT_USER_EMAIL: %s\n", email ? email : "(not set)");
        printf("GITHUB_PAT: %s\n", pat_status);

        free(name);
        free(email);
        free(pat);
    }

    if (strcmp(argv[1], "clone") == 0)
        ret = cmd_clone(argc - 1, argv + 1);
    else if (strcmp(argv[1], "add") == 0)
        ret = cmd_add(argc - 1, argv + 1);
    else if (strcmp(argv[1], "commit") == 0)
        ret = cmd_commit(argc - 1, argv + 1);
    else if (strcmp(argv[1], "push") == 0)
        ret = cmd_push(argc - 1, argv + 1);
    else
    {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_usage();
        ret = 1;
    }

    git_libgit2_shutdown();
    aros_net_shutdown();

    return ret;
}
