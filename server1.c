#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termio.h>



#define MAXPENDING  3   /* Maximum number of pending connections on server */
#define BUFFSIZE   255  /* Maximum number of bytes per message */



char *board [5][5];
char *client = "X";
char *server = "O";
int turn = 1;
char boardfil[BUFFSIZE];
sem_t*  psem1;
sem_t*  psem2;
int sem_value1;
int sem_value2;
bool finishGame=false;
int winer=0;
int connectionsActive=0;

void err_sys(char *mess) { perror(mess); exit(1); }

void VisualizeBoard(){
    printf("%s", "  1 2 3 4 5\n");
    for(int i = 0; i<5; i++){
        printf("%d", i+1);
        for(int j = 0; j<5; j++){
            printf("%s","|");
            printf("%s", board[i][j]);
        }
        printf("%s","|\n");
    }
}


void passboard(){
    strcpy(boardfil,"  1 2 3 4 5\n");
    for(int i = 0; i<5; i++){
        char pos[10] = {(i+1)+'0'};
        strcat(boardfil, pos);
        for(int j = 0; j<5; j++){
            strcat(boardfil,"|");
            strcat(boardfil,board[i][j]);
        }
        strcat(boardfil,"|\n");
    }
}

void SetUpBoard(){
    for(int i = 0; i<5; i++){
        for(int j = 0; j<5; j++){
            board[i][j] = " ";
        }
    }
    VisualizeBoard();
}

bool CheckValidPosition(int coordX, int coordY){

    if(board[coordX-1][coordY-1] != " "){return false;}
    return true;

}

void InsertMovement(int coordX, int coordY){

    switch(turn){
        case -1:
            board[coordX-1][coordY-1] = server;
        break;
        case 1:
            board[coordX-1][coordY-1] = client;
        break;
        default:
            printf("Error");
    }
    VisualizeBoard();
}

bool NotFinishedGame(){
    for(int i = 0; i<5; i++){
        for(int j = 0; j<5; j++){
            if(board[i][j] == " "){return true;}
        }
    }
    return false;
}

void closeProgram(int sock,int result){
    close(result);
    close(sock);
    exit(0);

}



void *handleThread(void *vargp) {
    int *mysock = (int *)vargp; 
    char buffer[BUFFSIZE];
    int received = -1;
    int contador;
    int result;
    char lnplace[BUFFSIZE]={"In which line would you like to place your movement? [1-5] "};
    char clplace[BUFFSIZE]={"In which column would you like to place your movement? [1-5] "};
    char outbound[BUFFSIZE]={"That's an out of bounds location, remember that the range of the matrix is [1-5] "};
    int pos=0;

    char* finishG;
    char* tie="It's a tie.\nGame finish\n";
    char* clientWin ="YOU WIN.\nGame finish\n";
    char* ServerWin="The other player wins, you lose.\nGame finish\n";
    

    
    int coordX;
    char coordXLectura[10];
    int coordY;
    char coordYLectura[10];
    char resultgame;
    char lineaLectura[10];

    finishG = tie;
    int resultsem1;
    int resultsem2;
    int lastresult1;
    int lastresult2;



    printf("Executing handleThread with socket descriptor ID: %d\n", *mysock); 
    fprintf(stdout, "Thread ID in handler %lu\n", pthread_self());


    read(*mysock,&buffer[0],BUFFSIZE);
    printf("message from client: %s\n", buffer);
    pos=atoi(buffer);

    if(pos==2){
        sem_wait(psem2);
    }

   
    VisualizeBoard();


    
    sem_getvalue(psem1, &sem_value1);
    sem_getvalue(psem2, &sem_value2);
    
    while(NotFinishedGame() && !finishGame){ 
        if(pos==1 && sem_value1>=0){
            write(*mysock,lnplace,strlen(lnplace)+1); /*Wich position in line choose the player*/
            read(*mysock,&buffer[0],BUFFSIZE); /*Recibe the position in line*/
            sscanf(buffer,"%d",&coordX); /*safe the position as a int*/
            while(coordX > 5 || coordX < 1){
                write(*mysock,outbound,strlen(outbound)+1); /*Wich position in line choose the player*/
                read(*mysock,&buffer[0],BUFFSIZE); /*Recibe the position in line*/
                sscanf(buffer,"%d",&coordX); /*safe the position as a int*/
            }

            fprintf(stderr, "C %d: Position line selected: %s\n",pos, buffer); /*print the line to be sure wich is the line*/
            write(*mysock,clplace,strlen(clplace)+1); /*Wich position in column choose the player*/
            read(*mysock,&buffer[0],BUFFSIZE); /*Recibe the position in column*/
            sscanf(buffer,"%d",&coordY); /*safe the position as a int*/
            while(coordY > 5 || coordY < 1){
                write(*mysock,outbound,strlen(outbound)+1); /*Wich position in line choose the player*/
                read(*mysock,&buffer[0],BUFFSIZE); /*Recibe the position in line*/
                sscanf(buffer,"%d",&coordY); /*safe the position as a int*/
            }
            fprintf(stderr, "C %d: Position column selected: %s\n",pos, buffer);

            printf("Line: %d",coordX);
            printf("Column: %d",coordY);
            printf("\n");
            if(CheckValidPosition(coordX, coordY)){
                InsertMovement(coordX, coordY);
                turn *= -1;
                printf("Did the client%d win?[Y/N]",pos);  
                fgets(lineaLectura, 10, stdin);
                resultgame = lineaLectura[0];

                if(resultgame=='Y'){
                    printf("Client%d win!!\n",pos);
                    winer=1;
                    finishGame=true;
                    finishG=clientWin;
                    passboard();
                    printf("%s",boardfil);
                    strcat(boardfil,finishG);
                    printf("Game finish\n");
                    write(*mysock,boardfil,strlen(boardfil)+1);
                    sem_post(psem2);
                    turn = 1;
                    close(*mysock);
                    return ((void*)NULL);
                }

                sem_getvalue(psem1, &sem_value1);
                sem_getvalue(psem2, &sem_value2);
                sem_post(psem2);
                sem_wait(psem1);
               
            }else{
                printf("This position has already been taken. \n");
            }    

        }else if(pos == 2 && sem_value2 >= 0){
            write(*mysock,lnplace,strlen(lnplace)+1); /*Wich position in line choose the player*/
            read(*mysock,&buffer[0],BUFFSIZE); /*Recibe the position in line*/
            sscanf(buffer,"%d",&coordX); /*safe the position as a int*/
            while(coordX > 5 || coordX < 1){
                write(*mysock,outbound,strlen(outbound)+1); /*Wich position in line choose the player*/
                read(*mysock,&buffer[0],BUFFSIZE); /*Recibe the position in line*/
                sscanf(buffer,"%d",&coordX); /*safe the position as a int*/
            }

            fprintf(stderr, "C %d: Position line selected: %s\n",pos, buffer); /*print the line to be sure wich is the line*/
            write(*mysock,clplace,strlen(clplace)+1); /*Wich position in column choose the player*/
            read(*mysock,&buffer[0],BUFFSIZE); /*Recibe the position in column*/
            sscanf(buffer,"%d",&coordY); /*safe the position as a int*/
            while(coordY > 5 || coordY < 1){
                write(*mysock,outbound,strlen(outbound)+1); /*Wich position in line choose the player*/
                read(*mysock,&buffer[0],BUFFSIZE); /*Recibe the position in line*/
                sscanf(buffer,"%d",&coordY); /*safe the position as a int*/
            }
            fprintf(stderr, "C %d: Position column selected: %s\n",pos, buffer);

            printf("Line: %d",coordX);
            printf("Column: %d",coordY);
            printf("\n");
            if(CheckValidPosition(coordX, coordY)){
                InsertMovement(coordX, coordY);
                turn *= -1;
                printf("Did the client%d win?[Y/N]",pos);  
                fgets(lineaLectura, 10, stdin);
                resultgame = lineaLectura[0];

                if(resultgame=='Y'){
                    printf("Client%d win!!\n",pos);
                    winer=2;
                    finishGame=true;
                    finishG=clientWin;
                    passboard();
                    printf("%s",boardfil);
                    strcat(boardfil,finishG);
                    printf("Game finish\n");
                    write(*mysock,boardfil,strlen(boardfil)+1);
                    sem_post(psem1);
                    turn = 1;
                    close(*mysock);
                    return ((void*)NULL);
                }
                
                sem_getvalue(psem1, &sem_value1);
                sem_getvalue(psem2, &sem_value2);
                sem_post(psem1);
                
                sem_wait(psem2);
            }else{
                printf("This position has already been taken. \n");
            }    
        }
                     
    }
    sem_getvalue(psem1, &sem_value1);
    sem_getvalue(psem2, &sem_value2);
    printf("%d\n",winer);
    if(winer==0){
        finishG=tie;
    }else if(winer==pos){
        finishG=clientWin;
    }else if(winer!=pos){
        finishG=ServerWin;
    }
    passboard();
    printf("%s",boardfil);
    strcat(boardfil,finishG);
    printf("Game finish\n");
    write(*mysock,boardfil,strlen(boardfil)+1);
    turn = 1;
    close(*mysock);
    connectionsActive=0;
    finishGame=false;
    return ((void*)NULL);

}

int main(int argc, char *argv[]) {
    struct sockaddr_in echoserver, echoclient,echoclient2;
    int serversock, clientsock;
    int returnedpid, result;
    int pid, ppid;
    pthread_t handleThreadId1,handleThreadId2;

    char *notplay="There is a game in progress. You can't play right now";
    int client1=0;
    int client2=0;
        char buffer[BUFFSIZE];



    /* Check input parameters */
    if (argc != 2) {
    err_sys("Usage: server <port>\n");
    }

    /* Create TCP socket*/
    serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serversock < 0) {
    err_sys("Error socket");
    }

    /* Set sockaddr_in */
    memset(&echoserver, 0, sizeof(echoserver));       /* Reset memory */
    echoserver.sin_family = AF_INET;                  /* Internet/IP */
    echoserver.sin_addr.s_addr = htonl(INADDR_ANY);   /* Any address */
    echoserver.sin_port = htons(atoi(argv[1]));       /* Server port */

    /* Bind */
    if (bind(serversock, (struct sockaddr *) &echoserver,sizeof(echoserver)) < 0) {
	    err_sys("error bind");
    }

    /* Listen */
    if (listen(serversock, MAXPENDING) < 0) {
	    err_sys("error listen");
    }
    printf("arribed here\n");
    /* Loop */
    //connectionsActive +=1;
    while (1) {
        fprintf(stdout, "Waiting connection: \n");
        unsigned int clientlen = sizeof(echoclient);
        /* we wait for a connection from a client */
        clientsock = accept(serversock, (struct sockaddr *) &echoclient,&clientlen);
        if (clientsock < 0) {
            err_sys("error accept");
        }
        connectionsActive +=1;


        if(connectionsActive < 2){
            char pospla[1]={connectionsActive+'0'};
            fprintf(stdout, "Client 1: %s\n",inet_ntoa(echoclient.sin_addr));
            client1 = clientsock;
            write(client1,pospla,strlen(pospla)+1);

            
        }else if(connectionsActive == 2){
            char pospla[1]={connectionsActive+'0'};
            client2 = clientsock;
            write(client2,pospla,strlen(pospla)+1);


            /* Open psem1 */
            psem1 = (sem_t*) sem_open("/sem1", O_CREAT, 0644, 0);
            if (psem1 == SEM_FAILED) {
                err_sys("Open psem1");
            }

            /* Read and print semaphore value */
            result = sem_getvalue(psem1, &sem_value1);
            if (result < 0) {
                err_sys("Read psem1");
            }

            while (sem_value1 > 0) {
                sem_wait(psem1);
                sem_value1--;
            }

            /* Open psem2 */
            psem2 = (sem_t*) sem_open("/sem2", O_CREAT, 0644, 0);
            if (psem2 == SEM_FAILED) {
                err_sys("Open psem2");
            }

            /* Read and print semaphore value */
            result = sem_getvalue(psem2, &sem_value2);
            if (result < 0) {
                err_sys("Read psem2");
            }

            while (sem_value2 > 0) {
                sem_wait(psem2);
                sem_value2--;
            }

            fprintf(stdout, "Client: %s\n",inet_ntoa(echoclient.sin_addr));


            SetUpBoard();
            pthread_create(&handleThreadId1, NULL, handleThread, (void *)&client1);
            pthread_create(&handleThreadId2, NULL, handleThread, (void *)&client2);
            connectionsActive+=1;
            
        
        }else if(connectionsActive > 3){
            write(clientsock,notplay,strlen(notplay)+1);
            connectionsActive-=1;
            close(clientsock);
        }
    }
    exit(0);
}