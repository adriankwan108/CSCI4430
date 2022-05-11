# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
# include <errno.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <netinet/in.h>
# include <pthread.h>
# include "myftp.h"
# include <dirent.h>

void putFunction(void* client, int fileNameLength)
    {
     int* client_sd = (int*)client;
     char received_Buffer[fileNameLength];
    if( recvn(*client_sd,received_Buffer, fileNameLength) <0){
        printf("Get: receive error in server side: %s (Errno:%d)\n", strerror(errno),errno);
        exit(1);
    }
    printf("fileName: %s\n", received_Buffer);
    
    //server send reply to client
    struct message_s PUT_REPLY;
    initialize(&PUT_REPLY, "PUT_REPLY", 0,0);

    if(sendn(*client_sd, (void*)&PUT_REPLY, 10)<0){
        printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }


    //receive file data from client
    struct message_s FILE_RECEIVE;
    if(recvn(*client_sd, (void*) &FILE_RECEIVE, 10) <0){
        printf("Get: Receive Error in Client: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }
    
    //validate
    int fileSize;
    if(memcmp(FILE_RECEIVE.protocol,"fubar",5)==0){
        if(FILE_RECEIVE.type == 0xFF){
            //ok, get payload file size
            fileSize = ntohl(FILE_RECEIVE.length)-10;
            printf("fileSize = %d\n", fileSize);
            //get data
            char* fileData = malloc(fileSize);
            if(recvn(*client_sd, fileData , fileSize)<0){
                printf("Get: File Data error in client.\n");
                exit(1);
            }
            
            //File successfully receive, create the file in our destination directory
            printf("fileData:%s", fileData);


            char destination[7+fileNameLength];
            memcpy(&destination, "./data/", 7);
       	    strcpy(&destination[7], received_Buffer);
            printf("file open: %s\n", destination);

            FILE *ptr;
            ptr = fopen(destination, "w");
            fwrite(fileData, fileSize, 1, ptr);
            fclose(ptr);
            
            free(fileData);
            
        }else{
            printf("Wrong type of message.\n");
            exit(1);
        }
    }else{
        printf("Wrong protocol.\n");
        exit(1);
    }

    }




void getFunction(void* client, int fileNameLength){
    int* client_sd = (int*)client;
    
    char received_Buffer[fileNameLength];
    if( recvn(*client_sd,received_Buffer, fileNameLength) <0){
        printf("Get: receive error in server side: %s (Errno:%d)\n", strerror(errno),errno);
        exit(1);
    }
    printf("fileName: %s\n", received_Buffer);
    
    //the following code cannot differentiate failure/ not exist, so abandon
    /*
    FILE *fptr;
    fptr = fopen("./data"+received_Buffer,"r");   //error here
    if(fptr==NULL){
        printf("Not found / Failure");
    }else{
        printf("Success");
    }
     */
    
    //verify the existance of file
    DIR* dir;
    int exist = 2;
    struct dirent* dirent_result;
    
    if((dir=opendir("./data")) == NULL){
        printf("Open Directory Failure / ./data does not exist.\n");
        exit(0);
    }
    while( (dirent_result=readdir(dir))!= NULL ){
        printf("%s\n", dirent_result->d_name);
        //add \n between file name
        if(strcmp(dirent_result->d_name, &received_Buffer[0])==0){
            exist = 1;
            break;
        }
    }
    
    //send reply message
    struct message_s GET_REPLY;
    initialize(&GET_REPLY, "GET_REPLY", 0, exist); //exist= 1, not exist= 2

    if(sendn(*client_sd, (void*)&GET_REPLY, 10)<0){
        printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }
    
    
    if(exist == 1){
        //prepare file data
        printf("prepare file data...\n");
        FILE* filePTR;
        
        //naming
        char destination[7+fileNameLength];
        memcpy(&destination, "./data/", 7);
        strcpy(&destination[7], received_Buffer);
        printf("file open: %s\n", destination);
        
        int filePayload=0;
        char c;
        //char fileBuffer[1073741824]; //segmentation fault
        char* fileBuffer;
        
        //open file
        filePTR = fopen(destination,"r");
        if(filePTR == NULL){
            printf("Open file failure in server.\n");
            exit(1);
        }else{
            //get file length
            fseek(filePTR, 0L, SEEK_END);
            filePayload = ftell(filePTR); //as assume 1GB, won't exceed int limit
            rewind(filePTR);
            
            fileBuffer = malloc(filePayload + 10);
            
            //message
            struct message_s FILE_DATA;
            initialize(&FILE_DATA, "FILE_DATA", filePayload,1);
            memcpy(fileBuffer,&FILE_DATA, 10);
            
            int i;
            for(i=10; i<filePayload+10; i++){
                c = fgetc(filePTR);
                fileBuffer[i] = c;
            }
            //printf("%s\n", &fileBuffer[10]);
            printf("filePayload = %d\n", filePayload);
            if(sendn(*client_sd, fileBuffer, 10 + filePayload)<0){
                printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
                exit(0);
            }
        }
        fclose(filePTR);
        free(fileBuffer);
        
    }else{
        //bye bye client
        printf("File not exist.\n");
        return;
    }
}


void exitFunction(void* client){
    int* client_sd = (int*)client;
    struct message_s EXIT_REPLY;
    initialize(&EXIT_REPLY, "EXIT_REPLY", 0,0);

    if(sendn(*client_sd, (void*)&EXIT_REPLY, 10)<0){
        printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }


}

void listFunction(void* client){
    int* client_sd = (int*)client;
    
    DIR* dir;                       // destination directory
    struct dirent* dirent_result;   // store results in dirent
    
    //open file
    if((dir=opendir("./data")) == NULL){
        printf("Open Directory Failure / Not exist.\n");
        exit(0);
    }
    
    //recording and save into a buffer
    /*Theoretically there is no limit in num of files under a file in Linux.
            But the num of file descriptor that any one process may open is up to 1024 per process */
    
    char send_buffer[NAME_MAX*1024 + 10];               //make dynammic buffer if you want
    memset(send_buffer,'\0', sizeof(send_buffer));      //initiailize
    int payload = 0;                                    //payload used in sending protocol.length
    printf("-------------Content--------------\n");
    while( (dirent_result=readdir(dir))!= NULL ){
        strcpy(&send_buffer[10+payload], dirent_result->d_name);
        payload += strlen(dirent_result->d_name);
        printf("%s\n", dirent_result->d_name);
        //add \n between file name
        memset(&send_buffer[10+payload],'\n',1);
        payload += 1;
    }
    printf("-----------payload = %d-----------\n", payload);
    
    //close file
    if( closedir(dir) == -1){
        printf("Close Directory Failure: %s (Errno:%d)\n", strerror(errno), errno);
        exit(1);
    }
    
    //add message to the buffer
    struct message_s LIST_REPLY;
    initialize(&LIST_REPLY, "LIST_REPLY", payload, 1);
    memcpy(&send_buffer,&LIST_REPLY,10);
    
    //send the whole buffer to client
    if(sendn(*client_sd, send_buffer, 10+payload )<0){
        printf("send error: %s (Errno:%d)\n", strerror(errno),errno);
        exit(1);
    }
}


void* clientHandler(void* client){
    int* client_sd = (int*)client;
    printf("Successful thread creation: client_sd: %d\n", *client_sd);
while(1){
    //receive data from client, save it to a buffer
    char received_Buffer[11];
    int len;
    if( (len=recvn(*client_sd,received_Buffer, 10)) <0){
        printf("receive error: %s (Errno:%d)\n", strerror(errno),errno);
        exit(1);
    }
    received_Buffer[10] = '\0';
    
    
    //turn data into our message format
    struct message_s RECEIVED;
    memcpy(&RECEIVED, received_Buffer, 10);
    RECEIVED.length = ntohl(RECEIVED.length);
    printf("Received protocol:      %.5s\n", RECEIVED.protocol);
    printf("Received type:          %u\n", RECEIVED.type);
    printf("Length in host order:   %u\n", RECEIVED.length);
    
    //remaining payload in request for get/put (fileNameLength)
    int payload;
    payload = RECEIVED.length-10;
    
    /*as string in C will detect null charactor \0 as terminated,
     strcmp should cause message.protocol ended with some content in random memory, then comparing string should give wrong result */
    //so use memcmp
    if(memcmp(RECEIVED.protocol,"fubar",5)==0){
        if(RECEIVED.type == 0xA1){
            printf("List\n");
            listFunction(client);
        }else if(RECEIVED.type == 0xB1){
            printf("Get\n");
            getFunction(client, payload);
        }else if(RECEIVED.type == 0xC1){
            printf("Put\n");
            putFunction(client, payload);
        }else if(RECEIVED.type == 0xD1){
            printf("Disconnected\n");
            exitFunction(client);
            break;
        }else{
            printf("Request failure\n");
            exit(1);
        }
    }else{
        printf("Transmission Error: different protocol / No message \n");
    }
}
    /*  tutorial
    while(1){
        char buff[100];
        int len;
        if((len=recv(*client_sd,buff,sizeof(buff),0))<0){
            printf("receive error: %s (Errno:%d)\n", strerror(errno),errno);
            exit(0);
        }
        buff[len]='\0';
        printf("RECEIVED INFO: ");
        if(strlen(buff)!=0){
            printf("%s\n",buff);
        }
        if(strcmp("exit",buff)==0){
            close(*client_sd);
            break;
        }
    }
    */
    
    close(*client_sd);
    printf("                ----------------------------------\n");
    printf("                    client_sd: %d is disconnected\n", *client_sd);
    printf("                ----------------------------------\n");
    pthread_exit(NULL);
}

int main(int argc, char** argv){
    //handle interface
    if(argc!=2)
    {
        printf("Usage Error: ./myftpserver PORT_NUMBER\n");
        exit(0);
    }
    
    int portNUm = atoi(argv[1]);
    if(portNUm<0 || portNUm > 65535){
        printf("Invalid port num\n");
        exit(0);
    }
    
    //successful command, create TCP listening server socket
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if(server == -1){
        printf("Listening server creation error");
        exit(1);
    }
    //reusable server port if server crashes
    long val = 1;
    if(setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(long))==-1){
        perror("setsockopt");
        exit(1);
    }
    
    //bind our server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(portNUm);
    if(bind(server, (struct sockaddr*) &server_addr, sizeof(server_addr))==-1){
        printf("Binding failure.\n");
        exit(1);
    }
    
    //server start listening, not a blocking call
    if(listen(server, 100 )<0){       //the max num of pending connections
        printf("listen error: %s (Errno:%d)\n", strerror(errno), errno);
        exit(0);
    }
    
    //create multi-thread for accepting
    int client_sd[15];
    pthread_t thread[15];
    struct sockaddr_in client[15];
    socklen_t client_len=sizeof(client);
    
    int index = 0;
    //the server shall not be stopped
    while(1){
        //printf("index: %d\n", index);
        if( (client_sd[index]=accept(server,(struct sockaddr*) &client[index],&client_len)) == -1){
            printf("Client[%d] accept error: %s, (Errno:%d)\n", index, strerror(errno),errno);
        }else{
            // if accepted successfully, create threads
            printf("                ----------------------------------\n");
            printf("                Client[%d] is connected: client_sd: %d\n", index, client_sd[index]);
            printf("                ----------------------------------\n");
            if(pthread_create(&thread[index], NULL, &clientHandler,(void*)&client_sd[index])!=0){
                printf("Create thread [%d] error occurs\n", index);
                exit(1);
            }
        }

        index += 1;
        index = index % 15;
    }
    int k;
    for(k=0; k<15; k++){
        pthread_join(thread[k], NULL);
    }
    //close(server);
	return 0;
}
