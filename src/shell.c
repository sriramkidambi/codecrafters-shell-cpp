#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024

int main(void) {
    char input[BUFFER_SIZE];

    while (1) {
        // Print the shell prompt with a trailing space.
        printf("$ ");
        fflush(stdout);

        // Wait for user input (this will block until input is provided).
        if (fgets(input, BUFFER_SIZE, stdin) == NULL) {
            // If an EOF (e.g., Ctrl+D) is encountered, exit the shell.
            break;
        }
        
        // In this stage we do not process the input.
        // Future stages will handle command parsing and execution.
    }

    return EXIT_SUCCESS;
} 