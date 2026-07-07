/*
HW2 - client side
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
#include <unistd.h>

#define MAXLINE 80

int sockfd = -1; // we made it global to use in the signal handler to close socket before exit

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

// SIGQUIT handler
void handle_quit(int sig)
{
    (void)sig; // only used it bc sometimes it might shows warnings if we don't
    printf("\nClient_side > Done, goodbye!\n");

    if (sockfd != -1)
        close(sockfd); // close socket before exit
    exit(0);
}

int main(int argc, char **argv)
{
    struct sockaddr_in servaddr; // server address struct
    char buff[MAXLINE+1]; // buffer for string input

    // bc command should be like ./client <IP> <port>
    if (argc != 3) {
        fprintf(stderr, "usage: %s <IP> <port>\n", argv[0]);
        return 1;
    }

    // create client socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    // fill server address struct
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons((unsigned short)atoi(argv[2])); // port is the 3rd argument argv[2], convert to int and then network byte order

    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) { // IP is the 2nd argument argv[1]
        fprintf(stderr, "invalid IP: %s\n", argv[1]);
        close(sockfd);
        return 1;
    }

    // connect to server
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    // first loop is to get first string from user
    while (1) {
        printf("Client_side > please enter a string: ");
        fflush(stdout); // to make sure prompt is printed before input

        if (fgets(buff, sizeof(buff), stdin) == NULL) { // check if NULL, error
            close(sockfd);
            return 1;
        }

        buff[strcspn(buff, "\n")] = '\0'; // to remove \n at end of string

        if (buff[0] != '\0') // save in buffer until \n
            break;

        printf("Client_side > empty string not allowed, try again\n");
    }

    signal(SIGQUIT, handle_quit); // for ctrl+\ to quit
    signal(SIGINT, handle_quit); // for ctrl+c if user wants to quit with that (idk if this is needed but just in case)

    // main loop to show menu and send requests
    while (1) {
        int choice; // choice for op
        char choicebuf[32]; // choice is first as string so will need buffer to read it and convert to int later
        unsigned char op; // op for operation
        unsigned char arg = 0; // arg for op 3 that needs argument
        unsigned char status; // status for response if error or not
        unsigned char header[2]; // header to send to server, first byte for op and second byte for arg, if no arg will be 0
        char response[256]; // buffer for response from server

        printf("\nClient_side > please select an operation from the following menu:\n");
        printf("  1) Change the string to capital letters\n");
        printf("  2) Count number of characters in the string\n");
        printf("  3) Count the frequency of some character\n");
        printf("  4) Enter another string\n");
        printf("Choice: ");
        fflush(stdout);

        if (fgets(choicebuf, sizeof(choicebuf), stdin) == NULL) // check if NULL, error
            break;

        choicebuf[strcspn(choicebuf, "\n")] = '\0'; // to remove \n at end of string
        choice = atoi(choicebuf); // convert choice from string to int

        if (choice < 1 || choice > 4) {
            printf("Client_side > invalid choice, pick 1-4\n");
            continue;
        }

        if (choice == 4) {
            while (1) {
                printf("Client_side > please enter a string: ");
                fflush(stdout);

                if (fgets(buff, sizeof(buff), stdin) == NULL) { // check if NULL, error
                    close(sockfd);
                    return 1;
                }

                buff[strcspn(buff, "\n")] = '\0';

                if (buff[0] != '\0')
                    break;

                printf("Client_side > empty string not allowed, try again\n");
            }
            continue;
        }

        // if choice is 3, need to ask for arg
        if (choice == 3) {
            char tmp[32];
            printf("Client_side > enter the character to count: ");
            fflush(stdout);

            if (fgets(tmp, sizeof(tmp), stdin) == NULL)
                break;

            tmp[strcspn(tmp, "\n")] = '\0';

            if (tmp[0] == '\0') {
                printf("Client_side > no character entered\n");
                continue;
            }

            arg = (unsigned char)tmp[0];
        }

        // header to send to server
        op = (unsigned char)choice;
        header[0] = op;
        header[1] = arg;

        printf("Client_side > sending \"%s\" to the server with requested operation %d\n",
               buff, choice);

        // check no errors in header then send to server
        if (writen(sockfd, header, 2) != 2) {
            perror("write header");
            break;
        }

        // check no errors in string then send to server
        if (writen(sockfd, buff, strlen(buff)+1) != (ssize_t)(strlen(buff)+1)) {
            perror("write string");
            break;
        }

        // read response from server
        if (readn(sockfd, &status, 1) != 1) {
            perror("read status");
            break;
        }

        // if error, read error message, if no error, read result
        if (read_string(sockfd, response, sizeof(response)) < 0) {
            fprintf(stderr, "read error\n");
            break;
        }

        // print response to user
        if (status == 0)
            printf("Client_side > received \"%s\" from the server\n", response);
        else
            printf("Client_side > server error: %s\n", response);
    }

    close(sockfd);
    return 0;
}
