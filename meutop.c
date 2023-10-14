#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <ctype.h>
#include <signal.h>
#include <pthread.h>

#define MAX_PROCESSES 20
#define SLEEP_TIME 1

struct process {
  int pid;
  char user[64];
  char proc_name[64];
  char state;
};

struct process processes[MAX_PROCESSES];

void processes_inicialization(int n){
  for (int i = 0; i < n; i++) {
    processes[i].pid = -1;
    strcpy(processes[i].user, "");
    strcpy(processes[i].proc_name,"");
    processes[i].state = 0;
  }
}

FILE* open_file(char* file){
  FILE *fp = fopen(file, "r");
  if (fp == NULL) {
    perror("open file");
    exit(EXIT_FAILURE);
  }
  return fp;
}

DIR *open_proc_dir() {
    DIR *dir = opendir("/proc");
    if (dir == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }
    return dir;
}

void close_proc_dir(DIR *dir) {
    if (closedir(dir) != 0) {
        perror("closedir");
        exit(EXIT_FAILURE);
    }
}

void print_top() {
    printf("PID   |   User   |   PROCNAME   |   Estado\n");
    printf("------|----------|--------------|---------\n");
}

void read_process_status(const char *proc_dir, int i) {
    char statusPath[256];
    snprintf(statusPath, sizeof(statusPath), "%s/status", proc_dir);
    FILE *statusFile;
    statusFile = open_file(statusPath);

    char line[256];

    while (fgets(line, sizeof(line), statusFile)) {
        if (sscanf(line, "Name: %s", processes[i].proc_name) == 1) {
            while (fgets(line, sizeof(line), statusFile)) {
                if (sscanf(line, "State: %c", &processes[i].state) == 1) {
                    break;
                }
            }
            break;
        }
    }
    fclose(statusFile);
}

void getProcesses() {
    DIR *dir = open_proc_dir();
    int i = 0;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && i < MAX_PROCESSES) {
        if (isdigit(entry->d_name[0])) {
            char path[512];
            snprintf(path, sizeof(path), "/proc/%s", entry->d_name);

            struct stat info;
            if (stat(path, &info) == 0) {
                uid_t uid = info.st_uid;
                struct passwd *pwd = getpwuid(uid);
                if (pwd != NULL) {
                    strncpy(processes[i].user, pwd->pw_name, sizeof(processes[i].user));
                    processes[i].pid = atoi(entry->d_name);
                    read_process_status(path, i);
                    i++;
                }
            }
        }
    }

    close_proc_dir(dir);
}

void print_process(struct process p){
    printf("%-5d | %-8s | %-12s | %-2c\n", p.pid, p.user, p.proc_name, p.state);
}

pthread_mutex_t screen = PTHREAD_MUTEX_INITIALIZER;

void print_processes() {
    processes_inicialization(MAX_PROCESSES);

    pthread_mutex_lock( &screen );
    printf("\033[2J\033[H"); // Clears the screen from the current line (2J) and moves the cursor to the top (H).

    print_top();
    getProcesses();
  
    for(int i = 0; i < MAX_PROCESSES; i++){
        print_process(processes[i]);
    }

    printf("Pressione <enter> para entra no modo de execução de comandos");
    fflush(stdout);
    pthread_mutex_unlock( &screen );
}

void send_signal(){
    char input[256];
    char aux[10];
    int pid, sig;
    fgets(aux, 10, stdin);
    
    pthread_mutex_lock( &screen );
    printf("insira o PID e o sinal > ");
    if (fgets(input, sizeof(input), stdin) != NULL) {
        if (sscanf(input, "%d %d", &pid, &sig) == 2) {
            if (kill(pid, sig) == 0) {
                printf("Signal %d sent to process %d successfully.\n", sig, pid);
            } else {
                perror("Signal delivery failed");
            }
        } else {
            printf("Invalid input or missing values. No signal sent.\n");
        }
    } else {
        printf("Failed to read input.\n");
    }
    sleep(SLEEP_TIME);
    pthread_mutex_unlock( &screen );
}

void* continuos_print(){
    while (1) {
        print_processes();
        sleep(SLEEP_TIME);
    }
    return NULL;
}

void* continuous_signal(){
    while (1) {
        send_signal();
    }
    
    return NULL;
}

int main() {
    pthread_t process_print, signal_send;
    
    if (pthread_create(&process_print, NULL, continuos_print, NULL) != 0){
        printf("Failed to create print thread\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&signal_send, NULL, continuous_signal, NULL) != 0){
        printf("Failed to create signal thread\n");
        exit(EXIT_FAILURE);
    }
    
    pthread_join(process_print, NULL);
    pthread_join(signal_send, NULL);

    return EXIT_SUCCESS;
}