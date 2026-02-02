#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <getopt.h>
#include <libgen.h>

#include "log.h"

typedef struct {
    const char *name;
    const char *brief;
    int (*handler)(int argc, char * const argv[]);
} subcommand_t;

int subcommand_help(int argc, char * const argv[]);
int subcommand_load(int argc, char * const argv[]);
int subcommand_write(int argc, char * const argv[]);
//int subcommand_info(int argc, char * const argv[]);
//int subcommand_list_devices(int argc, char * const argv[]);

static const char *base_argv0;

static const subcommand_t subcommands[] = {
    {
        .name = "help",
        .brief = "Show detailed help on a subcommand",
        .handler = subcommand_help,
    },

    {
        .name = "load",
        .brief = "Download a firmware image to a ProStick, once",
        .handler = subcommand_load,
    },

    {
        .name = "write",
        .brief = "Write a firmware image to ProStick flash storage",
        .handler = subcommand_write,
    },

#if 0
    {
        .name = "info"
        .brief = "Show info about a firmware image file, or firmware stored on flash",
        .handler = subcommand_info,
    },

    {
        .name = "list-devices",
        .brief = "List connected devices, and their firmware status",
        .handler = subcommand_list_devices
    },
#endif
};

static void usage() {
    log_verbose("pg2-firmware: utility to manage ProStick Gen 2 firmware images\n");
    log_verbose("Usage: %s <subcommand> [options..]\n", base_argv0);
    log_verbose("Available subcommands (try '%s help <subcommand>' for details):", base_argv0);

    for (size_t i = 0; i < sizeof(subcommands)/sizeof(subcommands[0]); ++i) {
        log_verbose("  %s %-10s  %s",
                    base_argv0,
                    subcommands[i].name,
                    subcommands[i].brief);
    }
}

static const subcommand_t *find_subcommand(const char *name)
{
    for (size_t i = 0; i < sizeof(subcommands)/sizeof(subcommands[0]); ++i) {
        if (!strcasecmp(name, subcommands[i].name))
            return &subcommands[i];
    }

    return NULL;
}

static int dispatch_subcommand(const char *subcommand_name, int argc, char *argv[])
{
    const subcommand_t *sc = find_subcommand(subcommand_name);
    if (!sc) {
        log_error("unknown subcommand %s", subcommand_name);
        usage();
        return EXIT_FAILURE;
    }

    /* tweak argv[0] to include the subcommand name i.e. "pg2-firmware load" */
    size_t argv0_len = strlen(base_argv0) + strlen(subcommand_name) + 2;
    argv[0] = malloc(argv0_len);
    if (!argv[0]) {
        log_perror("malloc");
        return EXIT_FAILURE;
    }
    snprintf(argv[0], argv0_len, "%s %s", base_argv0, subcommand_name);
    argv0 = argv[0];
    return sc->handler(argc, argv);
}

int subcommand_help(int argc, char * const argv[])
{
    /*
     *  pg2-firmware help [subcommand]
     */

    struct option options[] = {
        { "help", 0, NULL, 'h' },
        { "", 0, NULL, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "h", options, NULL)) != -1) {
        switch (opt) {
        case '?':
            usage();
            return EXIT_FAILURE;
        case 'h':
            usage();
            return EXIT_SUCCESS;
        }
    }

    if (optind >= argc) {
        /* "pg2-firmware help", no subcommand given */
        usage();
        return EXIT_SUCCESS;
    }

    char *dummy_argv[3] = {
        NULL,
        "-h"
    };

    return dispatch_subcommand(argv[optind], 2, dummy_argv);
}

int main(int argc, char * const argv[])
{
    base_argv0 = argv0 = basename(strdup(argv[0])); /* we don't care if this leaks once */

    /* find subcommand, build sub-argv containing everything except the subcommand name */
    char **sub_argv = calloc(argc - 1, sizeof(char*));
    if (!sub_argv) {
        log_perror("calloc");
        return EXIT_FAILURE;
    }

    char *subcommand_name = NULL;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') {
            /* found a subcommand */
            subcommand_name = argv[i];
            for (int j = i + 1; j < argc; ++j)
                sub_argv[j-1] = argv[j];
            break;
        }
        sub_argv[i] = argv[i];
    }

    if (!subcommand_name) {
        usage();
        return EXIT_FAILURE;
    }

    return dispatch_subcommand(subcommand_name, argc - 1, sub_argv);
}
