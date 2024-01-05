/**
 * Joey Pandina
 * Systems Programming (CS 360) Final Project, Fall 2023
 *
 * This source code is an implementation of a server that accepts connections and communicates with clients.
 */

#include "myftp.h"
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define BACKLOG 4 //How many client connections are allowed at once
#define MY_PORT_NUMBER 49992 //The port number that the clients will connect to
#define BUF_SIZE (PATH_MAX + 6) //A general buffer size for input from clients, and other general uses.

/**
 * The main function:
 * 1. Establishes a socket called 'listenfd' to listen for client connections.
 * 2. Runs a loop that continuously is listening for client connections.
 * 3. After a client connects, it forks a child process for that client and reads input from the client.
 */
int main() {
    //Establish the 'listenfd' socket to listen for client connections:
    int listenfd;
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    }
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(MY_PORT_NUMBER);
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listenfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    }
    if (listen(listenfd, BACKLOG) == -1) {
        fprintf(stderr, "Error: listen. %s", strerror(errno));
        exit(1);
    }

    //Run a loop that continuously listens for client connections
    int connectfd;
    unsigned int length = sizeof(struct sockaddr_in);
    struct sockaddr_in clientAddr;
    char hostName[NI_MAXHOST];
    int hostEntry;
    while(1){
        if ((connectfd = accept(listenfd, (struct sockaddr *) &clientAddr, &length)) < 0) {
            fprintf(stderr, "Error: %s\n", strerror(errno));
            exit(1);
        }

        //After a client connects, fork a child process and read input from the client.
        if(fork()){
            close(connectfd);
            continue;
        }
        hostEntry = getnameinfo((struct sockaddr *) &clientAddr, sizeof(clientAddr), hostName, sizeof(hostName), NULL, 0, NI_NUMERICSERV);
        if (hostEntry != 0) {
            fprintf(stderr, "Error: %s\n", gai_strerror(hostEntry));
            exit(1);
        }
        int childPID = getpid();
        printf("Child %d: a connection from host %s has been accepted\n", childPID, hostName);

        int dataListenFD = -1;
        int dataConnectFD = -1;
        while(1) {
            char buffer[BUF_SIZE];
            char response[BUF_SIZE];

            if ((read(connectfd, buffer, 1)) <= 0) {
                fprintf(stderr, "Read failed. Line %d. Error: %s\n", __LINE__ , strerror(errno));
                sprintf(response, "EFatal: %s\n", strerror(errno));
                if(write(connectfd, response, strlen(response))== -1) {
                    fprintf(stderr, "Write. %s.\n", strerror(errno));
                    exit(1);
                }
                exit(1);
            }

            if (strncmp(buffer, "D", 1) == 0) {
                if ((read(connectfd, buffer, 1)) < 0) {
                    fprintf(stderr, "Read failed. Line %d. Error: %s\n", __LINE__ , strerror(errno));
                    sprintf(response, "EFatal: %s\n", strerror(errno));
                    if(write(connectfd, response, strlen(response))== -1) {
                        fprintf(stderr, "Write. %s.\n", strerror(errno));
                        exit(1);
                    }
                    printf("Error fatal. Exiting session with child %d...\n", childPID);
                    exit(1);
                }
                if ((dataListenFD = createServerDataConnection(connectfd)) == -1) {
                    fprintf(stderr, "Data connection failed.\n");
                    printf("Error fatal. Exiting session with child %d...\n", childPID);
                    exit(1);
                }
                struct sockaddr_in address;
                memset(&address, 0, sizeof(address));
                unsigned int addrLength = sizeof(address);
                if ((getsockname(dataListenFD, (struct sockaddr *) &address, &addrLength)) == -1) {
                    int err = errno;
                    fprintf(stderr, "getsockname failed. Error: %s\n", strerror(err));
                    sprintf(response, "EFatal: %s\n", strerror(err));
                    if(write(connectfd, response, strlen(response))== -1) {
                        fprintf(stderr, "Write. %s.\n", strerror(errno));
                        exit(1);
                    }
                    close(dataListenFD);
                    printf("Error fatal. Exiting session with child %d...\n", childPID);
                    exit(1);
                }
                int port = ntohs(address.sin_port);
                sprintf(response, "A%d\n", port);
                if(write(connectfd, response, strlen(response))== -1) {
                    fprintf(stderr, "Write. %s.\n", strerror(errno));
                    exit(1);
                }
                if (listen(dataListenFD, 1) == -1){
                    fprintf(stderr, "Listen. %s\n", strerror(errno));
                }
                if ((dataConnectFD = accept(dataListenFD, (struct sockaddr *) &address, &addrLength)) < 0) {
                    int err = errno;
                    fprintf(stderr, "Failed to connect to client. Error: %s\n", strerror(err));
                    close(dataListenFD);
                    printf("Error fatal. Exiting session with child %d...\n", childPID);
                    exit(1);
                }
            }
            else if (strncmp(buffer, "C", 1) == 0) {
                ssize_t n;
                if ((n = read(connectfd, buffer, BUF_SIZE)) <= 0) {
                    fprintf(stderr, "Read failed. Line %d. Error: %s\n", __LINE__ , strerror(errno));
                    sprintf(response, "EFatal: %s\n", strerror(errno));
                    if(write(connectfd, response, strlen(response))== -1) {
                        fprintf(stderr, "Write. %s.\n", strerror(errno));
                        exit(1);
                    }
                    printf("Error fatal. Exiting session with child %d...\n", childPID);
                    exit(1);
                }
                buffer[n-1] = '\0';
                printf("Changing directory to '%s'...\n", buffer);
                int returnNum;
                if ((returnNum = changeServerDirectory(buffer, connectfd)) == -1) {
                    continue;
                }
                if (returnNum == -2) {
                    printf("Error fatal. Exiting session with child %d...\n", childPID);
                    exit(1);
                }
                sprintf(response, "A\n");
                if(write(connectfd, response, strlen(response))== -1) {
                    fprintf(stderr, "Write. %s.\n", strerror(errno));
                    exit(1);
                }
            }
            else if (strncmp(buffer, "L", 1) == 0) {
                printf("Listing directory contents...\n");
                if (dataConnectFD < 0) {
                    fprintf(stderr, "Error: Data connection has not been made.\n");
                    sprintf(response, "EData connection has not been made.\n");
                    if(write(connectfd, response, strlen(response))== -1) {
                        fprintf(stderr, "Write. %s.\n", strerror(errno));
                        exit(1);
                    }
                    printf("Error fatal. Exiting session with child %d...\n", childPID);
                    exit(1);
                }
                if (listServerContents(connectfd, dataConnectFD) == -2) {
                    printf("Error fatal. Exiting session with child %d...\n", childPID);
                    exit(1);
                }
                close(dataConnectFD);
                dataConnectFD = -1;

                sprintf(response, "A\n");
                if(write(connectfd, response, strlen(response))== -1) {
                    fprintf(stderr, "Write. %s.\n", strerror(errno));
                    exit(1);
                }
            }
            else if (strncmp(buffer, "G", 1) == 0) {
                printf("Getting file...\n");
                if (dataConnectFD < 0) {
                    fprintf(stderr, "Error: Data connection has not been made.\n");
                    sprintf(response, "EData connection has not been made.\n");
                    if(write(connectfd, response, strlen(response))== -1) {
                        fprintf(stderr, "Write. %s.\n", strerror(errno));
                        exit(1);
                    }
                    printf("Error fatal. Exiting session with child %d...\n", childPID);
                    exit(1);
                }
                int returnNum;
                if ((returnNum = getFile_Server(connectfd, dataConnectFD)) == -1) {
                    close(dataConnectFD);
                    dataConnectFD = -1;
                    continue;
                }
                if (returnNum == -2) {
                    printf("Error fatal. Exiting session with child %d...\n", childPID);
                    exit(1);
                }
                close(dataConnectFD);
                dataConnectFD = -1;
                sprintf(response, "A\n");
                if(write(connectfd, response, strlen(response))== -1) {
                    fprintf(stderr, "Write. %s.\n", strerror(errno));
                    exit(1);
                }
            }
            else if(strncmp(buffer, "P", 1) == 0) {
                printf("Putting file...\n");
                if (dataConnectFD < 0) {
                    fprintf(stderr, "Error: Data connection has not been made.\n");
                    sprintf(response, "EData connection has not been made.\n");
                    if(write(connectfd, response, strlen(response))== -1) {
                        fprintf(stderr, "Write. %s.\n", strerror(errno));
                        exit(1);
                    }
                    printf("Error fatal. Exiting session with child %d...\n", childPID);
                    exit(1);
                }
                int returnNum;
                if ((returnNum = putFile_Server(connectfd, dataConnectFD)) == -1) {
                    close(dataConnectFD);
                    dataConnectFD = -1;
                    continue;
                }
                if (returnNum == -2) {
                    printf("Error fatal. Exiting session with child %d...\n", childPID);
                    exit(1);
                }
                close(dataConnectFD);
                dataConnectFD = -1;
            }
            else if (strncmp(buffer, "Q", 1) == 0) {
                printf("Exiting session with child %d...\n", childPID);
                sprintf(response, "A\n");
                if(write(connectfd, response, strlen(response))== -1) {
                    fprintf(stderr, "Write. %s.\n", strerror(errno));
                    exit(1);
                }
                break;
            }

        }
        exit(0);
    }

}

/**
 * This function establishes another socket connection with the client. This socket connection is called the data connection,
 * which is used to transfer file data between the client and server. The data connection is distinct from the control connection.
 * @param connectfd
 * - The control connection socket that allows the server to write error messages to the client.
 * @return
 * - An integer representing the file descriptor for the newly created data connection socket.
 * - OR it returns -1 for a recoverable error
 */
int createServerDataConnection(int connectfd) {
    int socketfd;
    char response[BUF_SIZE];
    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        sprintf(response, "EFatal: %s\n", strerror(errno));
        if(write(connectfd, response, strlen(response))== -1) {
            fprintf(stderr, "Write. %s.\n", strerror(errno));
            exit(1);
        }
        return -1;
    }
    setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(0);
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(socketfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        sprintf(response, "EFatal: %s\n", strerror(errno));
        if(write(connectfd, response, strlen(response))== -1) {
            fprintf(stderr, "Write. %s.\n", strerror(errno));
            exit(1);
        }
        close(socketfd);
        return -1;
    }
    return socketfd;
}

/**
 * This function changes the directory of the server to the indicated path from the client.
 * @param path
 * - An input from the client, indicating the directory to move into.
 * @param connectfd
 * - The control connection in order to send an error message to the client, if needed.
 * @return
 * - Returns 0 for success, -1 for a recoverable error, or -2 for a nonrecoverable fatal error.
 */
int changeServerDirectory(char* path, int connectfd) {
    char response[BUF_SIZE];
    if (access(path, X_OK) == -1) {
        fprintf(stderr, "Error: path not accessible. %s.\n", strerror(errno));
        sprintf(response, "E%s\n", strerror(errno));
        if(write(connectfd, response, strlen(response))== -1) {
            fprintf(stderr, "Write. %s.\n", strerror(errno));
            exit(1);
        }
        return -1;
    }
    struct stat input, *s = &input;
    if (lstat(path, s) == -1) {
        fprintf(stderr, "lstat failed. %s.\n", strerror(errno));
        sprintf(response, "EFatal: %s\n", strerror(errno));
        if(write(connectfd, response, strlen(response))== -1) {
            fprintf(stderr, "Write. %s.\n", strerror(errno));
            exit(1);
        }
        return -2;
    }
    if (!S_ISDIR(s->st_mode)) {
        fprintf(stderr, "Error: path is not a directory.\n");
        sprintf(response, "EPath is not a directory.\n");
        if(write(connectfd, response, strlen(response))== -1) {
            fprintf(stderr, "Write. %s.\n", strerror(errno));
            exit(1);
        }
        return -1;
    }
    if (chdir(path) == -1) {
        fprintf(stderr, "Error: chdir failed. %s.\n", strerror(errno));
        sprintf(response, "EFatal: %s\n", strerror(errno));
        if (write(connectfd, response, strlen(response))== -1) {
            fprintf(stderr, "Write. %s.\n", strerror(errno));
            exit(1);
        }
        return -2;
    }
    return 0;
}

/**
 * Lists the server contents using ls and sends output to the client over the data connection:
 * 1. Forks a child process in order to eventually execlp ls.
 * 2. Within the child process:
 *      - Uses dup2 to redirect STDOUT to the data connection.
 *      - Uses execlp to execute ls, which sends the output to the client over the data connection.
 *
 * @param connectfd
 * - The control connection socket. Used to send messages to the client.
 * @param dataConnectFD
 * - The data connection socket. The output of ls is written to the data connection (by using dup2 to redirect STDOUT).
 * @return
 * - Returns 0 for success, or -2 for a nonrecoverable fatal error
 */
int listServerContents(int connectfd, int dataConnectFD) {
    char response[BUF_SIZE];

    //Fork a child process in order to eventually execlp ls.
    pid_t pid = fork();
    if (pid == -1) {
        int err = errno;
        fprintf(stderr, "Fork failed. %s\n", strerror(err));
        sprintf(response, "EFatal: %s\n", strerror(err));
        if (write(connectfd, response, strlen(response))== -1) {
            fprintf(stderr, "Write. %s.\n", strerror(errno));
            exit(1);
        }
        return -2;
    }

    /*  Within the child process:
          - Use dup2 to redirect STDOUT to the data connection.
          - Use execlp to execute ls, which sends the output to the client over the data connection. */
    if (pid == 0) {
        dup2(dataConnectFD, STDOUT_FILENO);
        execlp("ls", "ls", "-l", "-a", NULL);
        int err = errno;
        fprintf(stderr, "Exec failed. %s\n", strerror(err));
        exit(err);
    } else {
        int status;
        pid = waitpid(-1, &status, 0);
        if (pid == -1) {
            int err = errno;
            fprintf(stderr, "waitpid failed. %s.\n", strerror(err));
            sprintf(response, "EFatal: %s\n", strerror(err));
            if(write(connectfd, response, strlen(response)) == -1) {
                fprintf(stderr, "Write. %s.\n", strerror(errno));
                exit(1);
            }
            return -2;
        }
        else {
            if (WIFEXITED(status)) {
                int exit_status = WEXITSTATUS(status);
                if (exit_status != 0) {
                    sprintf(response, "EFatal: %s\n", strerror(exit_status));
                    if(write(connectfd, response, strlen(response)) == -1) {
                        fprintf(stderr, "Write. %s.\n", strerror(errno));
                        exit(1);
                    }
                    return -2;
                }
            }
            }
        }
    return 0;
}

/**
 * Sends a local file's contents to the client:
 * 1. Reads a path from the client using the control connection, 'connectfd', and checks to make sure that it points to
 *    a readable regular file.
 * 2. Reads from the file using open() and read() and sends the contents to the client over the data connection.
 *
 * @param connectfd
 * - The control connection used to send and receive messages with the client
 * @param dataConnectFD
 * - The data connection used to send the file contents to the client.
 * @return
 * - Returns 0 for success,
 */
int getFile_Server(int connectfd, int dataConnectFD) {
    //Read a path from the client
    char path[BUF_SIZE];
    char response[BUF_SIZE];
    ssize_t n;
    if ((n = read(connectfd, path, BUF_SIZE)) <= 0) {
        int err = errno;
        fprintf(stderr, "Read failed. Line %d. Error: %s\n", __LINE__ , strerror(err));
        sprintf(response, "EFatal: %s\n", strerror(err));
        if (write(connectfd, response, strlen(response)) == -1) {
            fprintf(stderr, "Write. %s.\n", strerror(errno));
            exit(1);
        }
        return -2;
    }
    path[n-1] = '\0';

    //Check to make sure that the path points to a readable regular file.
    if (access(path, F_OK) == 0) {
        struct stat input, *s = &input;
        if (lstat(path, s) == -1) {
            int err = errno;
            fprintf(stderr, "lstat failed. %s.\n", strerror(err));
            sprintf(response, "EFatal: %s\n", strerror(err));
            if(write(connectfd, response, strlen(response)) == -1) {
                fprintf(stderr, "Write. %s.\n", strerror(errno));
                exit(1);
            }
            return -2;
        }
        if (!S_ISREG(s->st_mode)) {
            fprintf(stderr, "Error: not a regular file.\n");
            sprintf(response, "EFile not regular\n");
            if (write(connectfd, response, strlen(response)) == -1) {
                fprintf(stderr, "Write. %s.\n", strerror(errno));
                exit(1);
            }
            return -1;
        }
        if (access(path, R_OK) == -1) {
            int err = errno;
            fprintf(stderr, "Error: access. %s.\n", strerror(err));
            sprintf(response, "E%s\n", strerror(err));
            if (write(connectfd, response, strlen(response)) == -1) {
                fprintf(stderr, "Write. %s.\n", strerror(errno));
                exit(1);
            }
            return -1;
        }
    }
    else {
        int err = errno;
        fprintf(stderr, "Error: access. %s.\n", strerror(err));
        sprintf(response, "E%s\n", strerror(err));
        if(write(connectfd, response, strlen(response)) == -1) {
            fprintf(stderr, "Write. %s.\n", strerror(errno));
            exit(1);
        }
        return -1;
    }

    //Read from the file using open() and read() and send the content to the client over the data connection.
    int fd = open(path, O_RDONLY);
    char buffer[BUF_SIZE];
    while((n = read(fd, buffer, BUF_SIZE)) > 0) {
        if (write(dataConnectFD, buffer, n) < 0) {
            int err = errno;
            fprintf(stderr, "Error: write. %s.\n", strerror(err));
            sprintf(response, "EFatal: %s\n", strerror(err));
            write(connectfd, response, strlen(response));
            close(fd);
            return -1;
        }
    }
    if (n < 0) {
        int err = errno;
        fprintf(stderr, "Error: read. %s.\n", strerror(err));
        sprintf(response, "EFatal: %s\n", strerror(err));
        if(write(connectfd, response, strlen(response)) == -1) {
            fprintf(stderr, "Write. %s.\n", strerror(errno));
            exit(1);
        }
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

/**
 * Writes a file to the local directory:
 * 1. Reads in a path from the client using the control connection. The function will use the last component of the path
 *    for the file name. The path preceding the file name is ignored.
 * 2. Parse the path to retrieve the file name.
 * 3. Make sure that the file does not already exist.
 * 4. Creates a new file and writes to it the contents in the data connection.
 *
 * @param connectfd
 * - The control connection used to send and receive messages with the client.
 * @param dataConnectFD
 * - The data connection used to read contents from the client to write to the new file.
 * @return
 * - Returns 0 for success, -1 for a recoverable error, or -2 for a nonrecoverable fatal error.
 */
int putFile_Server(int connectfd, int dataConnectFD) {
    //Read in a path from the client using the control connection.
    char path[BUF_SIZE];
    char response[BUF_SIZE];
    ssize_t n;
    if ((n = read(connectfd, path, BUF_SIZE)) <= 0) {
        int err = errno;
        fprintf(stderr, "Read failed. Line %d. Error: %s\n", __LINE__ , strerror(err));
        sprintf(response, "EFatal: %s\n", strerror(err));
        if(write(connectfd, response, strlen(response)) == -1) {
            fprintf(stderr, "Write. %s.\n", strerror(errno));
            exit(1);
        }
        return -2;
    }
    path[n-1] = '\0';

    //Parse the path to retrieve the file name.
    char pathCopy[strlen(path)+1];
    strcpy(pathCopy, path);
    char* fileName = basename(pathCopy);

    //Make sure that the file does not already exist.
    if (access(fileName, F_OK) == 0) {
        sprintf(response, "EFile already exists.\n");
        if (write(connectfd, response, strlen(response)) == -1) {
            fprintf(stderr, "Write. %s.\n", strerror(errno));
            exit(1);
        }
        return -1;
    }

    //Create a new file and write to it the contents in the data connection.
    int fd = open(fileName, O_WRONLY | O_CREAT, 0644);
    char buffer[BUF_SIZE];
    write(connectfd, "A\n", 2);
    while((n = read(dataConnectFD, buffer, BUF_SIZE)) > 0) {
        printf("%d\n", __LINE__);
        if (write(fd, buffer, n) < 0) {
            int err = errno;
            fprintf(stderr, "Error: write. %s.\n", strerror(err));
            sprintf(response, "EFatal: %s\n", strerror(err));
            if (write(connectfd, response, strlen(response)) == -1) {
                fprintf(stderr, "Write. %s.\n", strerror(errno));
                exit(1);
            }
            close(fd);
            return -2;
        }
    }
    if (n < 0) {
        int err = errno;
        fprintf(stderr, "Error: read. %s.\n", strerror(err));
        sprintf(response, "EFatal: %s\n", strerror(err));
        if(write(connectfd, response, strlen(response)) == -1) {
            fprintf(stderr, "Write. %s.\n", strerror(errno));
            exit(1);
        }
        close(fd);
        return -2;
    }
    close(fd);
    return 0;
}
