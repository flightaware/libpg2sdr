#include "internal/core.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <getopt.h>
#include <libgen.h>

typedef struct {
    const char *name;
    const char *brief;
    int (*handler)(int argc, char * const argv[]);
} subcommand_t;

int subcommand_help(int argc, char * const argv[]);
int subcommand_load(int argc, char * const argv[]);
int subcommand_write(int argc, char * const argv[]);
int subcommand_verify(int argc, char * const argv[]);
int subcommand_device_info(int argc, char * const argv[]);
int subcommand_image_info(int argc, char * const argv[]);
int subcommand_reset(int argc, char * const argv[]);
int subcommand_standby(int argc, char * const argv[]);
int subcommand_blink(int argc, char * const argv[]);

static const char *base_argv0;

/* shared pg2sdr_context */
pg2sdr_context *shared_pg2sdr_ctx = NULL;

static void log_callback(pg2sdr_context *context,
                         pg2sdr_log_level level,
                         const char *message)
{
    if (level <= PG2SDR_LOG_DEBUG)
        return;

    if (level <= PG2SDR_LOG_INFO)
        log_verbose("libpg2sdr: %s", message);
    else
        log_error("libpg2sdr: %s", message);
}

static bool setup_shared_ctx()
{
    int error;
    pg2sdr_context *ctx = NULL;
    if ((error = pg2sdr_init(&ctx)) < 0) {
        log_perror_pg2sdr(error, "pg2sdr_init");
        return false;
    }

    if ((error = pg2sdr_set_log_callback(ctx, log_callback)) < 0) {
        log_perror_pg2sdr(error, "pg2sdr_set_log_callback");
        pg2sdr_exit(ctx);
        return false;
    }

    shared_pg2sdr_ctx = ctx;
    return true;
}

static void cleanup_shared_ctx()
{
    if (shared_pg2sdr_ctx)
        pg2sdr_exit(shared_pg2sdr_ctx);
    shared_pg2sdr_ctx = NULL;
}

static const subcommand_t subcommands[] = {
    {
        .name = "help",
        .brief = "Show detailed help on a subcommand",
        .handler = subcommand_help,
    },

    {
        .name = "load-firmware",
        .brief = "Download a firmware image to a ProStick, once",
        .handler = subcommand_load,
    },

    {
        .name = "write-firmware",
        .brief = "Write a firmware image to ProStick flash storage",
        .handler = subcommand_write,
    },

    {
        .name = "verify-firmware",
        .brief = "Compare a firmware image to the contents of ProStick flash storage",
        .handler = subcommand_verify,
    },

    {
        .name = "device-info",
        .brief = "List connected devices, and their firmware status",
        .handler = subcommand_device_info
    },

    {
        .name = "image-info",
        .brief = "Show information about a firmware image file",
        .handler = subcommand_image_info
    },

    {
        .name = "reset",
        .brief = "Reset a ProStick device",
        .handler = subcommand_reset
    },

    {
        .name = "standby",
        .brief = "Put a ProStick device into standby mode",
        .handler = subcommand_standby
    },

    {
        .name = "blink",
        .brief = "Blink ProStick LEDs in a pattern",
        .handler = subcommand_blink
    },
};

static void usage() {
    log_verbose("pg2-util: utility to manage ProStick Gen 2 firmware images\n");
    log_verbose("Usage: %s <subcommand> [options..]\n", base_argv0);
    log_verbose("Available subcommands (try '%s help <subcommand>' for details):", base_argv0);

    int longest = 0;
    for (size_t i = 0; i < sizeof(subcommands)/sizeof(subcommands[0]); ++i) {
        if (strlen(subcommands[i].name) > longest)
            longest = strlen(subcommands[i].name);
    }

    for (size_t i = 0; i < sizeof(subcommands)/sizeof(subcommands[0]); ++i) {
        log_verbose("  %s %-*s  %s",
                    base_argv0,
                    longest,
                    subcommands[i].name,
                    subcommands[i].brief);
    }
}

static const subcommand_t *find_subcommand(const char *name)
{
    bool ambiguous = false;
    const subcommand_t *prefix_match = NULL;

    for (size_t i = 0; i < sizeof(subcommands)/sizeof(subcommands[0]); ++i) {
        /* exact match always wins */
        if (!strcasecmp(name, subcommands[i].name))
            return &subcommands[i];

        /* prefix matching on subcommand names, only succeed if the prefix is unambiguous */
        if (!ambiguous && strlen(name) < strlen(subcommands[i].name) && !strncasecmp(name, subcommands[i].name, strlen(name))) {
            if (prefix_match)
                ambiguous = true;
            else
                prefix_match = &subcommands[i];
        }
    }

    return ambiguous ? NULL : prefix_match;
}

static int dispatch_subcommand(const char *subcommand_name, int argc, char *argv[])
{
    const subcommand_t *sc = find_subcommand(subcommand_name);
    if (!sc) {
        log_error("unknown subcommand %s", subcommand_name);
        usage();
        return EXIT_FAILURE;
    }

    /* tweak argv[0] to include the subcommand name i.e. "pg2-util load" */
    size_t argv0_len = strlen(base_argv0) + strlen(sc->name) + 2;
    argv[0] = malloc(argv0_len);
    if (!argv[0]) {
        log_perror("malloc");
        return EXIT_FAILURE;
    }
    snprintf(argv[0], argv0_len, "%s %s", base_argv0, sc->name);
    argv0 = argv[0];
    return sc->handler(argc, argv);
}

int subcommand_help(int argc, char * const argv[])
{
    /*
     *  pg2-util help [subcommand]
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
        /* "pg2-util help", no subcommand given */
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

    /* init shared context (for logging, etc) before doing anything further */
    if (!setup_shared_ctx())
        return EXIT_FAILURE;

    int rc = dispatch_subcommand(subcommand_name, argc - 1, sub_argv);
    cleanup_shared_ctx();
    return rc;
}
