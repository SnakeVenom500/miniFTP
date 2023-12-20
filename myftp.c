#include "myftp.h"
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/wait.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define MY_PORT_NUMBER 49992
#define BUF_SIZE (PATH_MAX + 6)

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Error: Invalid number of arguments\n");
        exit(1);
    }
    char* address = argv[1];
    char port[6];
    sprintf(port, "%d", MY_PORT_NUMBER);
    int socketfd = createSocketConnection(port, address);
    if (socketfd == -1) {
        exit(1);
    }
    printf("Connection successful.\n");
    startCommandLine(socketfd, address);
}

void startCommandLine(int socketfd, char* address) {
    char input[BUF_SIZE];
    char *command;
    char *arguments;
    char serverResponse[BUF_SIZE];
    int dataConnectFD = -1;

    printf("ftp> ");
    ssize_t n;
    fflush(stdout);
    while ((n = read(0, input, BUF_SIZE)) > 0) {
        input[n-1] = '\0';

        command = strtok(input, " \t\n");
        if (command == NULL) {
            printf("No command entered.\n");
            printf("ftp> ");
            fflush(stdout);
            continue;
        }

        if (strcmp(command, "exit") == 0) {
            if(write(socketfd, "Q\n", 2)== -1) {
                fprintf(stderr, "Write. %s.\n", strerror(errno));
                exit(1);
            }
            printf("Exiting client.\n");
            readServerResponse(socketfd);
            break;
        }
        else if (strcmp(command, "cd") == 0) {
            arguments = strtok(NULL, " \t\n");
            if (arguments == NULL) {
                printf("No path provided for cd command.\n");
            } else {
                printf("Changing directory to '%s'\n", arguments);

                changeClientDirectory(arguments);
            }
        }
        else if (strcmp(command, "rcd") == 0) {
            arguments = strtok(NULL, " \t\n");
            if (arguments == NULL) {
                printf("No path provided for rcd command.\n");
            }
            else {
                printf("Changing server directory to '%s'\n", arguments);
                sprintf(serverResponse, "C%s\n", arguments);
                if(write(socketfd, serverResponse, strlen(serverResponse))== -1) {
                    fprintf(stderr, "Write. %s.\n", strerror(errno));
                    exit(1);
                }

                if (readServerResponse(socketfd) == -2) {
                    break;
                }
            }
        }
        else if (strcmp(command, "ls") == 0) {
            if (listClientContents() == -1) {
                break;
            }
        }
        else if (strcmp(command, "rls") == 0) {
            if ((dataConnectFD = createClientDataConnection(socketfd, address)) == -1) {
                fprintf(stderr, "Error fatal\n");
                exit(1);
            }
            if (rls(socketfd, dataConnectFD) == -1) {
                fprintf(stderr, "Error fatal\n");
                exit(1);
            }
            close(dataConnectFD);
            if (readServerResponse(socketfd) == -2) {
                fprintf(stderr, "Error fatal\n");
                exit(1);
            }
        }
        else if (strcmp(command, "get") == 0) {
            arguments = strtok(NULL, " \t\n");
            if (arguments == NULL) {
                printf("No path provided for get command.\n");
            }
            else {
                printf("Retrieving '%s' from the server.\n", arguments);
                if (getFile_Client(socketfd, arguments, address) == -1) {
                    break;
                }
            }
        }
        else if (strcmp(command, "show") == 0) {
            arguments = strtok(NULL, " \t\n");
            if (arguments == NULL) {
                printf("No path provided for show command.\n");
            }
            else {
                printf("Showing contents from path '%s' on the server. Press space to view more.\n", arguments);
                if (show(socketfd, arguments, address) == -1) {
                    break;
                }
            }

        }
        else if (strcmp(command, "put") == 0) {
            arguments = strtok(NULL, " \t\n");
            if (arguments == NULL) {
                printf("No path provided for put command.\n");
            }
            else {
                printf("Transmitting contents from '%s' to server.\n", arguments);
                if (put(socketfd, arguments, address) == -1) {
                    break;
                }
            }
        }
        else {
            printf("Error: invalid command. Try again.\n");
        }

        printf("ftp> ");
        fflush(stdout);
    }
    if (n == -1) {
        fprintf(stderr, "Error: read failed. %s.\n", strerror(errno));
        exit(errno);
    }
}

int createSocketConnection(char* portNumber, char* address) {
    int socketfd;
    struct addrinfo hints, *actualdata;
    memset(&hints, 0, sizeof(hints));
    int err;

    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;

    err = getaddrinfo(address, portNumber, &hints, &actualdata);
    if (err != 0) {
        fprintf(stderr, "Error: %s, Line %d\n", gai_strerror(err), __LINE__);
        return -1;
    }
    if ((socketfd = socket(actualdata->ai_family, actualdata->ai_socktype, 0)) < 0) {
        fprintf(stderr, "Error: %s, Line %d\n", strerror(errno), __LINE__);
        return -1;
    }
    if (connect(socketfd, actualdata->ai_addr, actualdata->ai_addrlen) < 0) {
        fprintf(stderr, "Error: %s, Line %d\n", strerror(errno), __LINE__);
        return -1;
    }
    return socketfd;
}

int createClientDataConnection(int socketfd, char* address) {
    ssize_t n;
    char serverResponse[BUF_SIZE];
    int dataConnectFD = -1;
    if ((write(socketfd, "D\n", 2)) < 0) {
        fprintf(stderr, "Write failed. %s.\n", strerror(errno));
        return -1;
    }
    if ((n = read(socketfd, serverResponse, 1)) < 0) {
        fprintf(stderr, "Read failed. %s.\n", strerror(errno));
        return -1;
    }
    serverResponse[n] = '\0';
    if (strcmp(serverResponse, "A") == 0) {
        if ((n = read(socketfd, serverResponse, BUF_SIZE)) < 0) {
            fprintf(stderr, "Read failed. %s.\n", strerror(errno));
            return -1;
        }
        serverResponse[n-1] = '\0';
        if ((dataConnectFD = createSocketConnection(serverResponse, address)) == -1) {
            return -1;
        }
    }
    else if (strcmp(serverResponse, "E") == 0) {
        if ((n = read(socketfd, serverResponse, BUF_SIZE)) < 0) {
            fprintf(stderr, "Read failed. %s.\n", strerror(errno));
            return -1;
        }
        fprintf(stderr, "From server: %s", serverResponse);
        return -1;
    }
    return dataConnectFD;
}

int listClientContents() {
    int pipeFD[2];
    if ((pipe(pipeFD)) == -1) {
        fprintf(stderr, "Pipe failed. %s.\n", strerror(errno));
        return -1;
    }
    pid_t pid = fork();
    if (pid == -1) {
        fprintf(stderr, "Fork failed. %s\n", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        close(pipeFD[0]);
        dup2(pipeFD[1], STDOUT_FILENO);
        close(pipeFD[1]);
        execlp("ls", "ls", "-l", "-a", NULL);
        fprintf(stderr, "Exec failed. %s\n", strerror(errno));
        exit(1);
    } else {
        pid_t pid2 = fork();
        if (pid2 == -1) {
            fprintf(stderr, "Fork failed. %s\n", strerror(errno));
            return -1;
        }
        if (pid2 == 0) {
            close(pipeFD[1]);
            dup2(pipeFD[0], STDIN_FILENO);
            close(pipeFD[0]);
            execlp("more", "more", "-20", NULL);
            fprintf(stderr, "Exec failed. %s\n", strerror(errno));
            exit(1);
        }
        close(pipeFD[1]); close(pipeFD[0]);
        while (1) {
            pid = waitpid(-1, NULL, 0);
            if (pid == -1) {
                if (errno == ECHILD) {
                    break;
                }
                else {
                    fprintf(stderr, "waitpid failed. %s.\n", strerror(errno));
                    return -1;
                }
            }
        }
    }
    return 0;
}

int changeClientDirectory(char* path) {
    if (access(path, X_OK) == -1) {
        fprintf(stderr, "Error: path not accessible. %s.\n", strerror(errno));
        return -1;
    }
    struct stat input, *s = &input;
    if (lstat(path, s) == -1) {
        fprintf(stderr, "lstat failed. %s.\n", strerror(errno));
        return -1;
    }
    if (!S_ISDIR(s->st_mode)) {
        fprintf(stderr, "Error: path is not a directory.\n");
        return -1;
    }
    if (chdir(path) == -1) {
        fprintf(stderr, "Error: chdir failed. %s.\n", strerror(errno));
        return -1;
    }
    return 0;
}

int readServerResponse(int socketfd) {
    char serverResponse[BUF_SIZE];
    ssize_t n;
    if ((n = read(socketfd, serverResponse, 1)) == 1) {
        serverResponse[n] = '\0';
        if (strcmp(serverResponse, "A") == 0) {
            read(socketfd, serverResponse, 1);
        }
        else if (strcmp(serverResponse, "E") == 0) {
            if ((n = read(socketfd, serverResponse, BUF_SIZE)) <= 0) {
                fprintf(stderr, "Read failed. %s.\n", strerror(errno));
                return -2;
            }
            serverResponse[n] = '\0';
            if (strncmp(serverResponse, "Fatal", 5) == 0) {
                return -2;
            }
            fprintf(stderr, "From server: %s", serverResponse);
            return -1;
        }
    }
    else {
        fprintf(stderr, "Read failed. %s.\n", strerror(errno));
        return -2;
    }
    return 0;
}

int rls(int socketfd, int dataConnectFD) {
    if(write(socketfd, "L\n", 2)== -1) {
        fprintf(stderr, "Write. %s.\n", strerror(errno));
        exit(1);
    }
    pid_t pid = fork();
    if (pid == -1) {
        fprintf(stderr, "Fork failed. %s\n", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        dup2(dataConnectFD, STDIN_FILENO);
        execlp("more", "more", "-20", NULL);
        fprintf(stderr, "Exec failed. %s\n", strerror(errno));
        exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        if (exit_status != 0) {
            fprintf(stderr, "Exec failed.\n");
            return -1;
        }
    }
    return 0;
}

int getFile_Client(int socketfd, char* path, char* address) {
    char pathCopy[strlen(path)+1];
    strcpy(pathCopy, path);
    char* fileName = basename(pathCopy);
    if (access(fileName, F_OK) == 0) {
        struct stat input, *s = &input;
        if (lstat(fileName, s) == -1) {
            fprintf(stderr, "lstat failed. %s.\n", strerror(errno));
            return -1;
        }
        if (!S_ISREG(s->st_mode)) {
            fprintf(stderr, "Error: not a regular file.\n");
            return -1;
        }
        if (access(fileName, W_OK) == -1) {
            fprintf(stderr, "Error: access. %s.\n", strerror(errno));
            return -1;
        }
    }
    int dataConnectFD;
    if ((dataConnectFD = createClientDataConnection(socketfd, address)) == -1) {
        fprintf(stderr, "Error fatal\n");
        exit(1);
    }

    char command[BUF_SIZE];
    sprintf(command, "G%s\n", path);
    if (write(socketfd, command, strlen(command)) < 0) {
        fprintf(stderr, "Error: write. %s.\n", strerror(errno));
        close(dataConnectFD);
        return -1;
    }
    if (readServerResponse(socketfd) < 0) {
        close(dataConnectFD);
        return 0;
    }

    int fd = open(fileName, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    char buffer[BUF_SIZE];
    ssize_t n;
    while((n = read(dataConnectFD, buffer, BUF_SIZE)) > 0) {
        if (write(fd, buffer, n) < 0) {
            fprintf(stderr, "Error: write. %s.\n", strerror(errno));
            close(fd);
            close(dataConnectFD);
            return -1;
        }
    }
    if (n < 0) {
        fprintf(stderr, "Error: read. %s.\n", strerror(errno));
        close(fd);
        close(dataConnectFD);
        return -1;
    }
    close(fd);
    close(dataConnectFD);
    return 0;
}

int show(int socketfd, char* path, char* address) {
    int dataConnectFD;
    if ((dataConnectFD = createClientDataConnection(socketfd, address)) == -1) {
        fprintf(stderr, "Error fatal\n");
        exit(1);
    }

    char command[BUF_SIZE];
    sprintf(command, "G%s\n", path);
    if (write(socketfd, command, strlen(command)) < 0) {
        fprintf(stderr, "Error: write. %s.\n", strerror(errno));
        close(dataConnectFD);
        return -1;
    }
    if (readServerResponse(socketfd) < 0) {
        close(dataConnectFD);
        return 0;
    }

    pid_t pid = fork();
    if (pid == -1) {
        fprintf(stderr, "Fork failed. %s\n", strerror(errno));
        close(dataConnectFD);
        return -1;
    }
    if (pid == 0) {
        dup2(dataConnectFD, STDIN_FILENO);
        execlp("more", "more", "-20", NULL);
        fprintf(stderr, "Exec failed. %s\n", strerror(errno));
        exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        if (exit_status != 0) {
            fprintf(stderr, "Exec failed.\n");
            close(dataConnectFD);
            return -1;
        }
    }
    close(dataConnectFD);
    return 0;
}

int put(int socketfd, char* path, char* address) {
    if (access(path, F_OK) == 0) {
        struct stat input, *s = &input;
        if (lstat(path, s) == -1) {
            fprintf(stderr, "lstat failed. %s.\n", strerror(errno));
            return -1;
        }
        if (!S_ISREG(s->st_mode)) {
            fprintf(stderr, "Error: not a regular file.\n");
            return 0;
        }
        if (access(path, R_OK) == -1) {
            fprintf(stderr, "Error: access. %s.\n", strerror(errno));
            return 0;
        }
    }
    else {
        int err = errno;
        fprintf(stderr, "Error: access. %s.\n", strerror(err));
        return 0;
    }

    int dataConnectFD;
    if ((dataConnectFD = createClientDataConnection(socketfd, address)) == -1) {
        fprintf(stderr, "Error fatal\n");
        exit(1);
    }

    int fd = open(path, O_RDONLY);
    char buffer[BUF_SIZE];
    ssize_t n;
    while((n = read(fd, buffer, BUF_SIZE)) > 0) {
        if (write(dataConnectFD, buffer, n) < 0) {
            int err = errno;
            fprintf(stderr, "Error: write. %s.\n", strerror(err));
            close(fd);
            close(dataConnectFD);
            return -1;
        }
    }
    if (n < 0) {
        int err = errno;
        fprintf(stderr, "Error: read. %s.\n", strerror(err));
        close(fd);
        close(dataConnectFD);
        return -1;
    }
    char command[BUF_SIZE];
    sprintf(command, "P%s\n", path);
    if(write(socketfd, command, strlen(command))== -1) {
        fprintf(stderr, "Write. %s.\n", strerror(errno));
        exit(1);
    }
    close(fd);
    close(dataConnectFD);
    readServerResponse(socketfd);
    return 0;
}
