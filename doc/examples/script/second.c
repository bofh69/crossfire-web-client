#include <stdbool.h>
#include <stdio.h>

#define LENGTH 200

int main() {
    char buffer[LENGTH];

    // Ask the client for a copy of all commands sent to the server.
    fprintf(stdout, "monitor\n");
    fflush(stdout);

    // Read the commands and print to standard error.
    while (fgets(buffer, LENGTH, stdin) != NULL) {
        fputs(buffer, stderr);
    }

    return 0;
}
