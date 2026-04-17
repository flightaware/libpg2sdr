#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "log.h"
#include "nanojson.h"
#include "meta.h"

static void show_image_info_help();
int subcommand_image_info(int argc, char * const argv[]);

static bool do_image_info(const char *path, bool json_output);

static void show_image_info_help()
{
    log_verbose("Usage: %s [OPTIONS] FIRMWARE-IMAGE\n"
                "This subcommand writes summary information about a firmware image file\n"
                "to stdout.\n"
                "\n"
                "Available options:\n"
                "\n"
                " -h, --help             show this help\n"
                " -q, --quiet            suppress informational logging, show errors only\n"
                " -j, --json             output machine-readable json to stdout",
                argv0);
}

int subcommand_image_info(int argc, char * const argv[])
{
    struct option opts[] = {
        { "help",   no_argument,       0, 'h' },
        { "quiet",  no_argument,       0, 'q' },
        { "json",   no_argument,       0, 'j' },
        { 0, 0, 0, 0 }
    };

    bool json_output = false;

    int opt;
    while ((opt = getopt_long(argc, argv, "hqj", opts, NULL)) != -1) {
        switch (opt) {
        case 'h':
            show_image_info_help(argv[0]);
            return EXIT_SUCCESS;

        case 'q':
            verbose_logging = false;
            break;

        case 'j':
            json_output = true;
            break;

        case '?':
            return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        log_error("a firmware image filename is required");
        return EXIT_FAILURE;
    }

    if (optind + 1 < argc) {
        log_error("only one firmware image filename is expected");
        return EXIT_FAILURE;
    }

    return do_image_info(argv[optind], json_output) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static bool do_image_info(const char *image_path, bool json_output)
{
    bool success = false;
    firmware_io_t *io = NULL;
    firmware_image_t *image = NULL;

    if (!(io = io_open_file(image_path)))
        goto cleanup;

    if (!(image = image_read(io)))
        goto cleanup;

    if (json_output) {
        json_set_output(stdout);
        json_start_object();
        json_firmware_image(image);
        json_end_object();
        fprintf(stdout, "\n");
    } else {
        fprintf(stdout, "Image file: %s\n", image_path);
        show_firmware_image("  ", image, stdout);
    }
    success = true;

 cleanup:
    if (image)
        image_free(image);
    if (io)
        io->close(io);

    return success;
}
