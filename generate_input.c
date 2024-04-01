#include <stdio.h>
#include <stdlib.h>
#include <time.h>


// Function to generate and write the data to 'input.txt'
void generate_input_file(int n, int h) {
    FILE *file = fopen("input.txt", "w");
    if (!file) {
        perror("Error opening file for writing");
        exit(EXIT_FAILURE);
    }

    srand((unsigned int)time(NULL)); // Seed the random number generator.

    fprintf(file, "%d\n", n + h); // Write the number of integers plus hidden keys as the first line.

    int hidden_inserted = 0;

    for (int i = 0; i < n; ++i) {
        // Randomly decide to insert a hidden key based on the ratio of remaining hidden keys to the total remaining numbers
        if ((rand() % (n + h - i)) < (h - hidden_inserted)) {
            int hidden_key = -(rand() % 60 + 1); // Generate a negative integer between -1 and -60.
            fprintf(file, "%d\n", hidden_key);
            hidden_inserted++;
        } else {
            int positive_integer = rand() % 100 + 1; // Generate a positive integer between 1 and MAX_POSITIVE.
            fprintf(file, "%d\n", positive_integer);
        }
    }

    // If not all hidden keys were inserted during the loop, add the rest now
    while (hidden_inserted < h) {
        int hidden_key = -(rand() % 60 + 1);
        fprintf(file, "%d\n", hidden_key);
        hidden_inserted++;
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <number of positive integers> <number of hidden keys>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int n = atoi(argv[1]);
    int h = atoi(argv[2]);

    // Validate the input.
    if (n <= 0 || h <= 0 || h > n) {
        fprintf(stderr, "Invalid input. Make sure the number of positive integers and hidden keys are positive, and the number of hidden keys does not exceed the number of positive integers.\n");
        return EXIT_FAILURE;
    }

    generate_input_file(n, h);
    printf("Generated input file with %d positive integers and %d hidden keys.\n", n, h);

    return EXIT_SUCCESS;
}
