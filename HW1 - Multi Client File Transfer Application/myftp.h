struct message_s { //total: 10 bytes
    unsigned char protocol[5];  // protocol string  5 bytes
    unsigned char type;         // type             1 bytes
    unsigned int length;        // header & payload 4 bytes
} __attribute__ ((packed));

//definitions shared:
#define PROTOCOL_STRING "fubar"
#define NAME_MAX 255    /*chars in a file name*/
#define PATH_MAX 4096   /*chars in a path name including nul*/


//functions shared:
int sendn(int sd, void *buf, int buf_len);

int recvn(int sd, void *buf, int buf_len);

void initialize(struct message_s* message, char* m_type, unsigned int payload, int reply);
