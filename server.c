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
#include <sys/time.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <time.h>

#define TRUE 1
#define FALSE 0

#define PORT 2222

#define SECRET_TOKEN "b12389doajdawd9123ad"
#define SECRET_COMMAND "/admin"
#define SECRET_CODE "admin"

#define MAX_ROOM 32
#define MAX_SLOT 16

#define RANK_NONUSER 0
#define RANK_USER 1
#define RANK_ADVUSER 2
#define RANK_ADMIN 3

#define USERS_PATH "users.csv"
#define ROOMS_PATH "rooms.csv"
#define BAN_PATH "ban.txt"
#define LOG_PATH "log.txt"

void toggleCharacter(char *str);
int safeString(char *str);

void sendToOtherClients(int senderid, char *text);
void sendAllMessage(int serverid, char *text);
void sendClientMessage(int clientid, char *text);
void sendRoomMessage(int senderid, int roomid, char *text);
void sendServerMessage(char *text);
void kickFromR(int clientid);
void kickFromS(int clientid);
void BanIP(char *IP);
int isIPBanned(char *IP);
int clientsNumberPerIP(char *IP);
int getIDFromName(char *name);

void initClient(int clientid, struct sockaddr_in c_address);
int controlClient(int clientid);
int loadUserData(int clientid, char *name);
void saveUserData(int clientid);
int deleteUserData(char *name);

int loadRoomData(int roomid, char *name);
void loadAllRoomData();
void saveRoomData(int roomid);
void deleteRoomData(int roomid);
int isValidRoom(int roomid);
int availableRoom();
void showRooms(int clientid);
int isPublicRoom(int roomid);
int isModeratorRoom(int clientid, int roomid);
int isFullRoom(int roomid);

int new_read(int sockid, char *buffer, int dim);

struct 
{
    char name[16];
    char password[32];
    int rank;
    int logged;
    int room;
    int realClient;
    char IP[13];
} infoU[FD_SETSIZE];

struct
{
    char name[64];
    char moderator[16];
    char password[32];
    int slot;
    int clients;
} infoR[MAX_ROOM]; // /banip /kick /setrank /broadcast /remove

char *general_commands[] = {"/register", "/login", "/setpassword", "/pm", "/clear", "/exit", "/help"};
char *room_commands[] = {"/refresh", "/join", "/leave", "/list"};
char *aduser_commands[] = {"/makeroom"};
char *moderator_commands[] = {"/kickr", "/setrpassword", "/removeroom"};
char *admin_commands[] = {"/broadcast", "/setrank", "/kick", "/banip", "/remove"};

int server_sockid;
fd_set readfds;

int main()
{
    int client_sockid;
    struct sockaddr_in s_address;
    fd_set tempfds;
    
    s_address.sin_family = AF_INET;
    s_address.sin_addr.s_addr = htonl(INADDR_ANY);
    s_address.sin_port = htons(PORT);
    
    server_sockid = socket(AF_INET, SOCK_STREAM, 0);
    if(bind(server_sockid, (struct sockaddr *)&s_address, sizeof(s_address)) == -1)
    {
        perror("Bind server");
        exit(EXIT_FAILURE);
    }
    if(listen(server_sockid, 5) == -1)
    {
        perror("Listen server");
        exit(EXIT_FAILURE);
    }
    
    printf("\n[AVVISO] Il server è stato avviato con successo.\n\n");
    FD_ZERO(&readfds);
    FD_SET(server_sockid, &readfds);
    
    loadAllRoomData();
    
    char text[512], format_text[512];    
    while(1)
    {
        int nread;
        
        tempfds = readfds;
        
        
        int result = select(FD_SETSIZE, &tempfds, 0, 0, 0);
        if(result < 1)
        {
            perror("Select (server)");
            exit(EXIT_FAILURE);
        }
        for(int fd = 0; fd < FD_SETSIZE; fd ++)
        {
            if(FD_ISSET(fd, &tempfds))
            {
                if(fd == server_sockid)
                {
                    struct sockaddr_in c_address;
                    socklen_t socklen = sizeof(c_address);
                    client_sockid = accept(server_sockid, (struct sockaddr *)&c_address, &socklen);
                    
                    initClient(client_sockid, c_address);
                    FD_SET(client_sockid, &readfds);
                    
                    if(clientsNumberPerIP(infoU[client_sockid].IP) > 1)
                    {
                        for(int i = 0; i < FD_SETSIZE; i++)
                        {
                            if(FD_ISSET(i, &readfds) && (strcmp(infoU[i].IP, infoU[client_sockid].IP) == 0))
                            {
                                sendClientMessage(i, "Può esservi solo un client per rete.\n\n");
                                kickFromS(i);
                            }
                        }
                    }
                }
                else
                {
                    ioctl(fd, FIONREAD, &nread);
                    
                    if(nread == 0) 
                    {
                        close(fd);
                        FD_CLR(fd, &readfds);
                        if(infoU[fd].realClient == 1)
                        {
                            sprintf(format_text, "%s si è disconnesso.\n", infoU[fd].name);
                            sendServerMessage(format_text);
                            if(infoU[fd].room != -1)
                            {
                                sprintf(format_text, "[INFO] %s ha abbandonato la stanza.\n", infoU[fd].name);
                                sendRoomMessage(fd, infoU[fd].room, format_text);
                            }
                            kickFromR(fd);
                        }
                    } 
                    else
                    {
                        new_read(fd, text, 256);
                        if(infoU[fd].realClient == 0)
                        {
                            if(!strcmp(text, SECRET_TOKEN))
                            {
                                char buffer[256];
                                new_read(fd, buffer, 256);
                                if(strlen(buffer) < 3 || strlen(buffer) > 15)
                                {
                                    sendClientMessage(fd, "Il nickname deve contenere da 3 a 15 caratteri.\n\n");
                                    kickFromS(fd);
                                    break;
                                }
                                strcpy(infoU[fd].name, buffer);
                                if(controlClient(fd) == 0)
                                {
                                    kickFromS(fd);
                                    break;
                                }
                                sprintf(format_text, "%s si è connesso. (IP: %s)\n", infoU[fd].name, infoU[fd].IP);
                                sendServerMessage(format_text);
                                sendClientMessage(fd, "\nConnessione al server avvenuta con successo.\nAssicurati di avere aperto il writer per scrivere.\nDigita /help nel writer per avere una panoramica dei comandi disponibili.\nDigita /exit nel writer per disconnettere il client.\n\n");
                                if(loadUserData(fd, infoU[fd].name) == 0)
                                {
                                    sprintf(format_text, "\t\t\tBenvenuto %s\n\nQuesto account non risulta registrato.\nNel writer digita /register <password> per registrarti.\n\n", infoU[fd].name);
                                    sendClientMessage(fd, format_text);
                                }
                                else
                                {
                                    sprintf(format_text, "\t\t\tBentornato %s\n\nQuesto account risulta registrato.\nNel writer digita /login <password> per loggarti.\n\n", infoU[fd].name);
                                    sendClientMessage(fd, format_text);
                                }
                                infoU[fd].realClient = 1;
                            }
                            else
                            {
                                sprintf(format_text, "Tentativo di connessione da client sconosciuto. (IP: %s)\n", infoU[fd].IP);
                                sendServerMessage(format_text);
                                kickFromS(fd);
                            }
                            break;
                        }
                        if(text[0] == '/')
                        {
                            char cmd[256];
                            sscanf(text, "%s", cmd);
                            
                            /* COMANDI */
                            if(!strcmp(cmd, "/register"))
                            {
                                if(infoU[fd].rank != RANK_NONUSER)
                                {
                                    sendClientMessage(fd, "[ERRORE] Questo account risulta già registrato.\n");
                                    break;
                                }
                                char password[32];
                                if(sscanf(text, "%*s %s", password) != 1)
                                {
                                    sendClientMessage(fd, "[USO] /register <password>\n");
                                    break;
                                }
                                if(safeString(password) == 0)
                                {
                                    sendClientMessage(fd, "[ERRORE] Formato password non valido\n");
                                    break;
                                }
                                if(strlen(password) < 6 || strlen(password) > 20)
                                {
                                    sendClientMessage(fd, "[ERRORE] La password deve contenere da 6 a 20 caratteri.\n");
                                    break;
                                }
                                strcpy(infoU[fd].password, password);
                                infoU[fd].rank = RANK_USER;
                                sendClientMessage(fd, "[INFO] Account registrato con successo, loggati con /login.\n");   
                                saveUserData(fd);
                                sprintf(format_text, "%s si è registrato al server.\n", infoU[fd].name);
                                sendServerMessage(format_text);
                            }
                            else if(!strcmp(cmd, "/login"))
                            {
                                if(infoU[fd].rank == RANK_NONUSER)
                                {
                                    sendClientMessage(fd, "[ERRORE] Questo account non risulta registrato.\n");
                                    break;
                                }
                                else if(infoU[fd].logged == TRUE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Account già loggato.\n");
                                    break;
                                }
                                char password[32];
                                if(sscanf(text, "%*s %s", password) != 1)
                                {
                                    sendClientMessage(fd, "[USO] /login <password>\n");
                                    break;
                                }
                                if(strcmp(infoU[fd].password, password) != 0)
                                {
                                    sendClientMessage(fd, "[ERRORE] La password è errata.\n");
                                    break;
                                }
                                infoU[fd].logged = TRUE;
                                sendClientMessage(fd, "[INFO] Account loggato con successo.\n");
                                showRooms(fd);
                                sprintf(format_text, "%s ha effettuato il login.\n", infoU[fd].name);
                                sendServerMessage(format_text);
                            }
                            else if(!strcmp(cmd, SECRET_COMMAND))
                            {
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                char password[32];
                                if(sscanf(text, "%*s %s", password) != 1)
                                {
                                    sprintf(format_text, "[USO] %s <password>\n", SECRET_COMMAND);
                                    sendClientMessage(fd, format_text);
                                    break;
                                }
                                if(infoU[fd].rank == RANK_ADMIN)
                                {
                                    sendClientMessage(fd, "[ERRORE] Hai già i permessi di admin.\n");
                                    break;
                                }
                                if(strcmp(password, SECRET_CODE) != 0)
                                {
                                    sendClientMessage(fd, "[ERRORE] Il codice segreto è errato.\n");
                                    break;
                                }
                                infoU[fd].rank = RANK_ADMIN;
                                saveUserData(fd);
                                sendClientMessage(fd, "[INFO] Hai acquisito i poteri di admin.\n");                                
                            }
                            else if(!strcmp(cmd, "/setpassword"))
                            {
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                char old_password[32], new_password[32];
                                if(sscanf(text, "%*s %s %s", old_password, new_password) != 2)
                                {
                                    sendClientMessage(fd, "[USO] /setpassword <vecchia> <nuova>\n");
                                    break;
                                }
                                if(strcmp(old_password, infoU[fd].password) != 0)
                                {
                                    sendClientMessage(fd, "[ERRORE] La vecchia password non corrisponde.\n");
                                    break;
                                }                                
                                if(safeString(new_password) == 0)
                                {
                                    sendClientMessage(fd, "[ERRORE] Formato password non valido\n");
                                    break;
                                }
                                if(strlen(new_password) < 6 || strlen(new_password) > 20)
                                {
                                    sendClientMessage(fd, "[ERRORE] La password deve contenere da 6 a 20 caratteri.\n");
                                    break;
                                }
                                strcpy(infoU[fd].password, new_password);
                                saveUserData(fd);
                                sendClientMessage(fd, "[INFO] La password è stata cambiata.\n");
                                break;
                            }
                            else if(!strcmp(cmd, "/pm"))
                            {
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                char dest[16], msg[128];
                                if(sscanf(text, "%*s %s %[^\n] \n", dest, msg) != 2)
                                {
                                    sendClientMessage(fd, "[USO] /pm <nome> <messaggio>\n");
                                    break;
                                }
                                int pid = getIDFromName(dest);
                                if(pid == -1)
                                {
                                    sendClientMessage(fd, "[ERRORE] Questa persona non risulta connessa.\n");
                                    break;
                                }
                                if(pid == fd)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non puoi inviare un PM a te stesso.\n");
                                    break;
                                }
                                if(infoU[pid].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Questa persona deve ancora effettuare il login.\n");
                                    break;
                                }
                                sprintf(format_text, "[PM] %s: %s\n", infoU[fd].name, msg);          
                                sendClientMessage(pid, format_text);
                                sprintf(format_text, "[PM] (%s -> %s): %s\n", infoU[fd].name, infoU[pid].name, msg);
                                sendServerMessage(format_text);
                                sendClientMessage(fd, "[INFO] Il PM è stato inviato con successo.\n");
                            }
                            else if(!strcmp(cmd, "/help"))
                            { 
                                char cmd_list[256];
                                strcpy(cmd_list, "\nComandi generali:");
                                for(int i = 0; i < (sizeof(general_commands) / 8); i++)
                                    sprintf(cmd_list, "%s %s", cmd_list, general_commands[i]);
                                sprintf(cmd_list, "%s\n", cmd_list);
                                sendClientMessage(fd, cmd_list);
                                
                                strcpy(cmd_list, "Comandi stanze:");
                                for(int i = 0; i < (sizeof(room_commands) / 8); i++)
                                    sprintf(cmd_list, "%s %s", cmd_list, room_commands[i]);
                                sprintf(cmd_list, "%s\n", cmd_list);
                                sendClientMessage(fd, cmd_list);  
                                
                                if(infoU[fd].rank >= RANK_ADVUSER)
                                {
                                    strcpy(cmd_list, "Comandi utente avanzato:");
                                    for(int i = 0; i < (sizeof(aduser_commands) / 8); i++)
                                        sprintf(cmd_list, "%s %s", cmd_list, aduser_commands[i]);
                                    sprintf(cmd_list, "%s\n", cmd_list);
                                    sendClientMessage(fd, cmd_list);
                                }                                
                                if(isModeratorRoom(fd, infoU[fd].room) || infoU[fd].rank == RANK_ADMIN)
                                {
                                    strcpy(cmd_list, "Comandi moderatore:");
                                    for(int i = 0; i < (sizeof(moderator_commands) / 8); i++)
                                        sprintf(cmd_list, "%s %s", cmd_list, moderator_commands[i]);
                                    sprintf(cmd_list, "%s\n", cmd_list);
                                    sendClientMessage(fd, cmd_list);
                                }                                
                                if(infoU[fd].rank == RANK_ADMIN)
                                {
                                    strcpy(cmd_list, "Comandi admin:");
                                    for(int i = 0; i < (sizeof(admin_commands) / 8); i++)
                                        sprintf(cmd_list, "%s %s", cmd_list, admin_commands[i]);
                                    sprintf(cmd_list, "%s\n", cmd_list);
                                    sendClientMessage(fd, cmd_list);
                                }
                                sendClientMessage(fd, "\n");
                            }
                            else if(!strcmp(cmd, "/remove"))
                            {                    
                                if(infoU[fd].rank != RANK_ADMIN)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non puoi utilizzare questo comando.\n");
                                    break;
                                }
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                char dest[16];
                                if(sscanf(text, "%*s %s", dest) != 1)
                                {
                                    sendClientMessage(fd, "[USO] /remove <nome>\n");
                                    break;
                                }
                                if(deleteUserData(dest) == 0)
                                {
                                    sendClientMessage(fd, "[ERRORE] Questa persona non risulta registrata nel database.\n");
                                    break;
                                }
                                int pid = getIDFromName(dest);
                                if(pid != -1)
                                {
                                    sendClientMessage(pid, "[INFO] Il tuo account è stato rimosso da un admin.\n");
                                    kickFromS(pid);
                                }
                                sendClientMessage(fd, "[INFO] L'utente è stato rimosso dal database.\n");
                                sprintf(format_text, "%s ha rimosso %s dal database.\n", infoU[fd].name, dest);
                                sendServerMessage(format_text);
                            }
                            else if(!strcmp(cmd, "/refresh"))
                            {
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                if(infoU[fd].room != -1)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non puoi utilizzare questo comando quando sei in una stanza.\n");
                                    break;
                                }
                                showRooms(fd);
                            }
                            else if(!strcmp(cmd, "/makeroom"))
                            {
                                if(infoU[fd].rank < RANK_ADVUSER)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non hai i permessi per utilizzare questo comando.\n");
                                    break;
                                }
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                int roomid = availableRoom();
                                if(roomid == -1)
                                {
                                    sendClientMessage(fd, "[ERRORE] E' stato raggiunto il numero massimo di stanze.\n");
                                    break;
                                }
                                if(infoU[fd].room != -1)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non devi essere in nessuna stanza per crearne una.\n");
                                    break;
                                }
                                char t_name[64], t_password[32];
                                int t_slot;
                                if(sscanf(text, "%*s %d %s %[^\n] \n", &t_slot, t_password, t_name) != 3)
                                {
                                    sendClientMessage(fd, "[USO] /makeroom <slot> <password(public per renderla pubblica)> <nome>\n");
                                    break;
                                }
                                if(t_slot < 2 || t_slot > MAX_SLOT)
                                {
                                    sendClientMessage(fd, "[ERRORE] La stanza può avere da 2 a 8 slot.\n");
                                    break;
                                }
                                if(safeString(t_password) == 0)
                                {
                                    sendClientMessage(fd, "[ERRORE] Formato password non valido.\n");
                                    break;
                                }
                                if(strlen(t_password) < 6 || strlen(t_password) > 20)
                                {
                                    sendClientMessage(fd, "[ERRORE] La password deve contenere da 6 a 20 caratteri.\n");
                                    break;
                                }
                                if(safeString(t_name) == 0)
                                {
                                    sendClientMessage(fd, "[ERRORE] Formato nome non valido.\n");
                                    break;
                                }
                                if(strlen(t_name) < 3 || strlen(t_name) > 60)
                                {
                                    sendClientMessage(fd, "[ERRORE] Il nome deve contenere da 3 a 60 caratteri.\n");
                                    break;
                                }
                                strcpy(infoR[roomid].name, t_name);
                                strcpy(infoR[roomid].password, t_password);
                                strcpy(infoR[roomid].moderator, infoU[fd].name);
                                infoR[roomid].slot = t_slot;
                                infoR[roomid].clients = 0;
                                saveRoomData(roomid);
                                sendClientMessage(fd, "[INFO] La stanza è stata creata con successo.\n");
                                sprintf(format_text, "%s ha creato la stanza: %s.\n", infoU[fd].name, t_name);
                                sendServerMessage(format_text);
                                showRooms(fd);
                            }
                            else if(!strcmp(cmd, "/removeroom"))
                            {
                                if(infoU[fd].rank < RANK_ADVUSER)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non hai i permessi per utilizzare questo comando.\n");
                                    break;
                                }
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                int roomid;
                                if(sscanf(text, "%*s %d", &roomid) != 1)
                                {
                                    sendClientMessage(fd, "[USO] /removeroom <roomid>\n");
                                    break;
                                }
                                if(!isValidRoom(roomid))
                                {
                                    sendClientMessage(fd, "[ERRORE] La roomid inserita è inesistente.\n");
                                    break;
                                }
                                if((infoU[fd].rank != RANK_ADMIN) && (isModeratorRoom(fd, roomid) == 0))
                                {
                                    sendClientMessage(fd, "[ERRORE] Non hai i permessi per rimuovere questa stanza.\n");
                                    break;
                                }
                                sprintf(format_text, "%s ha rimosso la stanza: %s.\n", infoU[fd].name, infoR[roomid].name);
                                sendServerMessage(format_text);
                                deleteRoomData(roomid);
                                sendClientMessage(fd, "[INFO] La stanza è stata rimossa.\n");
                                for(int i = 0; i < FD_SETSIZE; i++)
                                    if(FD_ISSET(i, &readfds))
                                        if(infoU[i].room == roomid && i != server_sockid)
                                        {
                                            kickFromR(i);
                                            if(i != fd)
                                                sendClientMessage(i, "[INFO] La stanza in cui eri è stata rimossa.\n");
                                            showRooms(i);
                                        }
                            }
                            else if(!strcmp(cmd, "/join"))
                            {
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                if(infoU[fd].room != -1)
                                {
                                    sendClientMessage(fd, "[ERRORE] Sei già in una stanza. Usa /leave per abbandonarla.\n");
                                    break;
                                }
                                int roomid;
                                if(sscanf(text, "%*s %d", &roomid) != 1)
                                {
                                    sendClientMessage(fd, "[USO] /join <roomid> <password(se privata)>\n");
                                    break;
                                }
                                if(!isValidRoom(roomid))
                                {
                                    sendClientMessage(fd, "[ERRORE] La roomid inserita è inesistente.\n");
                                    break;
                                }
                                if(isFullRoom(roomid))
                                {
                                    sendClientMessage(fd, "[ERRORE] La stanza è piena.\n");
                                    break;
                                }
                                if(!isPublicRoom(roomid))
                                {
                                    char password[32];
                                    if(sscanf(text, "%*s %d %s", &roomid, password) != 2)
                                    {
                                        sendClientMessage(fd, "[ERRORE] La stanza è protetta da password.\n[USO] /join <roomid> <password>\n");
                                        break;
                                    }
                                    if(strcmp(infoR[roomid].password, password) != 0)
                                    {
                                        sendClientMessage(fd, "[ERRORE] La password è errata.\n");
                                        break;
                                    }                                       
                                }
                                char text[32];
                                sendClientMessage(fd, "[INFO] Sei entrato nella stanza. Per abbandonarla digita /leave.\n\n");
                                sprintf(text, "[INFO] %s è entrato nella stanza.\n", infoU[fd].name);
                                sprintf(format_text, "%s è entrato nella roomid %d (%s)\n", infoU[fd].name, roomid, infoR[roomid].name);                                
                                sendServerMessage(format_text);
                                infoR[roomid].clients ++;
                                infoU[fd].room = roomid;
                                sendRoomMessage(fd, roomid, text);                   
                            }
                            else if(!strcmp(cmd, "/leave"))
                            {
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                if(infoU[fd].room == -1)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non sei in una stanza. Usa /join per accedere ad una.\n");
                                    break;
                                }
                                char text[32];
                                sprintf(text, "[INFO] %s ha abbandonato la stanza.\n", infoU[fd].name);
                                sendRoomMessage(fd, infoU[fd].room, text);   
                                sprintf(format_text, "%s ha abbandonato la roomid %d (%s)\n", infoU[fd].name, infoU[fd].room, infoR[infoU[fd].room].name);                                
                                sendServerMessage(format_text);
                                kickFromR(fd);
                                showRooms(fd);
                            }
                            else if(!strcmp(cmd, "/list"))
                            {
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                if(infoU[fd].room == -1)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non sei in una stanza. Usa /join per accedere ad una.\n");
                                    break;
                                }
                                char tmp[256];
                                strcpy(tmp, "[INFO] Membri:");
                                for(int i = 0; i < FD_SETSIZE; i++)
                                    if(FD_ISSET(i, &readfds) && i != server_sockid)
                                        if(infoU[i].room == infoU[fd].room)
                                            sprintf(tmp, "%s %s", tmp, infoU[i].name);
                                sprintf(tmp, "%s\n", tmp);
                                sendClientMessage(fd, tmp);
                            }
                            else if(!strcmp(cmd, "/kickr"))
                            {
                                if(infoU[fd].rank < RANK_ADVUSER)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non hai i permessi per utilizzare questo comando.\n");
                                    break;
                                }
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                char name[16];
                                if(sscanf(text, "%*s %s", name) != 1)
                                {
                                    sendClientMessage(fd, "[USO] /kickr <nome>\n");
                                    break;
                                }
                                int pid = getIDFromName(name);
                                if(pid == -1)
                                {
                                    sendClientMessage(fd, "[ERRORE] Questa persona non risulta essere connessa.\n");
                                    break;
                                }
                                if(pid == fd)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non puoi kickare te stesso.\n");
                                }
                                int roomid = infoU[pid].room;
                                if(roomid == -1)
                                {
                                    sendClientMessage(fd, "[ERRORE] Questa persona non è in una stanza.\n");
                                    break;
                                }
                                if(((infoU[fd].rank != RANK_ADMIN) && (isModeratorRoom(fd, roomid) == 0)) || (infoU[pid].rank == RANK_ADMIN && infoU[fd].rank != RANK_ADMIN))
                                {
                                    sendClientMessage(fd, "[ERRORE] Non puoi kickare questa persona dalla sua stanza.\n");
                                    break;
                                }
                                sprintf(format_text, "%s ha kickato %s dalla roomid %d (%s)\n", infoU[fd].name, infoU[pid].name, infoU[pid].room, infoR[infoU[pid].room].name);                 
                                sendServerMessage(format_text);
                                kickFromR(pid);
                                sendClientMessage(pid, "[INFO] Sei stato kickato dalla stanza in cui eri.\n");
                                showRooms(pid);
                                sendClientMessage(fd, "[INFO] L'utente è stato kickato dalla stanza in cui era.\n");
                            }
                            else if(!strcmp(cmd, "/setrpassword"))
                            {
                                if(infoU[fd].rank < RANK_ADVUSER)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non hai i permessi per utilizzare questo comando.\n");
                                    break;
                                }
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                int roomid = infoU[fd].room;
                                if(roomid == -1)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non sei in una stanza. Usa /join per accedere ad una.\n");
                                    break;
                                }
                                if(!isModeratorRoom(fd, roomid) && infoU[fd].rank != RANK_ADMIN)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non sei il moderatore di questa stanza.\n");
                                }
                                char t_password[32];
                                if(sscanf(text, "%*s %s", t_password) != 1)
                                {
                                    sendClientMessage(fd, "[USO] /setrpassword <password>\n");
                                    break;
                                }
                                if(safeString(t_password) == 0)
                                {
                                    sendClientMessage(fd, "[ERRORE] Formato password non valido.\n");
                                    break;
                                }
                                if(strlen(t_password) < 6 || strlen(t_password) > 20)
                                {
                                    sendClientMessage(fd, "[ERRORE] La password deve contenere da 6 a 20 caratteri.\n");
                                    break;
                                }
                                strcpy(infoR[roomid].password, t_password);
                                saveRoomData(roomid);
                                sendClientMessage(fd, "[INFO] La password della stanza è stata cambiata.\n");                                
                            }
                            else if(!strcmp(cmd, "/banip"))
                            {
                                if(infoU[fd].rank != RANK_ADMIN)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non hai i permessi per utilizzare questo comando.\n");
                                    break;
                                }
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                char name[16];
                                if(sscanf(text, "%*s %s", name) != 1)
                                {
                                    sendClientMessage(fd, "[USO] /banip <nome>\n");
                                    break;
                                }
                                int pid = getIDFromName(name);
                                if(pid == -1)
                                {
                                    sendClientMessage(fd, "[ERRORE] Questa persona non risulta essere connessa.\n");
                                    break;
                                }
                                if(pid == fd)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non puoi bannare te stesso.\n");
                                    break;
                                }                
                                sprintf(format_text, "%s ha bannato %s per IP (%s)\n", infoU[fd].name, infoU[pid].name, infoU[pid].IP);
                                sendServerMessage(format_text);
                                BanIP(infoU[pid].IP);
                                sendClientMessage(pid, "[INFO] Sei stato bannato dal server.\n");
                                sendClientMessage(fd, "[INFO] L'account è stato bannato dal server.\n");
                                kickFromS(pid);
                            }
                            else if(!strcmp(cmd, "/kick"))
                            {
                                if(infoU[fd].rank != RANK_ADMIN)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non hai i permessi per utilizzare questo comando.\n");
                                    break;
                                }
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                char name[16];
                                if(sscanf(text, "%*s %s", name) != 1)
                                {
                                    sendClientMessage(fd, "[USO] /kick <nome>\n");
                                    break;
                                }
                                int pid = getIDFromName(name);
                                if(pid == -1)
                                {
                                    sendClientMessage(fd, "[ERRORE] Questa persona non risulta essere connessa.\n");
                                    break;
                                }
                                if(pid == fd)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non puoi kickare te stesso.\n");
                                    break;
                                }         
                                sprintf(format_text, "%s ha kickato %s\n", infoU[fd].name, infoU[pid].name);
                                sendServerMessage(format_text);
                                sendClientMessage(pid, "[INFO] Sei stato kickato dal server.\n");
                                sendClientMessage(fd, "[INFO] L'utente è stato kickato dal server.\n");
                                kickFromS(pid);
                            }
                            else if(!strcmp(cmd, "/setrank"))
                            {
                                if(infoU[fd].rank != RANK_ADMIN)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non hai i permessi per utilizzare questo comando.\n");
                                    break;
                                }
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                char name[16];
                                int rank;
                                if(sscanf(text, "%*s %s %d", name, &rank) != 2)
                                {
                                    sendClientMessage(fd, "[USO] /setrank <nome> <rank>\n");
                                    break;
                                }
                                int pid = getIDFromName(name);
                                if(pid == -1)
                                {
                                    sendClientMessage(fd, "[ERRORE] Questa persona non risulta essere connessa.\n");
                                    break;
                                }
                                if(rank < 1 || rank > 3)
                                {
                                    sendClientMessage(fd, "[ERRORE] Puoi settare un rank che va da 1 a 3.\n");
                                    break;
                                }
                                infoU[pid].rank = rank;
                                saveUserData(pid);
                                sprintf(format_text, "[INFO] L'admin %s ha settato il tuo rank a %d.\n", infoU[fd].name, rank);
                                sendClientMessage(pid, format_text);
                                sendClientMessage(fd, "[INFO] Il rank dell'utente è stato cambiato.\n");
                                sprintf(format_text, "%s ha settato il rank di %s a %d.\n", infoU[fd].name, infoU[pid].name, rank);
                                sendServerMessage(format_text);
                            }
                            else if(!strcmp(cmd, "/broadcast"))
                            {
                                if(infoU[fd].rank != RANK_ADMIN)
                                {
                                    sendClientMessage(fd, "[ERRORE] Non hai i permessi per utilizzare questo comando.\n");
                                    break;
                                }
                                if(infoU[fd].logged == FALSE)
                                {
                                    sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di utilizzare un comando.\n");
                                    break;
                                }
                                char msg[256];
                                if(sscanf(text, "%*s %[^\n] \n", msg) != 1)
                                {
                                    sendClientMessage(fd, "[USO] /broadcast <messaggio>\n");
                                    break;
                                }
                                char format_text[256];
                                sprintf(format_text, "[BROADCAST (%s)] %s\n", infoU[fd].name, msg);
                                sendClientMessage(fd, "[INFO] Il messaggio è stato inviato.\n");
                                sendToOtherClients(fd, format_text);
                                sendAllMessage(server_sockid, format_text);
                            }
                            else
                                sendClientMessage(fd, "[ERRORE] Comando inesistente.\n");
                        }
                        else
                        {
                            if(infoU[fd].logged == FALSE)
                            {
                                sendClientMessage(fd, "[ERRORE] Devi loggarti con /login prima di scrivere qualcosa.\n");
                                break;
                            }
                            if(infoU[fd].room == -1)
                            {
                                sendClientMessage(fd, "[ERRORE] Devi entrare in una stanza per poter scrivere.\n");
                                break;
                            }
                            sprintf(format_text, "%s: %s\n", infoU[fd].name, text);
                            sendRoomMessage(-1, infoU[fd].room, format_text);
                            sprintf(format_text, "(%s [%d]) %s: %s\n", infoR[infoU[fd].room].name, infoU[fd].room, infoU[fd].name, text);
                            sendServerMessage(format_text);
                        }
                    }
                }
            }
        }
    }
}

void toggleCharacter(char *str)
{
    int len = strlen(str);
    str[len - 1] = '\0';
}

void sendToOtherClients(int senderid, char *text)
{
    for(int fd = 0; fd < FD_SETSIZE; fd++)
        if(FD_ISSET(fd, &readfds))
            if((fd != senderid) && (fd != server_sockid))
                sendClientMessage(fd, text);
}

void sendAllMessage(int serverid, char *text)
{
    for(int fd = 0; fd < FD_SETSIZE; fd++)
        if(FD_ISSET(fd, &readfds))
            if(fd != serverid)
                sendClientMessage(fd, text);
}

void sendClientMessage(int clientid, char *text)
{
    char t_text[256];    
    strcpy(t_text, text);
    send(clientid, t_text, strlen(t_text) + 1, 0);
}

void sendRoomMessage(int senderid, int roomid, char *text)
{
    for(int fd = 0; fd < FD_SETSIZE; fd ++)
        if(FD_ISSET(fd, &readfds))
            if(fd != server_sockid)
                if(fd != senderid && infoU[fd].room == roomid)
                    sendClientMessage(fd, text);
}

void sendServerMessage(char *text)
{
    char output[128];
    time_t rawtime;
    struct tm * timeinfo;

    time (&rawtime);
    timeinfo = localtime (&rawtime); 
    sprintf(output, "[%d/%d/%d %d:%d:%d]",timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    FILE *fd_log = fopen(LOG_PATH, "a");
    printf("%s", text);
    fprintf(fd_log, "%s %s", output, text);
    fclose(fd_log);
}

int getIDFromName(char *name)
{
    for(int fd = 0; fd < FD_SETSIZE; fd++)
        if(FD_ISSET(fd, &readfds))
            if(!strcmp(infoU[fd].name, name))
                return fd;
    return -1;
}

int safeString(char *str)
{
    for(int i = 0; i < strlen(str); i++)
        if(str[i] == ',' || str[i] == '\\')
            return 0;
    return 1;
}

void kickFromR(int clientid)
{
    int roomid = infoU[clientid].room;
    if(roomid == -1)
        return;
    infoR[roomid].clients --;
    infoU[clientid].room = -1;
}

void kickFromS(int clientid)
{
    kickFromR(clientid);
    close(clientid);
    FD_CLR(clientid, &readfds);
}

int isValidRoom(int roomid)
{
    if(!strlen(infoR[roomid].name))
        return 0;
    else
        return 1;
}

int availableRoom()
{
    for(int i = 0; i < MAX_ROOM; i++)
        if(!isValidRoom(i))
            return i;
    return -1;
}

int isPublicRoom(int roomid)
{
    if(!strcmp(infoR[roomid].password, "public"))
        return 1;
    else
        return 0;
}

int isFullRoom(int roomid)
{
    return infoR[roomid].clients == infoR[roomid].slot;
}

int isModeratorRoom(int clientid, int roomid)
{
    if(!strcmp(infoR[roomid].moderator, infoU[clientid].name))
        return 1;
    return 0;
}

void showRooms(int clientid)
{
    char t_text[2048];
    char state[16];
    int found = FALSE;
    strcpy(t_text, "\n\t\t\t\tSTANZE\n");
    sendClientMessage(clientid, t_text);
    for(int i = 0; i < MAX_ROOM; i++)
        if(isValidRoom(i))
        {
            if(!strcmp(infoR[i].password, "public"))
                sprintf(state, "Pubblica");
            else
                sprintf(state, "Privata");
            sprintf(t_text, "\nStanza: %s (ID: %d)\nStato: %s\nModeratore: %s\nPersone: %d/%d\n", infoR[i].name, i, state, infoR[i].moderator, infoR[i].clients, infoR[i].slot);
            sendClientMessage(clientid, t_text);
            found = TRUE;
        } 
    if(found == FALSE)
        sendClientMessage(clientid, "\nNon ci sono stanze disponibili.\n\n");
    else
        sendClientMessage(clientid, "\n[INFO] Utilizza il comando /join per accedere ad una stanza.\n[INFO] Utilizza il comando /refresh per aggiornare la lista delle stanze.\n\n");
}

void BanIP(char *IP)
{
    FILE *fd;
    fd = fopen(BAN_PATH, "a");
    fprintf(fd, "%s\n", IP);
    fclose(fd);
}

int isIPBanned(char *IP)
{
    FILE *fd;
    char tmp[13];
    fd = fopen(BAN_PATH, "a+");
    while(fscanf(fd, "%s", tmp) == 1)
        if(!strcmp(IP, tmp))
            return 1;
    return 0;
}

int clientsNumberPerIP(char *IP)
{
    int q = 0;
    for(int fd = 0; fd < FD_SETSIZE; fd++)
        if(FD_ISSET(fd, &readfds))
            if(!strcmp(infoU[fd].IP, IP))
                q++;
    return q;
}
                                        /* ROOM DATA FUNCTIONS */

int loadRoomData(int roomid, char *name)
{
    FILE *fd;
    char t_name[64], t_password[32], t_moderator[16];
    int t_slot;
    fd = fopen(ROOMS_PATH, "a+");
    while(fscanf(fd, "%[^,] , %[^,] , %[^,] , %d \n", t_name, t_password, t_moderator, &t_slot) == 4)
    {
        if(!strcmp(t_name, name))
        {
            strcpy(infoR[roomid].name, t_name);
            strcpy(infoR[roomid].password, t_password);
            strcpy(infoR[roomid].moderator, t_moderator);
            infoR[roomid].slot = t_slot;
            fclose(fd);
            return 1;
        }
    }
    fclose(fd);
    return 0;
}

void loadAllRoomData()
{
    FILE *fd;
    char t_name[64], t_password[32], t_moderator[16];
    int t_slot;
    int roomid = 0;
    fd = fopen(ROOMS_PATH, "a+");
    while(fscanf(fd, "%[^,] , %[^,] , %[^,] , %d \n", t_name, t_password, t_moderator, &t_slot) == 4)
    {
        strcpy(infoR[roomid].name, t_name);
        strcpy(infoR[roomid].password, t_password);
        strcpy(infoR[roomid].moderator, t_moderator);
        infoR[roomid].slot = t_slot;
        infoR[roomid].clients = 0;
        roomid ++;
        if(roomid == MAX_ROOM)
            break;
    }
    fclose(fd);
}

void saveRoomData(int roomid)
{
    FILE *fd_read, *fd_write;
    char t_name[64], t_password[32], t_moderator[16];
    int t_slot;
    int found = FALSE;
    fd_read = fopen(ROOMS_PATH, "a+");
    fd_write = fopen("tmpr.txt", "w");
    while(fscanf(fd_read, "%[^,] , %[^,] , %[^,] , %d \n", t_name, t_password, t_moderator, &t_slot) == 4)
    {
        if(!strcmp(t_name, infoR[roomid].name))
        {
            fprintf(fd_write, "%s,%s,%s,%d\n", infoR[roomid].name, infoR[roomid].password, infoR[roomid].moderator, infoR[roomid].slot);
            found = TRUE;
        }
        else
            fprintf(fd_write, "%s,%s,%s,%d\n", t_name, t_password, t_moderator, t_slot);
    }
    if(found == FALSE)
        fprintf(fd_write, "%s,%s,%s,%d\n", infoR[roomid].name, infoR[roomid].password, infoR[roomid].moderator, infoR[roomid].slot);
    fclose(fd_read);
    fclose(fd_write);
    remove(ROOMS_PATH);
    rename("tmpr.txt", ROOMS_PATH);
}

void deleteRoomData(int roomid)
{
    FILE *fd_read, *fd_write;
    char t_name[64], t_password[32], t_moderator[16];
    int t_slot;
    fd_read = fopen(ROOMS_PATH, "a+");
    fd_write = fopen("tmpr.txt", "w");
    while(fscanf(fd_read, "%[^,] , %[^,] , %[^,] , %d \n", t_name, t_password, t_moderator, &t_slot) == 4)
    {
        if(strcmp(t_name, infoR[roomid].name) != 0)
            fprintf(fd_write, "%s,%s,%s,%d\n", t_name, t_password, t_moderator, t_slot);
    }
    fclose(fd_read);
    fclose(fd_write);
    remove(ROOMS_PATH);
    rename("tmpr.txt", ROOMS_PATH);
    strcpy(infoR[roomid].name, "");
}

                                        /* CLIENT DATA FUNCTIONS */

void initClient(int clientid, struct sockaddr_in c_address)
{
    infoU[clientid].rank = RANK_NONUSER;
    infoU[clientid].logged = FALSE;
    infoU[clientid].room = -1;                   
    strcpy(infoU[clientid].IP, inet_ntoa(c_address.sin_addr));
    infoU[clientid].realClient = 0;
}

int controlClient(int clientid)
{
    char buffer[128];
    if(safeString(infoU[clientid].name) == 0)
    {
        sendClientMessage(clientid, "Il formato del nickname non è valido.\n\n");
        return 0;
    }
    if(isIPBanned(infoU[clientid].IP))
    {
        sendClientMessage(clientid, "Sei stato bannato dal server.\n\n");
        return 0;
    }
    return 1;
}

int loadUserData(int clientid, char *name)
{
    FILE *fd;
    char t_name[16], t_password[32];
    int t_rank;
    fd = fopen(USERS_PATH, "a+");
    while(fscanf(fd, "%[^,] , %[^,] , %d \n", t_name, t_password, &t_rank) == 3)
    {
        if(!strcmp(t_name, name))
        {
            strcpy(infoU[clientid].password, t_password);
            infoU[clientid].rank = t_rank;
            fclose(fd);
            return 1;
        }
    }
    fclose(fd);
    return 0;
}

void saveUserData(int clientid)
{
    FILE *fd_read, *fd_write;
    char t_name[16], t_password[32];
    int t_rank;
    int found = FALSE;
    fd_read = fopen(USERS_PATH, "a+");
    fd_write = fopen("tmpu.txt", "w");
    while(fscanf(fd_read, "%[^,] , %[^,] , %d \n", t_name, t_password, &t_rank) == 3)
    {
        if(!strcmp(t_name, infoU[clientid].name))
        {
            fprintf(fd_write, "%s,%s,%d\n", infoU[clientid].name, infoU[clientid].password, infoU[clientid].rank);
            found = TRUE;
        }
        else
            fprintf(fd_write, "%s,%s,%d\n", t_name, t_password, t_rank);
    }
    if(found == FALSE)
        fprintf(fd_write, "%s,%s,%d\n", infoU[clientid].name, infoU[clientid].password, infoU[clientid].rank);
    fclose(fd_read);
    fclose(fd_write);
    remove(USERS_PATH);
    rename("tmpu.txt", USERS_PATH);
}

int deleteUserData(char *name)
{
    FILE *fd_read, *fd_write;
    char t_name[16], t_password[32];
    int t_rank;
    int found = FALSE;
    fd_read = fopen(USERS_PATH, "a+");
    fd_write = fopen("tmp.txt", "w");
    while(fscanf(fd_read, "%[^,] , %[^,] , %d \n", t_name, t_password, &t_rank) == 3)
    {
        if(!strcmp(t_name, name))
        {
            found = TRUE;
            continue;
        }
        else
            fprintf(fd_write, "%s,%s,%d\n", t_name, t_password, t_rank);
    }
    fclose(fd_read);
    fclose(fd_write);
    remove(USERS_PATH);
    rename("tmp.txt", USERS_PATH);
    if(found)
        return 1;
    else
        return 0;
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