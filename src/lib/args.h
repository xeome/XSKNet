/*
 * ============================================================================
 *
 *       Filename:  args.h
 *
 *    Description:  Header file of the command line options parser
 *
 *        Created:  24/03/2016 21:30:39 PM
 *       Compiler:  gcc
 *
 *         Author:  Gustavo Pantuza
 *   Organization:  Software Community
 *
 * ============================================================================
 */

#ifndef ARGS_H
#define ARGS_H

#include <stdbool.h>

/* Max size of a file name */
#define FILE_NAME_SIZE 512
/* Max size of a device name */
#define DEV_NAME_SIZE 128

/* Defines the command line allowed options struct */
struct options {
    bool help;
    bool version;
    bool use_colors;
    char file_name[FILE_NAME_SIZE];
    char dev[DEV_NAME_SIZE];
};

/* Exports options as a global type */
typedef struct options options_t;

/* Public functions section */
void options_parser(int argc, char* argv[], options_t* options);
extern options_t opts;

#endif  // ARGS_H
