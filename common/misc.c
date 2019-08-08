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
 * @file
 * Contains misc useful functions that may be useful to various parts of code,
 * but are not especially tied to it.
 */

#include "client.h"

#include <errno.h>

#ifndef WIN32
#include <sys/wait.h>
#else
#include <direct.h>
#include <io.h>
#endif

GTimer* global_time;

/** Log level, or the threshold below which messages are suppressed. */
LogLevel MINLOG = LOG_INFO;

/**
 * Convert a buffer of a specified maximum size by replacing token characters
 * with a provided string.  Given a buffered template string "/input/to/edit",
 * the maximum size of the buffer, a token '/', and a replacement string ":",
 * the input string is transformed to ":input:to:edit".  If the replacement
 * string is empty, the token characters are simply removed.  The template is
 * processed from left to right, replacing token characters as they are found.
 * Replacement strings are always inserted whole.  If token replacement would
 * overflow the size of the conversion buffer, the token is not replaced, and
 * the remaining portion of the input string is appended after truncating it
 * as required to avoid overfilling the buffer.
 * @param buffer      A string to perform a find and replace operation on.
 * @param buffer_size Allocated buffer size (used to avoid buffer overflow).
 * @param find        A token character to find and replace in the buffer.
 * @param replace     A string that is to replace each token in the buffer.
 */
void replace_chars_with_string(char*        buffer,
                               const guint16 buffer_size,
                               const char   find,
                               const char*  replace      )
{

    guint16 buffer_len, expand, i, replace_len, replace_limit, template_len;
    char*  template;

    replace_limit = buffer_size - 1;
    replace_len = strlen(replace);
    template_len = strlen(buffer);
    template = g_strdup(buffer);
    buffer[0] = '\0';

    buffer_len = 0;
    for (i = 0; i <= template_len; i++) {
        expand = buffer_len + replace_len < replace_limit ? replace_len : 1;
        if (expand == 1 && buffer_len == replace_limit) {
            break;
        }
        if ((template[i] != find) || ((expand == 1) && (replace_len > 1))) {
            buffer[buffer_len++] = template[i];
            buffer[buffer_len] = '\0';
        } else {
            strcat(buffer, replace);
            buffer_len += replace_len;
        }
    }
    free(template);
}

/**
 * If any directories in the given path doesn't exist, they are created.
 */
int make_path_to_file(char *filename) {
    gchar *dirname = g_path_get_dirname(filename);
    int result = g_mkdir_with_parents(dirname, 0755);
    g_free(dirname);
    return result;
}

static const char *getLogLevelText(LogLevel level) {
    const char *LogLevelTexts[] = {
        "\x1b[34;1m" "DD" "\x1b[0m",
        "\x1b[32;1m" "II" "\x1b[0m",
        "\x1b[35;1m" "WW" "\x1b[0m",
        "\x1b[31;1m" "EE" "\x1b[0m",
        "\x1b[31;1m" "!!" "\x1b[0m",
        "\x1b[30;1m" "??" "\x1b[0m",
    };

    return LogLevelTexts[level > LOG_CRITICAL ? LOG_CRITICAL + 1 : level];
}

/**
 * Log messages of a certain importance to stderr. See 'client.h' for a full
 * list of possible log levels.
 */
void LOG(LogLevel level, const char *origin, const char *format, ...) {
    va_list ap;

    /* This buffer needs to be very big - larger than any other buffer. */
    char buf[20480];

    /* Don't log messages that the user doesn't want. */
    if (level < MINLOG) {
        return;
    }

    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);

    if (strlen(buf) > 0) {
        fprintf(stderr, "[%s] %lf (%s) %s\n", getLogLevelText(level), g_timer_elapsed(global_time, NULL), origin, buf);
    }

    va_end(ap);
}
