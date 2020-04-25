#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAXLINE 4096
#define DEFAULT_PORT 8000

int main(int argc, char *argv[])
{
    int sockfd, n, rec_len;
    char ip[40];
    char recvline[4096], sendline[4096];
    char buf[MAXLINE];
    struct sockaddr_in servaddr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        exit(0);
    }

    //configure server's ip and port
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(DEFAULT_PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    //connect to server
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        printf("connect error: %s(errno: %d)\n", strerror(errno), errno);
        exit(0);
    }

    //send and recv message in a blocking way
    //however, you can just type "Enter"(or in ASCII, "\n") to skip send, and receive the broadcasted message from server
    while (1)
    {
        printf("send msg to server: \n");
        fgets(sendline, 4096, stdin);

        //if you want to skip the blocked sending
        if (strcmp(sendline, "\n") != 0)
        {
            if (send(sockfd, sendline, strlen(sendline), 0) < 0)
            {
                printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
                exit(0);
            }
        }

        //recv broadcasted message from server
        if ((rec_len = recv(sockfd, buf, MAXLINE, 0)) == -1)
        {
            perror("recv error");
            exit(1);
        }
        buf[rec_len] = '\0';
        printf("Received : %s ", buf);
    }
    close(sockfd);
    exit(0);
}