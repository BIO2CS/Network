/**
 * CSE589 Project 1
 * Name:   Huiqiong Gu
 * UBIT:   hgu
 * Person: #35108523
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

#define true 1
#define false 0
#define STDIN 0
#define STDOUT 1
#define STDERR 2
#define MAXCONNECTION 5
#define PACKETSIZE 1024
#define SERVERNAME "timberlake.cse.buffalo.edu"

extern int h_errno;
typedef int bool;

void bail(const char *on_what);
void runCommand(char **argv);
void connectToHost(char *destination, int port);
char *findHostName(int sockfd);
void encodeMessage(char *buffer, char header, char *name, char *IP, int port);
void sendDownloadRequest(char *fileName, int fileSize, int selfsockfd);
void decodeMessage(char *buffer, char *name, char *IP, int *port);
void removeFromConnectionList(int ID);
void saveToConnectionList(char *hostName, char *hostIP, int hostPort);
void downloadFile(char *fileName, int chunkSize);
int findID(char *hostName);
void decodeDownloadMessage(char *buffer, char *fileName, int *startSize, int *chunkSize);
void encodeDownloadMessage(char *buffer, char *fileName, int startSize, int chunkSize);
void hostExit(void);
void registerWithServer(char *serverIP, int port);
void terminateConnection(int ID);
void listConnection(void);
void getIP(void);
void displayCommands();
void sendUpdateListToHost(char *hostname, char *hostIP, int port, int socket);

//These are constantly used variables. I set them as global to avoid passing them around.
int maxfd = 3;
int numberOfConnection = 0;
int numberOfID = 0;
fd_set masterSet;
fd_set tempSet;
bool isRegistered = false;
char MYIP[64];
char MYNAME[64];
int MYPORT;
int MYSOCKFD;
int CHUNKSIZE;

//connection list server/client maintains
typedef struct host{
    int ID;
    char hostname[128];
    char IP[64];
    int port;
}hostInfo;
hostInfo connectionList[MAXCONNECTION];

int main(int argc, char **argv){
    if(argc != 3){
        fprintf(stderr, "Usage: proj1 [Server/Client] [PortNumber]\n\n");
        exit(1);
    }
    int i, z;
    struct sockaddr_in addr;
    gethostname(MYNAME, sizeof MYNAME);
    getIP();
    MYPORT = atoi(argv[2]);
    MYSOCKFD = socket(AF_INET, SOCK_STREAM, 0);
    if(MYSOCKFD == -1){
        bail("socket()");
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));
    if(addr.sin_addr.s_addr == INADDR_NONE){
        bail("bad address.");
    }
    z = bind(MYSOCKFD, (struct sockaddr *) &addr, sizeof addr);
    if(z == -1){
        bail("bind()");
    }
    z = listen(MYSOCKFD, MAXCONNECTION);
    if(z == -1){
        bail("listen()");
    }
    FD_ZERO(&masterSet);
    FD_SET(STDIN, &masterSet);
    FD_SET(STDOUT, &masterSet);
    FD_SET(STDERR, &masterSet);
    FD_SET(MYSOCKFD, &masterSet);
    maxfd = MYSOCKFD + 1;
    while(true){
        FD_ZERO(&tempSet);
        tempSet = masterSet;
        printf("proj1 >> ");
        fflush(stdout);
        z = select(maxfd, &tempSet, NULL, NULL, NULL);
        if(z == -1){
            bail("select()");
        }
        if(FD_ISSET(STDIN, &tempSet)){
            runCommand(argv);
        }
        if(FD_ISSET(MYSOCKFD, &tempSet)){
            //int z;
            int newSock;
            struct sockaddr_in clientAddr;
            //struct hostent *hp;
            socklen_t addrlen;
            addrlen = sizeof clientAddr;
            char buffer[256];
            size_t bytesReceived;
            newSock = accept(MYSOCKFD, (struct sockaddr *) &clientAddr, &addrlen);
            if(newSock == -1){
                bail("accept()");
            }
            FD_SET(newSock, &masterSet);
            if(maxfd <= newSock){
                maxfd = newSock + 1;
            }
            numberOfConnection++;
            //immediate upon accept(), client receives the connecting host's info message(name ,ip, port)
            if(strcmp(argv[1], "c") == 0){
                char connectedClientName[128];
                char connectedClientIP[64];
                int connectedClientPort = 0;
                memset(&buffer, 0, sizeof(buffer));
                bytesReceived = recv(newSock, buffer, sizeof(buffer), 0);
                if(bytesReceived < 0){
                    bail("recv()");
                }
                char temp[256];
                strcpy(temp, buffer);
                char header = temp[0];
                if(header == 'C'){
                    printf("Peer connection request\n\n");
                }
                decodeMessage(temp, connectedClientName, connectedClientIP, &connectedClientPort);
                saveToConnectionList(connectedClientName, connectedClientIP, connectedClientPort);
                printf("Peer %s has successfully connected to me\n\n", connectedClientName);
            }
            //immediate upon accept(), server receives the connecting client's info message(name ,ip, port)
            if(strcmp(argv[1], "s") == 0){
                int id;
                char registeredClientName[128];
                char registeredClientIP[64];
                int registeredClientPort = 0;
                memset(&buffer, 0, sizeof(buffer));
                bytesReceived = recv(newSock, buffer, sizeof(buffer), 0);
                if(bytesReceived < 0){
                    bail("recv()");
                }
                char temp[256];
                strcpy(temp, buffer);
                char header = temp[0];
                if(header == 'R'){
                    printf("register request\n\n");
                }
                decodeMessage(temp, registeredClientName, registeredClientIP, &registeredClientPort);
                saveToConnectionList(registeredClientName, registeredClientIP, registeredClientPort);
                printf("Client %s has successfully registered\n\n", registeredClientName);
                sendUpdateListToHost(registeredClientName, registeredClientIP, registeredClientPort, newSock);
                printf("After %s has registered. The new server list:\n", registeredClientName);
                for(id = 0; id < numberOfID; id++){
                    printf("%d\t", connectionList[id].ID);
                    printf("%s\t", connectionList[id].hostname);
                    printf("%s\t", connectionList[id].IP);
                    printf("%d\n\n", connectionList[id].port);
                }
            }
        }
        /**run through the existing connections except listening socket for data transmission
         * depend on whether I am a server or a client and received messages, respond accordingly
         */
        for(i = 3; i < maxfd; i++){
            if(FD_ISSET(i, &tempSet)){
                if(i == MYSOCKFD){
                    continue;
                }
                char buffer[256];
                size_t bytesReceived;
                if(strcmp(argv[1], "c") == 0){
                    memset(&buffer, 0, sizeof(buffer));
                    bytesReceived = recv(i, buffer, sizeof(buffer), 0);
                    if(bytesReceived < 0){
                        perror("recv()");
                    }
                    char header = buffer[0];
                    //client receives the update message from server whenever new client registers with server
                    if(header == 'R'){
                        printf("Received register update from server\n\n");
                        char newJoinedClientName[128];
                        char newJoinedClientIP[64];
                        int newJoinedClientPort = 0;
                        decodeMessage(buffer, newJoinedClientName, newJoinedClientIP, &newJoinedClientPort);
                        printf("New client has registered with server: \n\t%s\t%s\t%d\n\n",
                                newJoinedClientName, newJoinedClientIP, newJoinedClientPort);
                    }
                    //someone terminated connection with me
                    if(header == 'T'){
                        printf("Received termination message from peer\n\n");
                        int j, terminatingID;
                        char terminatingHostName[128];
                        char terminatingHostIP[64];
                        int terminatingHostPort = 0;
                        decodeMessage(buffer, terminatingHostName, terminatingHostIP, &terminatingHostPort);
                        printf("%s has terminated connection with me on my socket%d\n\n", terminatingHostName, i);
                        for(j = 0; j < numberOfID; j++){
                            if(strcmp(connectionList[j].hostname, terminatingHostName) == 0){
                                break;
                            }
                        }
                        //error handling, not necessarily needed
                        if(j == numberOfID){
                            fprintf(stderr, "There is no %s in my connection list. Something is wrong\n\n", terminatingHostName);
                            break;
                        }
                        //close my connection with the terminating peer. update maxfd and connectionlist
                        close(i);
                        FD_CLR(i, &masterSet);
                        if(i == maxfd - 1){
                            int k;
                            for(k = i - 1; k >= 2; k--){
                                if(FD_ISSET(k, &masterSet)){
                                    maxfd = k + 1;
                                    break;
                                }
                            }
                        }
                        numberOfConnection--;
                        terminatingID = connectionList[j].ID;
                        removeFromConnectionList(terminatingID);
                    }
                    //'$' sign suggests file size request from a downloading client
                    if(header == '$'){
                        char fileName[128];
                        char peerName[64];
                        char sentBuffer[256];
                        FILE *fp;
                        int j, k, fileSize, z;
                        int pack = 0;
                        decodeMessage(buffer, fileName, peerName, &pack);
                        printf("Received request from %s for size of %s\n\n", peerName, fileName);
                        //for text or binary file
                        if((fp = fopen(fileName, "r+")) == NULL || (fp = fopen(fileName, "rb+")) == NULL ){
                            bail("fopen()");
                        }
                        fseek(fp, 0L, SEEK_END);
                        fileSize = ftell(fp);
                        fclose(fp);
                        encodeMessage(sentBuffer, '*', fileName, MYNAME, fileSize);
                        z = send(i, sentBuffer, sizeof(sentBuffer), 0);
                        if(z < 0){
                            bail("send()");
                        }
                        printf("Sent file size of \"%s\" to %s\n\n", fileName, peerName);
                    }
                    //downloading client receives the file size message (header is '*')
                    if(header == '*'){
                        char receivedFileName[128];
                        char fromPeerName[64];
                        int fileSize = 0;
                        decodeMessage(buffer, receivedFileName, fromPeerName, &fileSize);
                        printf("Received file size from %s\n", fromPeerName);
                        printf("The size of \"%s\" is %d bytes\n\n", receivedFileName, fileSize);
                        sendDownloadRequest(receivedFileName, fileSize, i);
                    }
                    //peers receive downloading message, start reading file and sending it in chunks
                    if(header == 'D'){
                        char fileName[128];
                        int startSize = 0;
                        int chunkSize = 0;
                        decodeDownloadMessage(buffer, fileName, &startSize, &chunkSize);
                        printf("Received file download message, chunksize is %d\n\n", chunkSize);
                        FILE *fp;
                        int k, r, s;
                        int readBytes = 0;
                        int sendBytes = 0;
                        int temp;
                        int numberOfDigits = 0;
                        int totalLength;
                        char sentBuffer[1200];
                        char fileContent[1024];

                        if((fp = fopen(fileName, "r+")) == NULL || (fp = fopen(fileName, "rb+")) == NULL){
                            bail("fopen()");
                        }
                        //seek to the start byte position and read from there for CHUNKSIZE bytes
                        fseek(fp, startSize, SEEK_SET);
                        memset(sentBuffer, 0, sizeof(sentBuffer));
                        temp = startSize;
                        while(temp > 0){
                            temp /= 10;
                            numberOfDigits++;
                        }
                        temp = startSize;
                        if(numberOfDigits == 0){
                            sentBuffer[0] = '0';
                            numberOfDigits++;
                        }
                        else{
                            k = numberOfDigits;
                            while(temp > 0){
                                sentBuffer[k - 1] = (char) (temp % 10 + '0');
                                temp /= 10;
                                k--;
                            }
                        }
                        sentBuffer[numberOfDigits] = '|';
                        totalLength = 1 + numberOfDigits;
                        printf("Start reading file\n\n");
                        r = fread(fileContent, sizeof(char), chunkSize, fp);
                        strcpy(sentBuffer + totalLength, fileContent);
                        totalLength += r;
                        printf("Bytes of file read: %d\n\n", r);
                        //send message looks like this: "start position(in bytes)|fileContent"
                        if(r > 0){
                            memset(sentBuffer + totalLength, 0, 1200 - totalLength);
                            printf("Start sending file\n\n");
                            if((s = send(i, sentBuffer, sizeof(sentBuffer), 0)) < 0){
                                fprintf(stderr, "Transfering file fails\n\n");
                                bail("send()");
                            }
                            memset(sentBuffer, 0, sizeof(sentBuffer));
                            int remainBytes = r - s;
                            if(remainBytes > 0){
                                s = send(i, buffer, remainBytes, 0);
                                if(s < 0){
                                    fprintf(stderr, "Transfering file fails\n\n");
                                    bail("send()");
                                }
                            }
                            memset(sentBuffer, 0, sizeof(sentBuffer));
                            printf("Finished sending %d bytes\n\n", chunkSize);
                        }
                        else if(r == 0){
                            //end of file, send a '&' sign
                            memset(sentBuffer, 0, sizeof(sentBuffer));
                            strcpy(sentBuffer, "&");
                            memset(sentBuffer + 1, 0, 1119);
                            printf("End of file. Sent\n\n");
                            if((s = send(i, sentBuffer, sizeof(sentBuffer), 0)) < 0){
                                bail("send()");
                            }
                        }
                        else{
                            bail("read()");
                        }
                        if(fp){
                            fclose(fp);
                        }
                    }
                    //If one client exits, he sends the message to the server. Then server notifies all other clients
                    if(header == 'E'){
                        printf("received message from server. someone has exited\n\n");
                        int j, exitedPeerID;
                        char exitedPeerName[128];
                        char exitedPeerIP[64];
                        int exitedPeerPort = 0;
                        decodeMessage(buffer, exitedPeerName, exitedPeerIP, &exitedPeerPort);
                        for(j = 0; j < numberOfID; j++){
                            if(strcmp(connectionList[j].hostname, exitedPeerName) == 0){
                                break;
                            }
                        }
                        //if not in my connectionlist, do nothing
                        if(j == numberOfID){
                            printf("Exited client %s is not in my connection list. No need to update\n\n", exitedPeerName);
                        }
                        else{
                            //if in my connectinolist, close connection, update my maxfd and connectionlist
                            close(i);
                            FD_CLR(i, &masterSet);
                            if(i == maxfd - 1){
                                int k;
                                for(k = i - 1; k >= 2; k--){
                                    if(FD_ISSET(k, &masterSet)){
                                        maxfd = k + 1;
                                        break;
                                    }
                                }
                            }
                            numberOfConnection--;
                            exitedPeerID = connectionList[j].ID;
                            removeFromConnectionList(exitedPeerID);
                            printf("I have closed connection with the exited client %s\n\n", exitedPeerName);
                        }
                    }
                }
                //server handling communication
                if(strcmp(argv[1], "s") == 0){
                    int j, z, count = 0;
                    memset(&buffer, 0, sizeof(buffer));
                    bytesReceived = recv(i, buffer, sizeof(buffer), 0);
                    if(bytesReceived < 0){
                        perror("recv()");
                    }
                    char header = buffer[0];
                    //server receives exit message from an exiting client, broadcast exiting client info to others
                    if(header == 'E'){
                        char exitedClientName[128];
                        char exitedClientIP[64];
                        int exitedClientPort = 0;
                        int exitedClientID;
                        decodeMessage(buffer, exitedClientName, exitedClientIP, &exitedClientPort);
                        printf("Client %s has exited\n\n", exitedClientName);
                        for(j = 0; j < numberOfID; j++){
                            if(strcmp(connectionList[j].hostname, exitedClientName) == 0){
                                break;
                            }
                        }
                        //error handling in case something went wrong, not necessarily needed
                        if(j == numberOfID){
                            fprintf(stderr, "There is no %s in my connection list. Something is wrong\n\n", exitedClientName);
                            break;
                        }
                        //close connection with exiting client, update maxfd and connection list
                        close(i);
                        FD_CLR(i, &masterSet);
                        if(i == maxfd - 1){
                            int k;
                            for(k = i - 1; k >= 2; k--){
                                if(FD_ISSET(k, &masterSet)){
                                    maxfd = k + 1;
                                    break;
                                }
                            }
                        }
                        numberOfConnection--;
                        exitedClientID = connectionList[j].ID;
                        removeFromConnectionList(exitedClientID);
                        char sentBuffer[256];
                        //send a message to all other clients with header 'E'  and exiting client info
                        encodeMessage(sentBuffer, 'E', exitedClientName, exitedClientIP, exitedClientPort);
                        for(j = 3; j < maxfd; j++){
                            if(j == i || j == MYSOCKFD){
                                continue;
                            }
                            if(FD_ISSET(j, &masterSet)){
                                z = send(j, sentBuffer, sizeof(sentBuffer), 0);
                                if(z < 0){
                                    bail("send()");
                                }
                                count++;
                            }
                        }
                        printf("Sent exited client info to %d other clients\n\n", count);
                    }
                }
            }
        }
    }
    // close all connections
    for(i = 3; i < maxfd; i++){
        if(FD_ISSET(i, &masterSet)){
            close(i);
        }
    }
    return 0;
}

/**
 * the communication message b/w client/server using the following format:
 * character(R(register), C(connect))|string(filename,hostname)|string(IP)|int(port)
 */
void encodeMessage(char *buffer, char header, char *name, char *IP, int port){
    int i, temp, totalLength;
    int numberOfDigits = 0;
    int covertedPort = port;
    buffer[0] = header;
    buffer[1] = '|';
    strcpy(buffer + 2, name);
    buffer[2 + strlen(name)] = '|';
    strcpy(buffer + 3 + strlen(name), IP);
    buffer[3 + strlen(name) + strlen(IP)] = '|';

//manually convert each digit in the int(port) to the character in the sending buffer
    temp = covertedPort;
    if(temp == 0){
        totalLength = 4 + strlen(name) + strlen(IP) + 1;
        buffer[totalLength - 1] = '0';
    }
    else{
        while(temp > 0){
            temp /= 10;
            numberOfDigits++;
        }
        temp = covertedPort;
        totalLength = 4 + strlen(name) + strlen(IP) + numberOfDigits;
        i = totalLength;
        while(temp > 0){
            buffer[i - 1] = (char) (temp % 10 + '0');
            temp /= 10;
            i--;
        }
    }
    memset(buffer + totalLength, 0, sizeof(256 - totalLength));
}

//receiver side: extract the information from the receiving buffer
void decodeMessage(char *buffer, char *name, char *IP, int *port){
    int j = 2;
    int k = 0;
    while(buffer[j] != '|'){
        name[k] = buffer[j];
        j++;
        k++;
    }
    name[k] = '\0';
    j++;
    k = 0;
    while(buffer[j] != '|'){
        IP[k] = buffer[j];
        j++;
        k++;
    }
    IP[k] = '\0';
    j++;
    k = 0;
    while(buffer[j] != '\0'){
        *port = 10 * (*port) + (buffer[j] - '0');
        j++;
    }
}

//server sends newly registered client's info to all other clients
void sendUpdateListToHost(char *hostname, char *hostIP, int port, int socket){
    char buffer[256];
    encodeMessage(buffer, 'R', hostname, hostIP, port);
    int i, z, count = 0;
    for(i = 3; i < maxfd; i++){
        if(i == MYSOCKFD || i == socket){ //except myself's listening and connection socket
            continue;
        }
        if(FD_ISSET(i, &masterSet)){
            z = send(i, buffer, sizeof(buffer), 0);
            if(z < 0){
                bail("send()");
            }
            count++;
        }
    }
    printf("Successfully sent new client (%s) info to other %d clients\n\n", hostname, count);
}

void runCommand(char **argv){
    char *command;
    char *cmdCollect[20];
    char in[128];
    char *input;
    int commandCount = 0;
    int i;
    memset(cmdCollect, 0, sizeof(cmdCollect));
    char *p = fgets(in, sizeof in, stdin);
    if(p == NULL ){
        bail("fgets()");
    }
    in[strlen(in) - 1] = '\0';
    command = strtok(in, " ");
    i = 0;
    while(command != NULL && i < 20){
        cmdCollect[i] = (char *) malloc((strlen(command) + 1) * sizeof(char));
        strcpy(cmdCollect[i], command);
        cmdCollect[i][strlen(command)] = '\0';
        command = strtok(NULL, " ");
        i++;
    }
    if(cmdCollect == NULL || cmdCollect[0] == NULL){
        fprintf(stderr, "Error in command\n\n");
        return;
    }
    i = 0;
    while(cmdCollect[i] != NULL){
        i++;
        commandCount++;
    }
    char **cmds = (char **) malloc(commandCount * sizeof(char *));
    for(i = 0; i < commandCount; i++){
        cmds[i] = (char *) malloc((strlen(cmdCollect[i]) + 1) * sizeof(char));
        strcpy(cmds[i], cmdCollect[i]);
        cmds[i][strlen(cmdCollect[i])] = '\0';
    }
    input = cmds[0];
    for(i = 0; input[i] != '\0'; i++){
        input[i] = toupper(input[i]);
    }

    while(true){
        if(strcmp(input, "HELP") == 0){
            displayCommands();
            break;
        }
        else if(strcmp(input, "CREATOR") == 0){
            printf("Name: Huiqiong Gu\tUBIT: hgu\tEmail: hgu@buffalo.edu\n\n");
            fflush(stdout);
            break;
        }
        else if(strcmp(input, "MYIP") == 0){
            printf("Local IP address is: %s\n\n", MYIP);
            fflush(stdout);
            break;
        }
        else if(strcmp(input, "MYPORT") == 0){
            printf("Local port number is %d\n\n", MYPORT);
            fflush(stdout);
            break;
        }
        else if(strcmp(input, "REGISTER") == 0){
            if(strcmp(argv[1], "s") == 0){
                fprintf(stderr, "This is server and does not need to register! --- Try again\n\n");
                break;
            }
            if(commandCount != 3){
                fprintf(stderr, "REGISTER <serverIP> <port> --- Try again\n\n");
                break;
            }
            char *serverIP = cmds[1];
            int port = atoi(cmds[2]);
            registerWithServer(serverIP, port);
            isRegistered = true;
            break;
        }
        else if(strcmp(input, "CONNECT") == 0){
            if(strcmp(argv[1], "s") == 0){
                fprintf(stderr, "This is server. Only clients connect to peers --- Try again\n\n");
                break;
            }
            if(commandCount != 3){
                fprintf(stderr, "CONNECT <destination> <port> --- Try again\n\n");
                break;
            }
            if(!isRegistered){
                fprintf(stderr, "You need to register with server before connecting to peers\n\n");
                break;
            }
            if(numberOfConnection >= MAXCONNECTION - 1){
                fprintf(stderr, "Maximum peer connection is %d!\n\n", MAXCONNECTION - 1);
                break;
            }
            char *destination = cmds[1];
            int port = atoi(cmds[2]);
            connectToHost(destination, port);
            break;

        }
        else if(strcmp(input, "LIST") == 0){
            listConnection();
            break;
        }
        else if(strcmp(input, "TERMINATE") == 0){
            if(commandCount != 2){
                fprintf(stderr, "TERMINATE <connection id> --- Try again\n\n");
                break;
            }
            int ID = atoi(cmds[1]);
            if(ID < 1 || ID > numberOfID){
                fprintf(stderr, "Connection does not exist.\n\n");
            }
            if(strcmp(argv[1], "c") == 0 && ID == 1){
                fprintf(stderr, "Client cannot terminate connection with server.\n\n");
                break;
            }
            terminateConnection(ID);
            //isRegistered = false;
            break;
        }
        else if(strcmp(input, "EXIT") == 0){
            hostExit();
        }
        else if(strcmp(input, "DOWNLOAD") == 0){
            if(commandCount != 3){
                fprintf(stderr, "DOWNLOAD <file_name> <chunk_size> --- Try again\n\n");
                break;
            }
            if(strcmp(argv[1], "s") == 0){
                fprintf(stderr, "This is server and does not have this option. --- Try again\n\n");
                break;
            }
            if(numberOfConnection == 1){
                fprintf(stderr, "Not connected to any peer yet. Cannot dowload.\n\n");
                break;
            }
            char *fileName = cmds[1];
            int chunkSize = atoi(cmds[2]);
            downloadFile(fileName, chunkSize);
            break;
        }
        else{
            fprintf(stderr, "Unrecognized command. Try again\n\n");
            fflush(stdout);
            break;
        }
    }
    for(i = 0; i < commandCount; i++){
        free(cmds[i]);
        free(cmdCollect[i]);
    }
    free(cmds);
}

//function to get hostname from the socket fd
char *findHostName(int sockfd) {
    struct sockaddr_in peerAddr;
    socklen_t addrlen;
    struct hostent *hp;
    int z;
    if(FD_ISSET(sockfd, &masterSet)){
        memset(&peerAddr, 0, sizeof peerAddr);
        addrlen = sizeof peerAddr;
        z = getpeername(sockfd, (struct sockaddr*) &peerAddr, &addrlen);
        if(z < 0){
            bail("getpeername()");
        }
        hp = gethostbyaddr((char *) &peerAddr.sin_addr,
                sizeof peerAddr.sin_addr, peerAddr.sin_family);
        if(!hp){
            fprintf(stderr, " Error: %s\n", hstrerror(h_errno));
            return "ERROR";
        }
        return hp->h_name;
    }
    else{
        return "ERROR";
    }
}

/**
 * real download work is done in this function
 **********************************************************************************************
 * Bug in this function:
 * Download request is sent to all peers through the sockets except "selfsockfd";
 * This is wrong, "selfsockfd" is the socket fd through which I received file size;
 * This bug causes me not able to download file from the peer from which I got the file size.
 ***********************************************************************************************
 */
void sendDownloadRequest(char *fileName, int fileSize, int selfsockfd){
    int numberOfRequest;
    bool *array;
    int startSize;
    FILE *fp;
    int j, k, h, l, z;
    struct sockaddr_in peerAddr;
    socklen_t addrlen;
    struct hostent *hp;
    struct timeval t1;
    struct timeval t2;
    long double elapsed;
    char sentBuffer[256];
    char receivedBuffer2[1200]; // 1024 bytes (PACKETSIZE) for file content
    char fileContent[PACKETSIZE];
    int bytesDownloaded = 0;
    int bytesReceived;
    int fileBytes;
    int startIndex;
    int index;
    bool available[maxfd];
    int count = 0;
    int chunkValue[maxfd][1000]; //record each chunk values received from each socket fd;
    int chunkNumberReceived[maxfd]; //record chunk number received from each socket fd;
    int serverSockfd;

    numberOfRequest = fileSize / CHUNKSIZE;
    if((fp = fopen(fileName, "w+")) == NULL || (fp = fopen(fileName, "wb+")) == NULL){
        bail("fopen()");
    }
/**
 * create an array of size(filezize/chunksize), all initialized to zero,
 * once finished downloading one chunk, set it to 1
 */
    array = (bool *) malloc(numberOfRequest * sizeof(bool));
    for(j = 0; j < numberOfRequest; j++){
        array[j] = 0;
    }
/**
 * using an array identifying whether the socket is available.
 * If yes, send another downloading message through it. If not, probably still downloading
 */
    for(k = 3; k < maxfd; k++){
        available[k] = 1;
        chunkNumberReceived[k] = 0;
    }

    j = 0;
    gettimeofday(&t1, 0);
    while(j < numberOfRequest){
        count = 0;
        for(k = 3; k < maxfd; k++){
            if(array[j] == 0){
                //Don't send to myself's listening socket or connection socket
                if(k == MYSOCKFD || k == selfsockfd){
                    continue;
                }
                if(FD_ISSET(k, &masterSet) && available[k] == 1){
                    //Don't send to server for downloading; take down server socket fd for later use
                    if(strcmp(findHostName(k), SERVERNAME) == 0){
                        serverSockfd = k;
                        continue;
                    }
                    startSize = j * CHUNKSIZE;
                    memset(sentBuffer, 0, sizeof(sentBuffer));
                    //send a message with filename, start byte size and chunksize; message looks like this: D|hello.txt|0|10
                    encodeDownloadMessage(sentBuffer, fileName, startSize, CHUNKSIZE);
                    z = send(k, sentBuffer, sizeof(sentBuffer), 0);
                    if(z < 0){
                        bail("send()");
                    }
                    available[k] = 0;  //socket not available anymore
                    count++;
                    array[j] = 1;
                    j++;
                }
            }
            if(count == numberOfID - 1){ //already sent download message to all connected peers
                break;
            }
        }
        //start downloading
        for(h = 3; h < maxfd; h++){
            memset(receivedBuffer2, 0, sizeof(receivedBuffer2));
            memset(fileContent, 0, sizeof(fileContent));
            //exclude myself and server
            if(h == MYSOCKFD || h == selfsockfd || h == serverSockfd){
                continue;
            }
            if(FD_ISSET(h, &masterSet)){
                bytesReceived = recv(h, receivedBuffer2, sizeof(receivedBuffer2), 0);
                if(bytesReceived > 0){
                    startIndex = 0;
                    l = 0;
                    if(receivedBuffer2[0] == '&'){     //end of file
                        break;
                    }
                    while(receivedBuffer2[l] != '|'){
                        startIndex = startIndex * 10 + (receivedBuffer2[l] - '0');
                        l++;
                    }
                    //received "start size|real file content" message
                    index = startIndex / CHUNKSIZE;
                    array[index] = 1;
                    fileBytes = bytesReceived - l;
                    strcpy(fileContent, receivedBuffer2 + l + 1);
                    bytesDownloaded += fileBytes;
                    //after downloading one chunk, this socket becomes available again, will be reused
                    available[h] = 1;
                    //seek to the start bytes position and start writing
                    fseek(fp, startIndex, SEEK_SET);
                    fwrite(fileContent, sizeof(char), sizeof(fileContent), fp);
                    //record down the start chunk value received from each socket;
                    int temp = chunkNumberReceived[h]++;
                    chunkValue[h][temp] = startIndex;
                }
            }
        }
    }
    //calculate total downloading time.
    gettimeofday(&t2, 0);
    elapsed = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;
    printf("finished downloading\n\n");
    printf("It takes %Lf micro seconds to download the file\n\n", elapsed);
    printf("Host\t\tHost-id\tFile chunks downloaded\n");
    for(k = 3; k < maxfd; k++){
        if(k == MYSOCKFD || k == selfsockfd || k == serverSockfd){
            continue;
        }
        if(FD_ISSET(k, &masterSet)){
            memset(&peerAddr, 0, sizeof peerAddr);
            addrlen = sizeof peerAddr;
            z = getpeername(k, (struct sockaddr*) &peerAddr, &addrlen);
            if(z < 0){
                bail("getpeername()");
            }
            hp = gethostbyaddr((char *) &peerAddr.sin_addr, sizeof peerAddr.sin_addr, peerAddr.sin_family);
            if(!hp){
                fprintf(stderr, " Error: %s\n", hstrerror(h_errno));
                return;
            }
            printf("%s\t", hp->h_name);
            if(findID(hp->h_name) != -1){
                printf("%d\t", findID(hp->h_name));
            }
            j = 0;
            for(; j < chunkNumberReceived[k]; j++){
                printf("%d-%d, ", chunkValue[k][j], chunkValue[k][j] + CHUNKSIZE - 1);
            }
            printf("\n\n");
        }
    }
    if(fp){
        fclose(fp);
    }
    free(array);
}

/**
 * this function sends a message to first connected peer to get file size
 * real download work is done after receiving file size message with '*' as header
 */
void downloadFile(char *fileName, int chunkSize){
    CHUNKSIZE = chunkSize;
    int count = 0;
    int i, j, k, h, l, z;
    struct sockaddr_in peerAddr;
    socklen_t addrlen;
    struct hostent *hp;
    char peerName[64];
    char buffer[256];
    int peerSockfd;
    int numberOfRequest;
    bool *array;
    int startSize;

    if(numberOfID == 1){
        fprintf(stderr, "Only connected to server. No peers are available\n\n");
        exit(1);
    }
    strcpy(peerName, connectionList[1].hostname);
    for(i = 3; i < maxfd; i++){
        if(i == MYSOCKFD){
            continue;
        }
        if(FD_ISSET(i, &masterSet)){
            memset(&peerAddr, 0, sizeof peerAddr);
            addrlen = sizeof peerAddr;
            z = getpeername(i, (struct sockaddr*) &peerAddr, &addrlen);
            if(z < 0){
                bail("getpeername()");
            }
            hp = gethostbyaddr((char *) &peerAddr.sin_addr, sizeof peerAddr.sin_addr, peerAddr.sin_family);
            if(!hp){
                fprintf(stderr, " Error: %s\n", hstrerror(h_errno));
                exit(1);
            }
            if(strcmp(peerName, hp->h_name) == 0){
                encodeMessage(buffer, '$', fileName, MYNAME, 0);
                z = send(i, buffer, sizeof(buffer), 0);
                if(z < 0){
                    bail("send()");
                }
                printf("Sent message to %s to get file size of %s\n\n", peerName, fileName);
                break;
            }
        }
    }
}

int findID(char *hostName){
    int i;
    for(i = 0; i < numberOfID; i++){
        if(strcmp(connectionList[i].hostname, hostName) == 0){
            return connectionList[i].ID;
        }
    }
    return -1;
}

// received message: D|hello.txt|0|10
void decodeDownloadMessage(char *buffer, char *fileName, int *startSize, int *chunkSize){
    int j = 2;
    int k = 0;
    while(buffer[j] != '|'){
        fileName[k] = buffer[j];
        j++;
        k++;
    }
    fileName[k] = '\0';
    j++;
    while(buffer[j] != '|'){
        *startSize = 10 * (*startSize) + (buffer[j] - '0');
        j++;
    }
    j++;
    while(buffer[j] != '\0'){
        *chunkSize = 10 * (*chunkSize) + (buffer[j] - '0');
        j++;
    }
}

// message looks something like this:   D|hello.txt|0|10
void encodeDownloadMessage(char *buffer, char *fileName, int startSize, int chunkSize) {
    buffer[0] = 'D';
    buffer[1] = '|';
    strcpy(buffer + 2, fileName);
    buffer[2 + strlen(fileName)] = '|';
    int i, temp;
    int numberOfDigits = 0;
    int totalLength = 0;
    temp = startSize;
    while(temp > 0){
        temp /= 10;
        numberOfDigits++;
    }
    temp = startSize;
    totalLength = 3 + strlen(fileName) + numberOfDigits;
    if(numberOfDigits == 0){
        buffer[totalLength] = '0';
        totalLength += 1;
    }
    else{
        i = totalLength;
        while(temp > 0){
            buffer[i - 1] = (char) (temp % 10 + '0');
            temp /= 10;
            i--;
        }
    }
    buffer[totalLength] = '|';
    temp = chunkSize;
    numberOfDigits = 0;
    while(temp > 0){
        temp /= 10;
        numberOfDigits++;
    }
    temp = chunkSize;
    totalLength += 1 + numberOfDigits;
    i = totalLength;
    while(temp > 0){
        buffer[i - 1] = (char) (temp % 10 + '0');
        temp /= 10;
        i--;
    }
    memset(buffer + totalLength, 0, sizeof(256 - totalLength));
}

//this function sends my info to server before exiting
void hostExit(){
    int i, z;
    struct sockaddr_in serverAddr;
    socklen_t addrlen;
    struct hostent *hp;
    char buffer[256];
    for(i = 3; i < maxfd; i++){
        if(i == MYSOCKFD){
            continue;
        }
        if(FD_ISSET(i, &masterSet)){
            memset(&serverAddr, 0, sizeof serverAddr);
            addrlen = sizeof serverAddr;
            z = getpeername(i, (struct sockaddr*) &serverAddr, &addrlen);
            if(z < 0){
                bail("getpeername()");
            }
            hp = gethostbyaddr((char *) &serverAddr.sin_addr, sizeof serverAddr.sin_addr, serverAddr.sin_family);
            if(!hp){
                fprintf(stderr, " Error: %s\n", hstrerror(h_errno));
                break;
            }
            if(strcmp(SERVERNAME, hp->h_name) == 0){
                encodeMessage(buffer, 'E', MYNAME, MYIP, MYPORT);
                z = send(i, buffer, sizeof(buffer), 0);
                if(z < 0){
                    bail("send()");
                }
                printf("Sent EXIT message to server\n\n");
                break;
            }
        }
    }
//clean up my connection list
    for(i = 0; i < numberOfID; i++){
        memset(connectionList[i].hostname, 0, sizeof(connectionList[i].hostname));
        memset(connectionList[i].IP, 0, sizeof(connectionList[i].IP));
        connectionList[i].ID = -1;
        connectionList[i].port = -1;
    }
//close connections
    for(i = 3; i < maxfd; i++){
        if(FD_ISSET(i, &masterSet)){
            close(i);
            FD_CLR(i, &masterSet);
        }
    }
    printf("All connections have closed. Process terminated. Bye\n\n");
    exit(1);
}

void terminateConnection(int ID){
    int i, z;
    struct sockaddr_in peerAddr;
    socklen_t addrlen;
    struct hostent *hp;
    char terminatePeerName[128];
    char terminatePeerIP[64];
    int terminatePeerPort;
    char buffer[256];

//find terminating ID's hostname in my connection list
    strcpy(terminatePeerName, connectionList[ID - 1].hostname);
    strcpy(terminatePeerIP, connectionList[ID - 1].IP);
    terminatePeerPort = connectionList[ID - 1].port;
    for(i = 3; i < maxfd; i++){
        if(i == MYSOCKFD){
            continue;
        }
        if(FD_ISSET(i, &masterSet)){
            memset(&peerAddr, 0, sizeof peerAddr);
            addrlen = sizeof peerAddr;
            z = getpeername(i, (struct sockaddr*) &peerAddr, &addrlen);
            if(z < 0){
                bail("getpeername()");
            }
            hp = gethostbyaddr((char *) &peerAddr.sin_addr, sizeof peerAddr.sin_addr, peerAddr.sin_family);
            if(!hp){
                fprintf(stderr, " Error: %s\n", hstrerror(h_errno));
                break;
            }
            //found terminating host
            if(strcmp(terminatePeerName, hp->h_name) == 0){
                encodeMessage(buffer, 'T', MYNAME, MYIP, MYPORT);
                z = send(i, buffer, sizeof(buffer), 0);
                if(z < 0){
                    bail("send()");
                }
                printf("Sent termination message to %s\n\n", terminatePeerName);
                close(i);
                FD_CLR(i, &masterSet);
                numberOfConnection--;
                printf("Terminating connection with %s on socket %d\n\n", terminatePeerName, i);
                removeFromConnectionList(ID);
                break;
            }
        }
    }
    if(i == maxfd - 1){
        int j;
        for(j = i - 1; j >= 2; j--){
            if(FD_ISSET(j, &masterSet)){
                maxfd = j + 1;
                break;
            }
        }
    }
}

void removeFromConnectionList(int ID){
    int i;
    numberOfID--;
    for(i = ID - 1; i < numberOfID; i++){
        memset(connectionList[i].hostname, 0, sizeof(connectionList[i].hostname));
        memset(connectionList[i].IP, 0, sizeof(connectionList[i].IP));
        strcpy(connectionList[i].hostname, connectionList[i + 1].hostname);
        strcpy(connectionList[i].IP, connectionList[i + 1].IP);
        connectionList[i].port = connectionList[i + 1].port;
    }
    connectionList[numberOfID].ID = -1;
    memset(connectionList[numberOfID].hostname, 0, sizeof(connectionList[numberOfID].hostname));
    memset(connectionList[numberOfID].IP, 0, sizeof(connectionList[numberOfID].IP));
    connectionList[numberOfID].port = -1;
}

void listConnection(){
    int i;
    printf("\nID\tHostname\t\tIP address\tPort No.\n\n");
    for(i = 0; i < numberOfID; i++){
        printf("%d\t%s\t\t%s\t%d\n\n", connectionList[i].ID, connectionList[i].hostname,
                connectionList[i].IP, connectionList[i].port);
    }
}

void connectToHost(char *destination, int port){
    int i, j, z;
    int len;
    struct hostent *hp;
    struct in_addr destAddr;
    struct sockaddr_in hostAddr;
    int mySockfd;
    char *hostIP;
    char peerIP[64];
    char peerName[128];

    if(strcmp(MYNAME, destination) == 0 || strcmp(MYIP, destination) == 0){
        fprintf(stderr, "Self connection is not allowed\n\n");
        return;
    }
    for(j = 0; j < numberOfID; j++){
        if(strcmp(connectionList[j].hostname, destination) == 0 || strcmp(connectionList[j].IP, destination) == 0){
            fprintf(stderr, "Connection already existed. Duplicate connection not allowed\n\n");
            return;
        }
    }
//input destination is an IP address, need to find the hostname;
    if(isdigit(destination[0])){
        inet_pton(AF_INET, destination, &destAddr);
        hp = gethostbyaddr(&destAddr, sizeof destAddr, AF_INET);
        strcpy(peerName, hp->h_name);
        strcpy(peerIP, destination);
    }
//input destination is a hostname, need to find the IP;
    else{
        hp = gethostbyname(destination);
        if(hp == NULL){
            herror("gethostbyname()");
            return;
        }
        strcpy(peerName, destination);
        strcpy(peerIP, inet_ntoa(*((struct in_addr *) hp->h_addr)));
    }
    memset(&hostAddr, 0, sizeof hostAddr);
    hostAddr.sin_family = AF_INET;
    hostAddr.sin_port = htons(port);
    hostAddr.sin_addr.s_addr = inet_addr(peerIP);
    if(hostAddr.sin_addr.s_addr == INADDR_NONE){
        bail("bad address");
    }
    mySockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(mySockfd == -1){
        bail("socket()");
    }
    z = connect(mySockfd, (struct sockaddr *) &hostAddr, sizeof hostAddr);
    if(z == -1){
        bail("connect()");
    }
//connect message looks like this: C|euston.cse.buffalo.edu|128.205.36.34|22222
    char buffer[256];
    encodeMessage(buffer, 'C', MYNAME, MYIP, MYPORT);
    z = send(mySockfd, buffer, sizeof(buffer), 0);
    if(z < 0){
        bail("send()");
    }
    saveToConnectionList(peerName, peerIP, port);
    FD_SET(mySockfd, &masterSet);
    if(maxfd <= mySockfd){
        maxfd = mySockfd + 1;
    }
    numberOfConnection++;
    printf("Successfully Connecting to %s \n\n", peerName);
}

void registerWithServer(char *serverIP, int port){
    int sockfd;
    struct sockaddr_in serverAddr;
    int i, z;
    memset(&serverAddr, 0, sizeof serverAddr);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);
    if(serverAddr.sin_addr.s_addr == INADDR_NONE){
        bail("bad address");
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        bail("socket()");
    }
    z = connect(sockfd, (struct sockaddr *) &serverAddr, sizeof serverAddr);
    if(z == -1){
        bail("connect()");
    }
//register message looks like this: R|euston.cse.buffalo.edu|128.205.36.34|22222
    char buffer[256];
    encodeMessage(buffer, 'R', MYNAME, MYIP, MYPORT);
    z = send(sockfd, buffer, sizeof(buffer), 0);
    if(z < 0){
        bail("send()");
    }
    saveToConnectionList(SERVERNAME, serverIP, port);
    FD_SET(sockfd, &masterSet);
    if(maxfd <= sockfd){
        maxfd = sockfd + 1;
    }
    numberOfConnection++;
    printf("Successfully sent register request to server %s\n\n", serverIP);
}

void saveToConnectionList(char *hostName, char *hostIP, int hostPort){
    int i = numberOfID;
    numberOfID++;
    connectionList[i].ID = numberOfID;
    strcpy(connectionList[i].hostname, hostName);
    strcpy(connectionList[i].IP, hostIP);
    connectionList[i].port = hostPort;
}

void getIP() {
    int z, len, sockfd;
    struct sockaddr_in localAddr;
    struct sockaddr_in publicServerAddr;
    const char *publicServer = "8.8.8.8";
    const unsigned int port = 53;
    memset(&publicServerAddr, 0, sizeof publicServerAddr);
    publicServerAddr.sin_family = AF_INET;
    publicServerAddr.sin_port = htons(port);
    inet_aton(publicServer, &(publicServerAddr.sin_addr));
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        bail("socket()");
    }
    z = connect(sockfd, (struct sockaddr *) &publicServerAddr, sizeof publicServerAddr);
    if(z == -1){
        bail("connect()");
    }
    len = sizeof localAddr;
    z = getsockname(sockfd, (struct sockaddr *) &localAddr, &len);
    if(z == -1){
        bail("getsockname()");
    }
    strcpy(MYIP, inet_ntoa(localAddr.sin_addr));
    close(sockfd);
}

void displayCommands(){
    printf("\n");
    printf("List of commands:\n");
    printf("\tMYIP --- Display the IP address\n");
    printf("\tMYPORT --- Display the port number\n");
    printf("\tREGISTER <IP> <port> --- Register client with the server\n");
    printf("\tCONNECT <destination> <port> --- Connect to destination at specified port\n");
    printf("\tLIST --- Display all connections\n");
    printf("\tTERMINATE <connectionID> --- Terminate connection id from the list\n");
    printf("\tEXIT --- Close all connections and terminate the process\n");
    printf("\tDOWNLOAD <file_name> <chunk_size> --- Download chunks of file from all connected hosts\n");
    printf("\n");
}

void bail(const char *on_what) {
    perror(on_what);
    exit(1);
}
