#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>
#define DEFAULT_PORT 8000
#define MAXLINE 4096

typedef struct ClientNode
{
    int fd;
    struct ClientNode *next;
} Client;

int add(Client *head, int fd)
{
    Client *new_node = (Client *)malloc(sizeof(Client));
    if (new_node == NULL)
        return -1;

    new_node->fd = fd;
    new_node->next = head->next;

    head->next = new_node;

    return 0;
}

int del(Client *head, int fd)
{
    Client *p = head;

    while (p->next != NULL && p->next->fd != fd)
        p = p->next;

    if (p->next == NULL)
        return -1;
    else
    {
        Client *deleted_node = p->next;
        p->next = deleted_node->next;
        free(deleted_node);
        return 0;
    }
}

void broadcast(Client *head, char *buf, int n)
{
    Client *p = head->next;

    while (p != NULL)
    {
        if (send(p->fd, buf, n, 0) == -1)
        {
            printf("send to fd=%d failed, remove this client\n", p->fd);

            close(p->fd);
            del(head, p->fd);
        }
        else
        {
            printf("broadcast %s\n", buf);
            p = p->next;
        }
    }
}

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int sem_init(int key)
{
    int semid;
    semid = semget((key_t)key, 0, 0666);
    if (semid == -1)
    {
        semid = semget((key_t)key, 1, 0666 | IPC_CREAT);

        union semun mu;
        mu.val = 1;

        semctl(semid, 0, SETVAL, mu);
    }
    return semid;
}

void sem_p(int id)
{
    struct sembuf buff;
    buff.sem_num = 0;
    buff.sem_op = -1;
    buff.sem_flg = SEM_UNDO;
    semop(id, &buff, 1);
}

void sem_v(int id)
{
    struct sembuf buff;
    buff.sem_num = 0;
    buff.sem_op = 1;
    buff.sem_flg = SEM_UNDO;
    semop(id, &buff, 1);
}

void sem_del(int id)
{
    semctl(id, 0, IPC_RMID);
}

typedef struct SharedBuffer
{
    int len;
    int is_written;
    char buffer[4096];
} SharedBuffer;

int main(int argc, char **argv)
{
    //init sem
    int SEM_KEY = 111;
    int semid = sem_init(SEM_KEY);

    //init shared memory
    int MEMORY_KEY = 222;
    int shmid = shmget(MEMORY_KEY, sizeof(SharedBuffer), 0666 | IPC_CREAT);
    if (shmid == -1)
    {
        printf("shared memory error\n");
        return -1;
    }
    SharedBuffer *buffer = (SharedBuffer *)shmat(shmid, NULL, 0);
    memset(buffer, 0, sizeof(SharedBuffer));

    //init socket
    int socket_fd, connect_fd;
    struct sockaddr_in servaddr;
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        exit(0);
    }
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); //IP
    servaddr.sin_port = htons(DEFAULT_PORT);      //PORT

    //if bind successfully
    if (bind(socket_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        printf("bind socket error: %s(errno: %d)\n", strerror(errno), errno);
        exit(0);
    }
    //if listen successfully
    if (listen(socket_fd, 10) == -1)
    {
        printf("listen socket error: %s(errno: %d)\n", strerror(errno), errno);
        exit(0);
    }

    Client *head = (Client *)malloc(sizeof(Client));
    head->fd = -1;
    head->next = NULL;

    fd_set readfds;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    printf("======waiting for client's request======\n");
    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(socket_fd, &readfds);

        //non-blocking accept
        int rtn = select(socket_fd + 1, &readfds, NULL, NULL, &timeout);
        if ((rtn < 0) && (errno != EINTR))
        {
            printf("select error");
            return -1;
        }
        //if select don't timeout, we can still broadcast
        else if (rtn != 0)
        {
            //this if block is not essential, because fdset just has one fd
            if (FD_ISSET(socket_fd, &readfds))
            {
                if ((connect_fd = accept(socket_fd, (struct sockaddr *)NULL, NULL)) == -1)
                {
                    printf("accept socket error: %s(errno: %d)", strerror(errno), errno);
                    continue;
                }
                if (add(head, connect_fd) == -1)
                {
                    printf("add client error\n");
                    continue;
                }

                //fork a child process to receive message from a client
                if (!fork())
                {
                    char tem_buffer[4096];
                    int n;
                    SharedBuffer *buffer = (SharedBuffer *)shmat(shmid, NULL, 0);
                    while (1)
                    {
                        n = recv(connect_fd, tem_buffer, MAXLINE, 0);
                        tem_buffer[n] = '\0';

                        //wait for server to broadcast message
                        while (buffer->is_written == 1)
                            sleep(100);

                        //send its data to server
                        sem_p(semid);
                        strcpy(buffer->buffer, tem_buffer);
                        buffer->is_written = 1;
                        buffer->len = n;
                        sem_v(semid);
                    }
                }
            }
        }

        if (buffer->is_written)
        {
            sem_p(semid);
            broadcast(head, buffer->buffer, buffer->len);
            buffer->is_written = 0;
            sem_v(semid);
        }
    }
}