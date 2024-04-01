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
#define MAX_CHILDREN 4
#define MAX_TREE_HEIGHT 3
#define HIDDEN_KEY_LOWER_BOUND -60
#define HIDDEN_KEY_UPPER_BOUND -1


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

void process_data_segment(int *data, int start, int end, int process_id) {
    int max = data[start];
    int count_hidden = 0, sum = 0;
    for (int i = start; i < end; ++i) {
        if (data[i] > max) max = data[i];
        if (data[i] >= HIDDEN_KEY_LOWER_BOUND && data[i] <= HIDDEN_KEY_UPPER_BOUND) count_hidden++;
        sum += data[i];
    }

    float avg = (float)sum / (end - start);

    int fd = open("output-BFS.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    dprintf(fd, "Hi I'm process %d with return arg %d and my parent is %d.\n", process_id, max, getppid());
    for (int i = start; i < end; ++i) {
        if (data[i] >= HIDDEN_KEY_LOWER_BOUND && data[i] <= HIDDEN_KEY_UPPER_BOUND) {
            dprintf(fd, "I found the hidden key %d in position A[%d].\n", data[i], i);
        }
    }
    dprintf(fd, "Max=%d, Avg=%.2f\n", max, avg);
    close(fd);
}

void bfs_process_data(int *data, int size, int current_level, int max_levels, int idx_in_level) {
    int total_processes_in_level = 1 << current_level; // Number of processes at the current level
    int segment_size = size / total_processes_in_level;

    if (current_level >= max_levels) {
        int start = idx_in_level * segment_size;
        int end = start + segment_size;
        process_data_segment(data, start, end, getpid());
        return;
    }

    for (int i = 0; i < MAX_CHILDREN; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            int new_idx_in_level = idx_in_level * MAX_CHILDREN + i;
            bfs_process_data(data, size, current_level + 1, max_levels, new_idx_in_level);
            exit(0);
        } else if (pid > 0) {
            // Parent process
        } else {
            perror("fork");
            exit(EXIT_FAILURE);
        }
    }

    // Parent waits for all children to complete
    for (int i = 0; i < MAX_CHILDREN; ++i) {
        wait(NULL);
    }
}



int main(int argc, char *argv[]) {
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

    int size;
    int *data = read_data("input.txt", &size);

    bfs_process_data(data, size, 0, MAX_TREE_HEIGHT, 0);

    free(data);
    return 0;
}