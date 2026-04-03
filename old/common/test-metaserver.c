#include <stdio.h>

#include "metaserver.h"

static void print_entry(char *server, int update, int players, char *version,
                        char *comment, bool compatible) {
    printf("%s:%d:%s:%s\n", server, players, version, comment);
}

int main() {
    ms_init();
    ms_fetch(print_entry);
}
