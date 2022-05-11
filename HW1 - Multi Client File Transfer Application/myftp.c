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

int sendn(int sd, void* buf, int buf_len){
    int n_left = buf_len;
    int n;
    while(n_left > 0){
        if( (n=send(sd,buf+(buf_len-n_left),n_left,0)) <0 ){
            if(errno == EINTR){
                n = 0; //EINTR: interrupt
            }else{
                return -1;
            }
        }else if(n == 0){
            return 0;
        }
        n_left -= n;
    }
    return buf_len;
}

int recvn(int sd, void* buf, int buf_len){
    int n_left = buf_len;
    int n;
    while(n_left>0){
        if( (n=recv(sd, buf + (buf_len - n_left), n_left,0))<0 ){
            if(errno == EINTR){
                n=0;
            }else{
                return -1;
            }
        }else if(n == 0){
            return 0;
        }
        n_left -= n;
    }
    return buf_len;
}

void initialize(struct message_s* message, char* m_name, unsigned int payload, int reply){
    struct message_s* modify = message;
    if(strcmp(m_name,"LIST_REQUEST")==0){
        memcpy(modify->protocol, "fubar", 5);
        modify->type = 0xA1;
        modify->length = htonl(10);
    }else if(strcmp(m_name, "LIST_REPLY")==0){
        memcpy(modify->protocol,"fubar", 5);
        modify->type = 0xA2;
        modify->length = htonl(10+payload);
    }else if(strcmp(m_name, "GET_REQUEST")==0){
        memcpy(modify->protocol,"fubar", 5);
        modify->type = 0xB1;
        modify->length = htonl(10+payload);
    }else if(strcmp(m_name, "GET_REPLY")==0){
        memcpy(modify->protocol,"fubar", 5);
        if(reply==1){
            //file exitst
            modify->type = 0xB2;
        }else if(reply==2){
            //file not exist
            modify->type = 0xB3;
        }
        modify->length = htonl(10);
    }else if(strcmp(m_name, "PUT_REQUEST")==0){
        memcpy(modify->protocol,"fubar", 5);
        modify->type = 0xC1;
        modify->length = htonl(10+payload);
    }else if(strcmp(m_name, "PUT_REPLY")==0){
        memcpy(modify->protocol,"fubar", 5);
        modify->type = 0xC2;
        modify->length = htonl(10);
    }else if(strcmp(m_name, "FILE_DATA")==0){
        memcpy(modify->protocol,"fubar", 5);
        modify->type = 0xFF;
        modify->length = htonl(10 + payload);
    }else if(strcmp(m_name, "EXIT_REQUEST")==0){
        memcpy(modify->protocol,"fubar", 5);
        modify->type = 0xD1;
        modify->length = htonl(10);
    }else if(strcmp(m_name, "EXIT_REPLY")==0){
        memcpy(modify->protocol,"fubar", 5);
        modify->type = 0xD2;
        modify->length = htonl(10);
    }
    
    if(reply == 1 || reply == 2){
        printf("Reply Message protocol:               %.5s\n", (*message).protocol);
        printf("Reply Message type:                   %u\n", (*message).type);
        printf("Reply Length in   host byte order:    %d\n", ntohl((*message).length));
        printf("               network byte order:    %u\n", (*message).length);
    }
}
