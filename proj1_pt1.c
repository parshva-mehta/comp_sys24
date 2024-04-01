#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>

#define NUM_CHILDREN 4
#define HIDDEN_KEY_LOWER_BOUND -60 // Define the range for hidden keys.
#define HIDDEN_KEY_UPPER_BOUND -1

// Function to read the data from the file into an array.
int *read_data(const char *filename, int *size) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    fscanf(file, "%d", size); // Assuming the first number in the file is the size.
    int *data = malloc((*size) * sizeof(int));
    if (!data) {
        perror("Malloc failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < *size; ++i) {
        if (fscanf(file, "%d", &data[i]) != 1) {
            perror("Failed to read integer from file");
            exit(EXIT_FAILURE);
        }
    }

    fclose(file);
    return data;
}

// Function for a child process to find the max in its data segment.
int find_max(int *data, int start, int end) {
    printf("Child %d: Start computation.\n", getpid());
    int max = data[start];
    for (int i = start + 1; i < end; ++i) {
        if (data[i] > max) max = data[i];
    }
    printf("Child %d: Computation completed. Max found: %d\n", getpid(), max);
    return max;
}

void process_data_segment(int *data, int start, int end, int child_idx) {
    int max = data[start];
    int count_hidden = 0;  // Count of hidden keys found.
    int sum = 0;           // Sum of values for average calculation.

    for (int i = start; i < end; ++i) {
        if (data[i] > max) max = data[i];
        if (data[i] >= HIDDEN_KEY_LOWER_BOUND && data[i] <= HIDDEN_KEY_UPPER_BOUND) count_hidden++;
        sum += data[i];
    }

    int avg = sum / (end - start);  // Calculate average.

    // Open the output file and append the result.
    int fd = open("output.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    dprintf(fd, "Hi I'm process %d with return arg %d and my parent is %d.\n", getpid(), max, getppid());
    int foundKeys = 0;
    for (int i = start; i < end; ++i) {
        if (data[i] >= HIDDEN_KEY_LOWER_BOUND && data[i] <= HIDDEN_KEY_UPPER_BOUND) {
            dprintf(fd, "I found the hidden key in position A[%d].\n", i);
            foundKeys++;
        }
    }
    dprintf(fd, "Max=%d, Avg=%d\n", max, avg); // Report the max and average.
    dprintf(fd, "Keys Found: %d\n", foundKeys);
    close(fd);
}


int main() {
    int size;
    int *data = read_data("input.txt", &size); // Assuming this function is defined elsewhere.
    int segment_size = size / NUM_CHILDREN;
    pid_t pids[NUM_CHILDREN], original_parent_pid;

    original_parent_pid = getpid();
    printf("Parent process %d: Starting program.\n", original_parent_pid);

    // Clear output file or create it if it doesn't exist
    int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }
    close(fd);

    // Fork processes and assign them segments of data
    for (int i = 0; i < NUM_CHILDREN; ++i) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pids[i] == 0) { // Child process
            int start = i * segment_size;
            int end = (i == NUM_CHILDREN - 1) ? size : (i + 1) * segment_size;
            process_data_segment(data, start, end, i); // Assuming this function is defined elsewhere.
            free(data);
            exit(EXIT_SUCCESS);
        }
    }



    // Parent process waits for child processes to complete
    for (int i = 0; i < NUM_CHILDREN; ++i) {
        waitpid(pids[i], NULL, 0);
    }

    if (getpid() == original_parent_pid) {
        printf("Parent process %d: Children have completed.\n", original_parent_pid);
        printf("Parent process %d: Terminating.\n", original_parent_pid);
    }

    free(data);
    return 0;
}