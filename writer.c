#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <string.h>

#define MSG_KEY 38572

void toggleCharacter(char *str);

struct
{
    long int type;
    char text[128];
} message;

int main()
{
    int msgid;
    printf("Prima di scrivere, assicurati di essere entrato in una stanza nel client.\nScrivi /exit quando vorrai chiudere il programma\n\n");
    message.type = 1;
    while(1)
    {
        printf("Scrivi qualcosa: ");
        fgets(message.text, 128, stdin);
        toggleCharacter(message.text);
        if(strlen(message.text) == 0)
            continue;
        msgid = msgget((key_t)MSG_KEY, 0666 | IPC_CREAT);
        if(msgsnd(msgid, (void *)&message, 128, 0) == -1)
        {
            perror("Msgsnd (writer)");
            exit(EXIT_FAILURE);
        }
    }
}

void toggleCharacter(char *str)
{
    int len = strlen(str);
    str[len - 1] = '\0';
}