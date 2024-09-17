#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include "../src/lab.h"
#include <readline/readline.h>
#include <readline/history.h>

// Function to handle command-line arguments using getopt
void handle_getopt(int argc, char *argv[]) {
    int c;
    while((c = getopt(argc, argv, "abc:v")) != -1) {
        switch(c) {
            case 'a':
                printf("Option -a selected\n");
                break;
            case 'b':
                printf("Option -b selected\n");
                break;
            case 'c':
                printf("Option -c selected with value %s\n", optarg);
                break;
            case 'v':
                printf("Version %d.%d\n", lab_VERSION_MAJOR, lab_VERSION_MINOR);
                exit(0);
            case '?':
                if (optopt == 'c') {
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                } else if (isprint(optopt)) {
                    fprintf(stderr, "Unknown option `-%c`.\n", optopt);
                } else {
                    fprintf(stderr, "Unknown option character `\\x%x`.\n", optopt);
                }
                exit(1);
            default:
                abort();
        }
    }
}

int main(int argc, char *argv[]) {
    // Handle getopt logic
    handle_getopt(argc, argv);

    // Initialize Readline for interactive input
    char *line;
    using_history();  // Initialize history

    while ((line = readline("$ "))) {  // Display "$ " as the prompt
        if (*line) {  // Check if the line is not empty
            printf("You entered: %s\n", line);
            add_history(line);  // Add the line to history for arrow key navigation
        }
        free(line);  // Free the allocated memory from readline
    }

    return 0;
}
