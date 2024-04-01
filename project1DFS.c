#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>

#define MAX_POSITIVE_INT 10000
#define MIN_NEGATIVE_INT -60
#define HIDDEN_KEYS_COUNT 60

// Function to read the data from the file into an array.
int *read_data(const char *filename, int *size) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    fscanf(file, "%d", size);
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

// Function for a child process to find the max and hidden keys in its data segment.
void process_data_segment(int *data, int start, int end, int child_idx) {
    int max = data[start];
    int count_hidden = 0;  // Count of hidden keys found.
    int sum = 0;           // Sum of values for average calculation.

    for (int i = start; i < end; ++i) {
        if (data[i] > max) max = data[i];
        if (data[i] >= MIN_NEGATIVE_INT && data[i] <= -1) count_hidden++;
        sum += data[i];
    }

    float avg = (float)sum / (end - start);  // Calculate average.

    // Open the output file and append the result.
    int fd = open("output-DFS.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    dprintf(fd, "Hi I'm process %d with return arg %d and my parent is %d.\n", getpid(), max, getppid());
    for (int i = start; i < end; ++i) {
        if (data[i] >= MIN_NEGATIVE_INT && data[i] <= -1) {
            dprintf(fd, "I found the hidden key %d in position A[%d].\n", data[i], i);
        }
    }
    dprintf(fd, "Max=%d, Avg=%.2f\n", max, avg);
    close(fd);
}

// Function to generate a file with L positive integers and H hidden negative keys
void generate_and_hide_keys(const char* filename, int L, int H) {
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    srand((unsigned int)time(NULL)); 

    fprintf(file, "%d\n", L);

    int* numbers = malloc(L * sizeof(int));
    if (!numbers) {
        perror("Failed to allocate memory for numbers array");
        fclose(file);
        exit(1);
    }

    // Generate L positive integers
    for (int i = 0; i < L; ++i) {
        numbers[i] = rand() % MAX_POSITIVE_INT + 1;
    }

    // Randomly replace some integers with hidden keys
    for (int i = 0; i < H; ++i) {
        int pos = rand() % L;
        numbers[pos] = MIN_NEGATIVE_INT + rand() % (1 - MIN_NEGATIVE_INT);
    }

    // Write the numbers to the file
    for (int i = 0; i < L; ++i) {
        fprintf(file, "%d\n", numbers[i]);
    }

    free(numbers);
    fclose(file);
}


int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <L> <H> <PN>\n", argv[0]);
        return 1;
    }

    int L = atoi(argv[1]);
    int H = atoi(argv[2]);
    int PN = atoi(argv[3]);

    if (L < 1 || H <= 30 || H >= 60 || H > L) {
        fprintf(stderr, "Invalid input: L must be >= 1 and 30 < H < 60, H must be <= L\n");
        return 1;
    }

    // Generate the file
    generate_and_hide_keys("input.txt", L, H);

    int size;
    int *data = read_data("input.txt", &size);
    int segment_size = size / PN;
    pid_t pids[PN], original_parent_pid;

    original_parent_pid = getpid();
    printf("Parent process %d: Starting program.\n", original_parent_pid);

    // Clear
    // Clear output file or create it if it doesn't exist
    int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }
    close(fd);

    // Fork processes and assign them segments of data
    for (int i = 0; i < PN; ++i) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pids[i] == 0) { // Child process
            int start = i * segment_size;
            int end = (i == PN - 1) ? size : (i + 1) * segment_size;
            process_data_segment(data, start, end, i);
            free(data);
            exit(EXIT_SUCCESS);
        }
    }

    // Parent process waits for child processes to complete
    for (int i = 0; i < PN; ++i) {
        waitpid(pids[i], NULL, 0);
    }

    printf("Parent process %d: Children have completed.\n", original_parent_pid);
    printf("Parent process %d: Terminating.\n", original_parent_pid);

    free(data);
    return 0;
}
