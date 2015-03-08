/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2013 Mark Wedel and the Crossfire Development Team
 * Copyright (c) 1992 Frank Tore Johansen
 *
 * Crossfire is free software and comes with ABSOLUTELY NO WARRANTY. You are
 * welcome to redistribute it under certain conditions. For details, please
 * see COPYING and LICENSE.
 *
 * The authors can be reached via e-mail at <crossfire@metalforge.org>.
 */

/**
 * @file sound-src/main.c
 */

#include "client.h"

#include "sndproto.h"
#include "version.h"

/** Print detailed version information. */
static void print_version() {
    printf("Crossfire Sound Server %s\n", FULL_VERSION);
}

/** Print a message stating how to get help. */
static void print_quickhelp() {
    fprintf(stderr, "Type 'cfsndserv -h' for usage.\n");
}

/** Print out usage information. */
static void print_usage() {
    printf(
        "Usage: cfsndserv [options]\n"
        "\n"
        "Options:\n"
        "  -h   display this help message\n"
        "  -t   run sanity tests\n"
        "  -v   display version information\n"
    );
}

int main(int argc, char *argv[]) {
    int flag, test = 0;

    /* Parse command-line arguments. */
    while ((flag = getopt(argc, argv, "htv")) != -1) {
        switch (flag) {
            case 'h':
                print_usage();
                exit(EXIT_SUCCESS);
                break;
            case 't':
                test = 1;
                break;
            case 'v':
                print_version();
                exit(EXIT_SUCCESS);
                break;
            case '?':
                print_quickhelp();
                exit(EXIT_FAILURE);
                break;
        }
    }

    /* Initialize sound server, exit on failure. */
    if (init() != 0) {
        exit(EXIT_FAILURE);
    }

    /* If not running sanity test, start server. */
    if (!test) {
        sdl_mixer_server();
    } else {
        printf("===>>> Sanity tests PASSED!\n");
    }

    exit(EXIT_SUCCESS);
}
