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

int *read_data(const char *filename, int *size);
void process_data_segment(int *data, int start, int end, int child_idx, int write_pipe, int read_pipe);
void generate_and_hide_keys(const char* filename, int L, int H);
void pause_child();
void handle_sigcont(int signum);

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

    int fd_clear = open("output-DFSp2.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_clear == -1) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }
    close(fd_clear);

    int size;
    int *data = read_data("input.txt", &size);
    int segment_size = size / PN;
    pid_t pids[PN];

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < PN; ++i) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pids[i] == 0) { // Child process
            close(pipefd[1]); // Close the write end of the pipe in the child
            int start = i * segment_size;
            int end = (i == PN - 1) ? size : (i + 1) * segment_size;
            process_data_segment(data, start, end, i, pipefd[0], pipefd[1]);
            exit(EXIT_SUCCESS);
        }
    }

    // Parent process waits for all children to complete
    for (int i = 0; i < PN; ++i) {
        int status;
        pid_t terminated_pid = wait(&status);
        if (WIFEXITED(status)) {
            printf("Child process %d terminated with exit status %d\n", terminated_pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Child process %d terminated by signal %d\n", terminated_pid, WTERMSIG(status));
        }

        // Check if rule 1 was invoked
        if (WIFCONTINUED(status)) {
            printf("Rule 1 invoked for child process %d\n", terminated_pid);
        }

        // Check if rule 2 was invoked
        if (WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL) {
            printf("Rule 2 invoked for child process %d\n", terminated_pid);
        }
        
        // Send SIGCONT to unblock the child
        kill(terminated_pid, SIGCONT);
        // Pause for a while before sending SIGINT
        sleep(1);
        // Send SIGINT to each child
        kill(terminated_pid, SIGINT);
        // Pause for a while before sending SIGQUIT
        sleep(1);
        // Send SIGQUIT to each child
        kill(terminated_pid, SIGQUIT);
    }

    free(data);
    return 0;
}

volatile sig_atomic_t sigint_received = 0;

// Signal handler for SIGINT
void sigint_handler(int signum) {
    sigint_received = 1;
}

void process_data_segment(int *data, int start, int end, int child_idx, int read_pipe, int write_pipe) {
    signal(SIGTSTP, pause_child);
    signal(SIGINT, sigint_handler); // Register SIGINT handler

    int max = data[start];
    int sum = 0, count_hidden = 0;
    for (int i = start; i < end; ++i) {
        if (data[i] > max) max = data[i];
        if (data[i] >= MIN_NEGATIVE_INT && data[i] <= -1) {
            count_hidden++;
        }
        sum += data[i];
    }
    float avg = (end > start) ? (float)sum / (end - start) : 0.0;

    // Write data to the output file
    int fd = open("output-DFSp2.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Error opening output file");
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

    // Write count_hidden to the parent
    write(write_pipe, &count_hidden, sizeof(int));

    raise(SIGTSTP); // Pause the child

    // Child resumed by parent
    signal(SIGCONT, handle_sigcont);

    // Perform decision rules
    // RULE 1: If localMAX(child) > localMAX(parent) or if localMAX(child) > localMAXs of all sibling, continue child
    if (max > data[0] || max > data[child_idx + 1]) {
        kill(getpid(), SIGCONT);
    }
    // RULE 2: If Rule 1 does not hold and if a child has the highest number of hidden numbers compared to its siblings, then terminate the child
    else if (count_hidden > data[0] && count_hidden > data[child_idx + 1]) {
        kill(getpid(), SIGKILL);
    }
    // RULE 3: If Rule 1 and Rule 2 do not hold, delay child termination and print pid and ppid upon receiving SIGINT, then terminate upon receiving SIGQUIT
    else {
        printf("Child process %d executing Rule 3\n", getpid());
        while (!sigint_received) {
            // Run some long loop iteration or sleep
        }
        printf("Received SIGINT. Child pid: %d, Parent pid: %d\n", getpid(), getppid());
        raise(SIGQUIT); // Terminate child upon receiving SIGQUIT
    }

    // After decision rules, child executes its code
    // Optional: Sleep to display subtree
    sleep(1);

    // Child exits
    exit(EXIT_SUCCESS);
}

void pause_child() {
    // No action needed, just pause
}

void handle_sigcont(int signum) {
    // No action needed, just resume
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