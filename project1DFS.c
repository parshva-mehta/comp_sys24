#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>

#define MAX_POSITIVE_INT 10000
#define MIN_NEGATIVE_INT -60

int *read_data(const char *filename, int *size);
void process_data_segment(int *data, int start, int end, int child_idx);
void generate_and_hide_keys(const char* filename, int L, int H);

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <L> <H> <PN>\n", argv[0]);
        return 1;
    }

    int L = atoi(argv[1]);
    int H = atoi(argv[2]);
    int PN = atoi(argv[3]);

    if (L < 1 || H < 1 || H > L) {
        fprintf(stderr, "Invalid input: L must be >= 1 and 1 <= H <= L\n");
        return 1;
    }

    generate_and_hide_keys("input.txt", L, H);
    int size;
    int *data = read_data("input.txt", &size);
    int segment_size = size / PN;
    pid_t pids[PN];

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
            exit(EXIT_SUCCESS);
        }
    }

    // Parent process waits for child processes to complete
    for (int i = 0; i < PN; ++i) {
        waitpid(pids[i], NULL, 0);
    }

    free(data);
    return 0;
}

void process_data_segment(int *data, int start, int end, int child_idx) {
    clock_t begin = clock();
    printf("Child %d (PID: %d) started processing data segment from %d to %d.\n", child_idx, getpid(), start, end);

    int max = data[start];
    int sum = 0;
    for (int i = start; i < end; ++i) {
        if (data[i] > max) max = data[i];
        sum += data[i];
    }
    float avg = (end > start) ? (float)sum / (end - start) : 0.0;
    clock_t end_clock = clock();
    double time_spent = (double)(end_clock - begin) / CLOCKS_PER_SEC;

    printf("Child %d (PID: %d) finished processing. Max=%d, Avg=%.2f, Time taken: %f seconds\n", child_idx, getpid(), max, avg, time_spent);
}

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

void generate_and_hide_keys(const char* filename, int L, int H) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    srand((unsigned int)time(NULL));
    fprintf(file, "%d\n", L);
    int* numbers = malloc(L * sizeof(int));
    if (!numbers) {
        perror("Failed to allocate memory for numbers array");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < L; ++i) {
        numbers[i] = rand() % MAX_POSITIVE_INT + 1;
    }

    for (int i = 0; i < H; ++i) {
        int pos = rand() % L;
        numbers[pos] = MIN_NEGATIVE_INT + rand() % (1 - MIN_NEGATIVE_INT);
    }

    for (int i = 0; i < L; ++i) {
        fprintf(file, "%d\n", numbers[i]);
    }

    free(numbers);
    fclose(file);
}
