/*
 * ============================================================================
 *
 *       Filename:  args.c
 *
 *    Description:  Command line options parser using GNU getopt
 *
 *        Created:  24/03/2015 22:00:09 PM
 *       Compiler:  gcc
 *
 *         Author:  Gustavo Pantuza
 *   Organization:  Software Community
 *
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "messages.h"
#include "args.h"

/*
 * Sets the default options
 */
static void set_default_options(options_t* options) {
    options->help = false;
    options->version = false;
    options->use_colors = true;
    strncpy(options->file_name, "-", FILE_NAME_SIZE);
    strncpy(options->dev, "/dev/stdout", DEV_NAME_SIZE);
}

/*
 * Finds the matching case of the current command line option
 */
void switch_options(int arg, options_t* options) {
    switch (arg) {
        case 'h':
            options->help = true;
            help();
            exit(EXIT_SUCCESS);

        case 'v':
            options->version = true;
            version();
            exit(EXIT_SUCCESS);
        case 'd':
            strncpy(options->dev, optarg, DEV_NAME_SIZE);
            break;
        case 0:
            options->use_colors = false;
            break;

        case '?':
            usage();
            exit(EXIT_FAILURE);

        default:
            usage();
            abort();
    }
}

/*
 * Tries to get the file name. Otherwise, gets stdin
 */
void get_file_name(int argc, char* argv[], options_t* options) {
    /* If there is more arguments, probably, it is an input file */
    if (optind < argc) {
        strncpy(options->file_name, argv[optind++], FILE_NAME_SIZE);
    } else {
        strncpy(options->file_name, "-", FILE_NAME_SIZE);
    }
}

/*
 * Public function that loops until command line options were parsed
 */
void options_parser(int argc, char* argv[], options_t* options) {
    set_default_options(options);

    /* Current option */

    /* getopt allowed options */
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"dev", required_argument, 0, 'd'},
        {"no-colors", no_argument, 0, 0},
    };

    while (true) {
        int option_index = 0;
        const int arg = getopt_long(argc, argv, "hvd:t:", long_options, &option_index);
        /* End of the options? */
        if (arg == -1) {
            break;
        }
        /* Find the matching case of the argument */
        switch_options(arg, options);
    }

    /* Gets the file name or exits with error */
    get_file_name(argc, argv, options);
}
