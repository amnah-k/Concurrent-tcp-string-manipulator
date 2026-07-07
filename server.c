/*
HW2 - server side
Amnah Al-Issa , ID: 155209
Razan Al-Momani, ID: 151870
*/

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXLINE 80

// readn & writen & read_string functions are the same as client.c bc its used in both
// read n bytes
ssize_t readn(int fd, void *buf, size_t n)
{
    size_t nleft = n; // how many bytes left to read
    ssize_t nread; // how many bytes read until now
    char *p = (char *)buf; // buffer pointer

    while (nleft > 0) {
        nread = read(fd, p, nleft);

        if (nread < 0) {
            if (errno == EINTR) // if EINTR, loop again
                continue;
            return -1;
        }

        if (nread == 0) // if nothing was read, brek loop
            break;

        nleft -= nread; // minus how many bytes read from how many bytes nleft
        p += nread; // move pointer to next place to read
    }

    return (ssize_t)(n - nleft); // return how many bytes was read at end
}

// write n bytes
ssize_t writen(int fd, const void *buf, size_t n)
{
    size_t nleft = n; // same logic as readn but for writing
    ssize_t nwritten;
    const char *p = (const char *)buf;

    while (nleft > 0) {
        nwritten = write(fd, p, nleft);

        if (nwritten <= 0) {
            if (nwritten < 0 && errno == EINTR) // if EINTR, loop again
                continue;
            return -1; // if error other than EINTR or if nwritten == 0, return -1
        }

        nleft -= nwritten;
        p += nwritten;
    }

    return (ssize_t)n;
}

// function to read from socket
int read_string(int fd, char *out, size_t max)
{
    size_t i = 0; // index
    char ch; // char we read from string

    while (i < max) {
        if (readn(fd, &ch, 1) != 1) // to read byte by byte, only one at time
            return -1;

        out[i] = ch; // write in buffer
        i++; // continue to next
    
        if (ch == '\0') // if null, done and return 0 no error
            return 0;
    }
    return -1;
}

// SIGCHLD handler for done children
void handle_child(int sig)
{
    int status;
    pid_t pid;
    (void)sig; // only used it bc sometimes it might shows warnings if we don't

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Server_side (parent) > child whose PID = %ld has exited\n", (long)pid);
        fflush(stdout);
    }
}

int main(int argc, char **argv)
{
    int listenfd, connfd;
    struct sockaddr_in servaddr, cliaddr; // server and client address structs
    socklen_t clilen; // client address length
    pid_t childpid;

    // command should be like ./server <port>
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return 1;
    }

    // create server socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket");
        return 1;
    }

    /* i found sometimes if user exited suddenly with ctrl+c or ctrl+z and then we try to run server again
    it will say "address already in use" because the socket is still in TIME_WAIT or CLOSE_WAIT state
    so we can use setsockopt with SO_REUSEADDR to reuse of the address
    */
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // fill server address struct
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons((unsigned short)atoi(argv[1])); // convert port to network byte order
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // listen on all interfaces

    // bind socket
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind");
        close(listenfd);
        return 1;
    }

    // start listening
    if (listen(listenfd, 1024) < 0) {
        perror("listen");
        close(listenfd);
        return 1;
    }

    signal(SIGCHLD, handle_child);

    // parent accept clients and fork child for each time
    while (1) {
        clilen = sizeof(cliaddr);

        printf("\nServer_side (parent) > waiting for client messages...\n");
        fflush(stdout);

        // accept client connection
        connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
        if (connfd < 0) {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }

        childpid = fork(); // create child
        if (childpid < 0) {
            perror("fork");
            close(connfd);
            continue;
        }

        // here starts child process
        if (childpid == 0) {
            char cliip[INET_ADDRSTRLEN]; // client ip as string
            unsigned int cliport; // client port number
            unsigned char header[2], op, arg, status;
            char buff[MAXLINE+1]; 
            char result[256];
            size_t i, j, len; // for loops and length of string

            close(listenfd); // child doesnt need listen socket

            // get client ip/port
            inet_ntop(AF_INET, &cliaddr.sin_addr, cliip, sizeof(cliip));
            cliport = (unsigned int)ntohs(cliaddr.sin_port);
            printf("Server_side (child) > connected to client (%s, %u)\n", cliip, cliport);
            fflush(stdout);

            while (1) {
                printf("\nServer_side (child) > waiting for client messages...\n");
                fflush(stdout);

                if (readn(connfd, header, 2) != 2) { // read op and arg from header
                    printf("Server_side (child) > client disconnected, goodbye\n");
                    break;
                }
                op = header[0];
                arg = header[1];

                // read string from client
                if (read_string(connfd, buff, sizeof(buff)) < 0) {
                    status = 1; // status 1 for error
                    strcpy(result, "read error");
                    writen(connfd, &status, 1);
                    writen(connfd, result, strlen(result)+1);
                    continue;
                }

                printf("Server_side (child) > received \"%s\" from (%s, %u) with request type = %u\n",
                       buff, cliip, cliport, (unsigned)op);
                fflush(stdout);

                len = strlen(buff); // length of string

                // op 1: convert to uppercase
                if (op == 1) {
                    if (len >= sizeof(result)) {
                        status = 1;
                        strcpy(result, "error: string too long");
                    } else {
                        strcpy(result, buff);
                        for (j = 0; result[j] != '\0'; j++) {
                            if (result[j] >= 'a' && result[j] <= 'z') // if lowercase, convert to uppercase
                                result[j] = (char)(result[j] - 32); // minus 32 from ascii
                        }
                        status = 0; // status 0 for no error
                    }

                // op 2: count number of chars
                } else if (op == 2) {
                    sprintf(result, "%zu", len);
                    status = 0;

                // op 3: count frequency of one character
                } else if (op == 3) {
                    if (arg == '\0') {
                        status = 1;
                        strcpy(result, "error: no character given");
                    } else {
                        size_t count = 0;
                        for (i = 0; buff[i] != '\0'; i++) {
                            if ((unsigned char)buff[i] == arg) // if char matches arg, increment count
                                count++;
                        }
                        if (count == 0) {
                            status = 1;
                            strcpy(result, "error: character not found");
                        } else {
                            sprintf(result, "%zu", count); // write count as string in result
                            status = 0;
                        }
                    }

                // any op other than 1/2/3 is not valid
                } else {
                    status = 1;
                    strcpy(result, "error: unknown operation");
                }

                // send status then result string back to client
                writen(connfd, &status, 1);
                writen(connfd, result, strlen(result)+1);

                // depends on error or not
                if (status == 0)
                    printf("Server_side (child) > sending \"%s\" to the client\n", result);
                else
                    printf("Server_side (child) > sending error \"%s\" to the client\n", result);

                fflush(stdout);
            }

            close(connfd); // done with client, child exits 
            exit(0);
        }

        printf("Server_side (parent) > created child process with PID = %ld to handle client\n",
               (long)childpid);
        fflush(stdout);

        close(connfd); // parent closes connected socket, child handles client
    }

    close(listenfd);
    return 0;
}
