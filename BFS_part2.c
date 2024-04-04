#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#define MAX_POSITIVE_INT 10000
#define MIN_NEGATIVE_INT -60
#define HIDDEN_KEYS_COUNT 60
#define MAX_CHILDREN 4
#define MAX_TREE_HEIGHT 3
#define HIDDEN_KEY_LOWER_BOUND -60
#define HIDDEN_KEY_UPPER_BOUND -1

// Signal handler for SIGINT in child processes
void sigint_handler(int signum) {
    printf("Received SIGINT. My PID is %d and my parent's PID is %d.\n", getpid(), getppid());
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

void process_data_segment(int *data, int start, int end, int process_id, int write_pipe) {
    printf("Child %d (PID: %d) started processing data segment from %d to %d.\n", process_id, getpid(), start, end); // Log when child starts

    clock_t begin = clock(); // Start the clock to measure processing time

    int max = data[start];
    int sum = 0, count_hidden = 0;
    for (int i = start; i < end; ++i) {
        if (data[i] > max) {
            max = data[i];
        }
        if (data[i] >= HIDDEN_KEY_LOWER_BOUND && data[i] <= HIDDEN_KEY_UPPER_BOUND) {
            count_hidden++;
        }
        sum += data[i];
    }
    float avg = (end > start) ? (float)sum / (end - start) : 0.0;

    clock_t end_clock = clock(); // End the clock

    // Calculate time taken in seconds
    double time_spent = (double)(end_clock - begin) / CLOCKS_PER_SEC;

    for (int i = start; i < end; ++i) {
        if (data[i] >= HIDDEN_KEY_LOWER_BOUND && data[i] <= HIDDEN_KEY_UPPER_BOUND) {
            // Write to pipe when a key is found
            write(write_pipe, &data[i], sizeof(int));
        }
    }

    int fd = open("output-BFSp2.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Writing process data and findings to the file
    dprintf(fd, "Hi I'm process %d with return arg %d and my parent is %d.\n", process_id, max, getppid());
    for (int i = start; i < end; ++i) {
        if (data[i] >= HIDDEN_KEY_LOWER_BOUND && data[i] <= HIDDEN_KEY_UPPER_BOUND) {
            dprintf(fd, "I am process %d and I found the hidden key %d in position A[%d].\n", getpid(), data[i], i);
        }
    }
    dprintf(fd, "Max=%d, Avg=%.2f\n", max, avg);

    // Writing the time taken by the process
    dprintf(fd, "Process %d time taken: %f seconds\n", process_id, time_spent);
    printf("Child %d (PID: %d) finished processing. Max=%d, Avg=%.2f, Time taken: %f seconds\n", process_id, getpid(), max, avg, time_spent); // Log when child ends

    close(fd);
    close(write_pipe); // Close the write end of the pipe
}

// Rule 1: Send SIGCONT signal to child process
void rule_1(int child_pid) {
    kill(child_pid, SIGCONT);
}

// Rule 2: Send SIGKILL signal to child process with the highest number of hidden keys
void rule_2(int *hidden_counts, int num_children) {
    int max_hidden = 0;
    int max_hidden_child = -1;

    // Find child with the highest number of hidden keys
    for (int i = 0; i < num_children; ++i) {
        if (hidden_counts[i] > max_hidden) {
            max_hidden = hidden_counts[i];
            max_hidden_child = i;
        }
    }

    if (max_hidden_child != -1) {
        kill(max_hidden_child, SIGKILL);
    }
}

// Rule 3: Unblock child, run loop, send SIGINT, then SIGQUIT signals to each child
void rule_3(int child_pid) {
    kill(child_pid, SIGCONT);
    // Run loop to delay completion
    for (int i = 0; i < 1000000; ++i) {}
    kill(child_pid, SIGINT);
    sleep(1); // Wait for a bit before sending SIGQUIT
    kill(child_pid, SIGQUIT);
}

void bfs_process_data(int *data, int size, int current_level, int max_levels, int idx_in_level, int write_pipe, int parent_pid, int *hidden_counts, int *num_hidden) {
    int num_processes_at_max_level = 1 << max_levels; // Total number of processes at the max level
    int base_segment_size = size / num_processes_at_max_level;
    int remaining_elements = size % num_processes_at_max_level;

    int start = idx_in_level * base_segment_size + (idx_in_level < remaining_elements ? idx_in_level : remaining_elements);
    int end = start + base_segment_size + (idx_in_level < remaining_elements ? 1 : 0);

    if (current_level == max_levels) {
        if (start < size) { // Ensure the process has data to process
            process_data_segment(data, start, end, getpid(), write_pipe);
            *num_hidden = 0;
        }
        return;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    int num_children = 0;
    pid_t child_pids[MAX_CHILDREN];

    for (int i = 0; i < MAX_CHILDREN; ++i) {
        pid_t pid = fork();
        if (pid == 0) { // Child process
            close(pipefd[0]); // Close read end of the pipe in child
            signal(SIGINT, sigint_handler); // Register SIGINT handler
            bfs_process_data(data, size, current_level + 1, max_levels, idx_in_level * MAX_CHILDREN + i, pipefd[1], getpid(), hidden_counts, num_hidden);
            exit(0);
        } else if (pid > 0) {
            // Parent process
            child_pids[num_children++] = pid;
        } else {
            perror("fork");
            exit(EXIT_FAILURE);
        }
    }

    close(pipefd[1]); // Close write end of the pipe in parent

    // Parent process reads from the pipe
    int key_count = 0, read_key;
    while (key_count < HIDDEN_KEYS_COUNT && read(pipefd[0], &read_key, sizeof(int)) > 0) {
        key_count++;
        if (key_count >= HIDDEN_KEYS_COUNT) {
            printf("Success: Found %d hidden keys.\n", HIDDEN_KEYS_COUNT);
            break;
        }
    }

    // Close the read end of the pipe
    close(pipefd[0]);

    // Wait for all children to complete
    for (int i = 0; i < num_children; ++i) {
        int status;
        waitpid(child_pids[i], &status, 0);

        if (WIFEXITED(status)) {
            // Child process terminated normally
        } else if (WIFSIGNALED(status)) {
            // Child process terminated by signal
            if (WTERMSIG(status) != SIGKILL) {
                printf("Child process %d terminated by signal %d.\n", child_pids[i], WTERMSIG(status));
            }
        }
    }

    // Update parent's hidden nodes count based on child processes
    for (int i = 0; i < num_children; ++i) {
        if (hidden_counts[i] > 0) {
            *num_hidden += hidden_counts[i];
        }
    }

    // Decision making based on rules
    if (*num_hidden > 0) {
        rule_1(getpid());
    } else {
        rule_2(hidden_counts, num_children);
        if (*num_hidden == 0) {
            rule_3(getpid());
        }
    }
}

int main(int argc, char *argv[]) {
    // Clear output file
    FILE* outputFile = fopen("output-BFSp2.txt", "w");
    if (!outputFile) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }
    fclose(outputFile);

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

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    int hidden_counts[MAX_CHILDREN] = {0};
    int num_hidden = 0;

    // Fork the first set of processes
    bfs_process_data(data, size, 0, MAX_TREE_HEIGHT, 0, pipefd[1], getpid(), hidden_counts, &num_hidden);
    
    // Close the write end of the pipe in the parent
    close(pipefd[1]);

    return 0;
}