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
#include <pthread.h>

#define ADDRESS "127.0.0.1"
#define PORT 2222
#define SECRET_TOKEN "b12389doajdawd9123ad"
#define MSG_KEY 38572

#define TRUE 1
#define FALSE 0

struct
{
    long int type;
    char text[128];
} message;

int sockid;

int new_read(int sockid, char *buffer, int dim);
void new_write(int sockid, char *buffer, int dim);
void *reading_function(void *arg);

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        printf("Specificare il nickname come primo argomento.\n");
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
    if(sock_conn == -1)
    {
        perror("Connect client");
        exit(EXIT_FAILURE);
    }    
    char format_text[256];
    sprintf(format_text, "%s", argv[1]);
    new_write(sockid, token, 128);
    new_write(sockid, format_text, 32);
    
    msgid = msgget((key_t)MSG_KEY, 0666 | IPC_CREAT);
    msgctl(msgid, IPC_RMID, NULL);
    msgid = msgget((key_t)MSG_KEY, 0666 | IPC_CREAT);
    
    pthread_t reading_thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&reading_thread, NULL, reading_function, (void *)&sockid);
    pthread_attr_destroy(&attr);
    
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
            new_write(sockid, message.text, 256);
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

void new_write(int sockid, char *buffer, int dim)
{  
    int i;
    for(i = 0; i < dim; i++)
    {
        write(sockid, &buffer[i], 1);
        if(buffer[i] == '\0')
            break;
    }
}

void *reading_function(void *arg)
{
    char text[256];
    int *sockid = (int *)arg;
    while(1)
    {
        if(new_read(*sockid, text, 256) == 0)
        {
            close(*sockid);
            printf("[INFO] La comunicazione con il server è terminata.\n");
            fflush(stdout);
            exit(EXIT_SUCCESS);
            break;
        }
        printf("%s", text);
        fflush(stdout);
    }
}
