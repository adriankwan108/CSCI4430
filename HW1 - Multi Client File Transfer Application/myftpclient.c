# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
# include <errno.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include "myftp.h"

void clientExit(int server){
    struct message_s EXIT_REQUEST;
    initialize(&EXIT_REQUEST,"EXIT_REQUEST",0,0);
    
    //send request message to server
    int len;
    if((len=sendn(server, (void*)&EXIT_REQUEST, 10))<0){
        printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }
    
    //recevie message and get payload from server
    struct message_s RECEIVE;
    if(recvn(server, (void*) &RECEIVE, 10) <0){
        printf("Receive Error in Client: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }
    

    //verify message
    if((memcmp(RECEIVE.protocol,"fubar",5)==0) && (RECEIVE.type==0xD2)){
        printf("Exit request permitted. Close in 3s.\n");
        sleep(3);
    }else{
        printf("Reply invalid or wrong protocol.\n");
        exit(1);
    }

    
}




void clientList(int server){
    struct message_s LIST_REQUEST;
    initialize(&LIST_REQUEST,"LIST_REQUEST",0,0);
    
    //send request message to server
    int len;
    if((len=sendn(server, (void*)&LIST_REQUEST, 10))<0){
        printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }
    
    //recevie message and get payload from server
    struct message_s RECEIVE;
    if(recvn(server, (void*) &RECEIVE, 10) <0){
        printf("Receive Error in Client: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }
    
    unsigned int payload;
    //verify message
    if((memcmp(RECEIVE.protocol,"fubar",5)==0) && (RECEIVE.type==0xA2)){
        payload = ntohl(RECEIVE.length) - 10;
        //printf("payload = %d\n", payload);
    }else{
        printf("Reply invalid or wrong protocol.\n");
        exit(1);
    }
    
    char printBuffer[payload+1];
    //printf("payload = %d\n", payload);
    if(recvn(server, printBuffer, payload) <0){
        printf("Receive Error in Client: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }
    printBuffer[payload] = '\0';
    //printf("list files:\n");
    printf("%s", printBuffer);
    
}

void clientPut(int server, char* fileName){
    if( access( fileName, F_OK ) != 0 ) {
    // file doesn't exist
        printf("file doesn't exist\n");
        return;
    }
    //else
        //printf("%s exist\n",fileName);  
    // file  exists 
    struct message_s GET_REQUEST;
    int fileNameLength = strlen(fileName);
    //printf("len = %d\n", fileNameLength);

    //create put request message
    struct message_s PUT_REQUEST;
    initialize(&PUT_REQUEST, "PUT_REQUEST", fileNameLength +1, 0);  //add one more null charactor at the end of string
    char request_Buffer[10+fileNameLength+1];
    memset(&request_Buffer, '\0', sizeof(request_Buffer));
    memcpy(&request_Buffer,&PUT_REQUEST,10);
    strcpy(&request_Buffer[10], fileName);
    //printf("%s\n", request_Buffer);

    //send put request to server
    if(sendn(server, (void*)&request_Buffer, sizeof(request_Buffer))<0){
        printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }
  
    //receive reply from server
    struct message_s RECEIVE;
    if(recvn(server, (void*) &RECEIVE, 10) <0){
        printf("Get: Receive Error in Client: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }
    
    //verify message
    if( memcmp(RECEIVE.protocol,"fubar",5) == 0){
        //identify types: exist / not exist
        if(RECEIVE.type == 0xC2){   //exist
            printf("Get a reply from the server awaiting file!\n");
        }
        else{
            printf("Get_reply type error\n");
            exit(1);
        }
    }else{
        printf("Reply invalid or wrong protocol.\n");
        exit(1);
    }    

    
    //prepare to send file data to server
    printf("prepare upload file data...\n");
    FILE* filePTR;
    //naming
    char destination[2+fileNameLength];
    memcpy(&destination, "./", 2);
    strcpy(&destination[2], fileName);
    //printf("file open: %s\n", destination);
    int filePayload=0;
    char c;
    //char fileBuffer[1073741824]; //segmentation fault
    char* fileBuffer;
    //open file
    filePTR = fopen(destination,"r");
    if(filePTR == NULL){
        printf("Open file failure in client.\n");
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
        //printf("fileBuffer's data:%s\n", &fileBuffer[10]);
        //printf("filePayload = %d\n", filePayload);
        if(sendn(server, fileBuffer, 10 + filePayload)<0){
                printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
                exit(0);
            }
        }
        fclose(filePTR);
        free(fileBuffer); 

}

void clientGet(int server, char* fileName){
    int fileNameLength = strlen(fileName);
    //printf("len = %d\n", fileNameLength);
    
    //create get request message
    struct message_s GET_REQUEST;
    initialize(&GET_REQUEST, "GET_REQUEST", fileNameLength +1, 0);  //add one more null charactor at the end of string
    
    //create buffer for sending to server
    char request_Buffer[10+fileNameLength+1];
    memset(&request_Buffer, '\0', sizeof(request_Buffer));
    memcpy(&request_Buffer,&GET_REQUEST,10);
    strcpy(&request_Buffer[10], fileName);
    //printf("%s\n", request_Buffer);
    if(sendn(server, (void*)&request_Buffer, sizeof(request_Buffer))<0){
        printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }
    
    //receive reply message from server
    struct message_s RECEIVE;
    if(recvn(server, (void*) &RECEIVE, 10) <0){
        printf("Get: Receive Error in Client: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }
    
    //verify message
    if( memcmp(RECEIVE.protocol,"fubar",5) == 0){
        //identify types: exist / not exist
        if(RECEIVE.type == 0xB2){   //exist
            printf("Downloading...\n");
        }else if(RECEIVE.type == 0xB3){  //not exist
            printf("The file is not exist.\n");
            return;
        }else{
            printf("Get_reply type error\n");
            exit(1);
        }
    }else{
        printf("Reply invalid or wrong protocol.\n");
        exit(1);
    }
    
    
    //receive file data from server
    struct message_s FILE_RECEIVE;
    if(recvn(server, (void*) &FILE_RECEIVE, 10) <0){
        printf("Get: Receive Error in Client: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }
    
    //validate
    int fileSize;
    if(memcmp(FILE_RECEIVE.protocol,"fubar",5)==0){
        if(FILE_RECEIVE.type == 0xFF){
            //ok, get payload file size
            fileSize = ntohl(FILE_RECEIVE.length)-10;
            //printf("fileSize = %d\n", fileSize);
            //get data
            char* fileData = malloc(fileSize);
            if(recvn(server, fileData , fileSize)<0){
                printf("Get: File Data error in client.\n");
                exit(1);
            }
            
            //File successfully receive, create the file in our destination directory
            //printf("%s", fileData);
            FILE *ptr;
            ptr = fopen(fileName, "w");
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

int isValidServerIP(char* ip){
    struct sockaddr_in sk;
    int result = inet_pton(AF_INET, ip, &sk.sin_addr);
    if(result==1){
        return 1;
    }else{
        return 0;
    }
}

void tutorialFunction(int sd){
    int functioning = 1;
    while(functioning==1){
        char buff[100];
        memset(buff,0,100);
        scanf("%s",buff);
        printf("%s\n",buff);
        int len;
        if((len=send(sd,buff,strlen(buff),0))<0){
            printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
            exit(0);
        }
        if(strcmp(buff,"exit")==0){
            functioning = 0;
        }
    }
}

void loopFunction(int sd){
    int functioning = 1;
    char *token;
    const char s[2] = " ";

    char ch_arr[3][33];
    memset(ch_arr,0,99);
    char* c;
    
    while(functioning==1){
        char buff[100];
        memset(buff,0,100);
        fgets(buff, sizeof(buff), stdin);
        c=buff;
        while(*c!='\n')
        {
            c++;
        }
        *c ='\0';
        //printf("%s\n",buff);
        int i=0;
        /* get the first token */
        token = strtok(buff, s);
        /* walk through other tokens */
        while(token != NULL ) {
           strncpy ( ch_arr[i], token, 33 );
           //printf( "%s\n", ch_arr[i]);
           token = strtok(NULL, s);
           i++;
           if(i==3)
               break;
        }

        if(i==1 && strcmp(ch_arr[0],"list")==0)
        {
           //printf("ch_arr[0] is %s\n",ch_arr[0]);
           clientList(sd);
           //printf("This is list command\n");
        }
        else if (i==2 && strcmp(ch_arr[0],"get")==0)
        {
           clientGet(sd, ch_arr[1]);
          // printf("This is get command\n");
        }
        else if (i==2 && strcmp(ch_arr[0],"put")==0)
        {
           clientPut(sd, ch_arr[1]);
          // printf("This is put command\n");
        }
        else if (i==1 && strcmp(ch_arr[0],"exit")==0)
        {
           clientExit(sd);
           break;
        }
        else
        {
           printf("Please tpye corrent command: list | get <filename> | put <filename> | exit\n");
        }
        //printf("i is:%d\n",i);

    }


}


int main(int argc, char** argv){
    in_addr_t serverIP;
    in_port_t serverPORT;
    int tempPORT;
    
    //handle interface
    if((argc==4 && strcmp(argv[3],"list")==0)|| (argc==5 &&(strcmp(argv[3],"get")==0||strcmp(argv[3],"put")==0))){
        if( isValidServerIP(argv[1])!= 1){
            printf("Invalid Server IP Address\n");
            exit(1);
        }else{
            serverIP = inet_addr(argv[1]);
        }
        tempPORT = atoi(argv[2]);
        if(tempPORT<0 || tempPORT > 65535){
            printf("Invalid Server PORT number\n");
            exit(1);
        }else{
            serverPORT = htons(tempPORT);
        }
    }else{
        printf("Usage error: ./myftpclient <server ip addr> <server port> <list|get|put> <file>\n");
        exit(1);
    }
    
    //printf("Success: IP:<%u> Port:<%u> in network byte orders\n", serverIP, serverPORT);
    
    //after successful commands, create TCP socket,
    int sd = socket(AF_INET, SOCK_STREAM,0);
    if(sd==-1){
        printf("TCP socket built failure");
        exit(1);
    }
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = serverIP;
    server.sin_port = serverPORT;
    
    //connect to the server
    if(connect(sd, (struct sockaddr*)&server, sizeof(server))<0){
        printf("connection error: %s (Errno:%d)\n", strerror(errno), errno);
        exit(0);
    }
    
    //handle different commands
    //these commands should be blocking functions
    //tutorialFunction(sd);
    
    
    if(argc==4){
        clientList(sd);
    }else if(strcmp(argv[3],"put")==0){
        clientPut(sd, argv[4]);
    }else{
        clientGet(sd, argv[4]);
    }
    loopFunction(sd);
    printf("ByeBye~\n");
    close(sd);
	return 0;
}
