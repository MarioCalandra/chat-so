#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/msg.h>
#include <signal.h>
#include <wait.h>
#include <fcntl.h>

#define ADDRESS "127.0.0.1"
#define PORT 2222
#define SECRET_TOKEN "vd12u30awdj0uejoi"
#define MSG_KEY 38572

struct
{
    long int type;
    char text[128];
} message;

int sockid, reading_pid;

void sighandler(int signum);
int new_read(int sockid, char *buffer, int dim);

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        printf("[USO] ./client_i <nickname>\n");
        exit(EXIT_FAILURE);
    }
    int msgid, sock_conn;
    struct sockaddr_in c_address;
    
    c_address.sin_family = AF_INET;
    c_address.sin_addr.s_addr = inet_addr(ADDRESS);
    c_address.sin_port = htons(PORT);
    
    sockid = socket(AF_INET, SOCK_STREAM, 0);
    char token[128];
    strcpy(token, SECRET_TOKEN);
    sock_conn = connect(sockid, (struct sockaddr *)&c_address, sizeof(c_address));
    write(sockid, token, 128);
    if(sock_conn == -1)
    {
        perror("Connect client");
        exit(EXIT_FAILURE);
    }
    
    char format_text[256];
    sprintf(format_text, "%s", argv[1]);
    write(sockid, format_text, 16);
    signal(SIGTERM, sighandler);
    signal(SIGUSR1, sighandler);
    
    msgid = msgget((key_t)MSG_KEY, 0666 | IPC_CREAT);
    msgctl(msgid, IPC_RMID, NULL);
    msgid = msgget((key_t)MSG_KEY, 0666 | IPC_CREAT);
    
    char text[257];
    int nread;
    pid_t pid;
    pid = fork();
    
    switch(pid)
    {
        case -1:
            perror("Fork (client)");
            exit(EXIT_FAILURE);
        case 0:
            while(1)
            {
                if(new_read(sockid, text, 256) == 0)
                {
                    kill(getppid(), SIGUSR1);
                    close(sockid);
                    exit(EXIT_SUCCESS);
                    break;
                }
                printf("%s", text);
                fflush(stdout);
            }
            break;
        default:
            reading_pid = pid;
            while(1)
            {
                if(msgrcv(msgid, (void *)&message, 2048, 0, 0) == -1)
                {
                    perror("Msgrcv (client)");
                    exit(EXIT_FAILURE);
                }
                if(!strcmp(message.text, "/exit"))
                {
                    printf("[INFO] Ti sei disconnesso.\n");
                    kill(pid, SIGKILL);
                    wait(NULL);
                    exit(EXIT_SUCCESS);
                }
                else if(!strcmp(message.text, "/clear"))
                {
                    switch(fork())
                    {
                        case -1:
                            perror("Fork clear (client)");
                            exit(EXIT_FAILURE);
                        case 0:
                            execlp("clear", "clear", NULL);
                        default:
                            wait(NULL);
                            printf("[INFO] La board è stata pulita.\n\n");
                            break;
                    }
                }
                else
                    write(sockid, message.text, 256);
            }
            break;
    }
}

int new_read(int sockid, char *buffer, int dim)
{
    int n, i;
    for(i = 0; i < dim; i++)
    {
        n = read(sockid, &buffer[i], 1);
        if(n <= 0 || buffer[i] == '\0')
            break;
    }
    return n;
}

void sighandler(int signum)
{
    char buff[128 + 32];
    switch(signum)
    {
        case SIGTERM:
            printf("[INFO] Sei stato kickato dal server.\n");
            kill(reading_pid, SIGKILL);
            wait(NULL);
            exit(EXIT_SUCCESS);
        case SIGUSR1:
            wait(NULL);
            close(sockid);
            exit(EXIT_SUCCESS);
    }
}
