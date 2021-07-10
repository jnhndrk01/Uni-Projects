
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>
#include "uthash.h"

#define BACKLOG 10

#define GET_REQ  0b00000100
#define SET_REQ  0b00000010
#define DEL_REQ = 0b00000001
#define GET_RES = 0b00001100
#define SET_RES = 0b00001010
#define DEL_RES = 0b00001001

struct Packet {
    char flags;
    uint16_t keyLen;
    uint32_t valueLen;
    char *key;
    char *value;
} __attribute__((packed));

struct hash { //struct used by the uthash header file
    void *key;
    int keyLength;
    void *value;
    int valueLength;
    UT_hash_handle hh;
};

static struct hash *hashTable = NULL; //the hashtable

void set(struct hash *s, void *ptr) {
    HASH_ADD_KEYPTR(hh, hashTable, s->key, s->keyLength, s);
}

struct hash *get(struct hash *s, void *ptr) { //get the value to a specific key in the table
    struct hash *a = NULL;
    HASH_FIND(hh, hashTable, s->key, s->keyLength, a);
    return a;
}

void delete(struct hash *entry) { //delete an entry in the table
    HASH_DEL(hashTable, entry);
}

void sigchld_handler(int s){

    while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){

    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}





size_t set_msg (char* src, char* msg, int* a, int num){

    int msglen=0;
    int row= num;

    FILE* fp2 = fopen(src,"r");

    printf("%d\n",row);
    int chp;

    while(!feof(fp2)) {
        if (num != 0) {
            chp = fgetc(fp2);
            if (chp == '\n') {
                num--;
            }
        }
        else {
            msglen++;
            //if (a[row]<=2) {
                //msg= calloc(a[row], sizeof(char));
                if (a[row]<=2) {a[row]=1; break;}
                fgets(msg, a[row], fp2);
                //printf("%s",msg);
                printf("File: row got generated!\n");
                break;
            /*}
            else if (i==1 || a[row]==0)return -1;
            else {chp = fgetc(fp2); chp = fgetc(fp2);row++;}*/
        }
    }

    fclose(fp2);

    return a[row];
}




//receiven des Headers mit Key und Val Länge

int recv_req(int s, struct Packet* P){

    char buffer[7];
    int bytes_received = 0;

    //7 bytes abrufen für header + val/key längen
    while(true){
        int nbytes = 0;

        //7 bytes abrufen für header + key_len + val_len
        nbytes = recv(s, &buffer, 7, 0);
        bytes_received+=nbytes;
        memcpy(P->flags+bytes_received,&buffer,nbytes);
        //Nichts (mehr) bekommen
        if (bytes_received == 7){
            break;
        }
    }

    return 0;
}





//receiven des Keys

int recv_key(int s, struct Packet* P){
    P->key = calloc(P->keyLen,sizeof(char));
    char buffer[512];

    int bytes_received = 0;

    while(true){
        int nbytes = 0;

        nbytes = recv(s, &buffer, 512, 0);
        memcpy(P->key+bytes_received,&buffer,nbytes);
        bytes_received+=nbytes;
        //Nichts (mehr) bekommen
        if (nbytes == 0 || bytes_received == P->keyLen){
            break;
        }
    }

    //Key aus dem Buffer in das Packet kopieren
    return 0;

}





// receiven des Values

int recv_value(int s, struct Packet* P){
    P->value = calloc(P->valueLen,sizeof(char));
    char buffer[512];

    int bytes_received = 0;

    while(true){
        int nbytes = 0;

        nbytes = recv(s, &buffer, 512, 0);
        memcpy(P->value,&buffer+bytes_received,nbytes);
        bytes_received+=nbytes;
        //Nichts (mehr) bekommen
        if (nbytes == 0 || bytes_received == P->valueLen){
            break;
        }
    }

    //Key aus dem Buffer in das Packet kopieren
    return 0;

}


// receiven von header value und key kombiniert. Value wird nur received, wenn set bit gesetzt ist.
int recv_all(int s, struct Packet* P){
    recv_req(s,P);
    recv_key(s,P);
    if(P->flags == SET_REQ){
        recv_value(s,P);
    }

    return 0;
}

int send_pkg(int s, struct Packet* P){
    //Marshalling packet struct into one buffer - genullt durch calloc
    int buffer_len = 7+P->keyLen+P->valueLen;
    char *buffer = calloc(1, buffer_len);
    // Kopiere Flags an Anfang des Buffers
    memcpy(buffer,P->flags,1);
    // Kopiere keyLen nach Flags (+1byte) mit länge 2 byte
    memcpy(buffer+1,P->keyLen,2);
    // Kopiere ValueLen nach Keylen (+3byte) in den Buffer mit Länge 4 Byte
    memcpy(buffer+3,P->valueLen,4);
    // Kopiere Key nach ValueLen (+7byte) in Buffer mit länge KeyLen
    memcpy(buffer+7,P->key,P->keyLen);
    // Kopiere Value nach Key (+7byte+keylen) in den Buffer mit länge ValueLen
    memcpy(buffer+7+P->keyLen,P->value,P->valueLen);

    while(true){
        long bytes_sent = 0;

        while(bytes_sent != buffer_len - 1){
            int bytes = send(s, buffer + bytes_sent, buffer_len - bytes_sent - 1, 0);
            if (bytes < 0){
                fprintf(stderr, "Sending data failed!\n");
                break;
            }
            bytes_sent += bytes;
        }
        close(s);
        return 0;
    }




}




int main(int argc, char** argv) {
    if (argc < 3) {
        printf("%s\n", "No enough args provided!");
        return 1;
    }

    char* port = argv[1];
    char* path = argv[2];

    int count = 0;



    struct addrinfo hints;
    struct addrinfo* res;
    struct addrinfo* p;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


    int status = getaddrinfo(NULL, port, &hints, &res);
    if (status != 0){
        fprintf(stderr, "%s\n", "getaddrinfo() failed!");
        return 1;
    }

    int s = -1;
    for(p = res; p != NULL;p = p->ai_next){
        s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (s == -1){
            continue;
        }

        int optval = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));


        status = bind(s, res->ai_addr, res->ai_addrlen);
        if (status != 0){
            close(s);
            continue;
        }

        break;
    }

    if (s == -1){
        fprintf(stderr, "%s\n", "Unable to create socket!");
        return 1;
    }

    if (status != 0){
        fprintf(stderr, "%s\n", "Failed to bind socket!");
        return 1;
    }

    status = listen(s, 1);
    if (status != 0){
        fprintf(stderr, "Listen failed!\n");
        return 1;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while(true){
        struct sockaddr_storage client_addr;
        socklen_t c_addr_size = sizeof(client_addr);
        int client = accept(s, (struct sockaddr*) &client_addr, &c_addr_size);
        fprintf(stderr, "%s\n", "Client accepted!");


        struct Packet incoming;
        recv_all(client, &incoming);

        long nsent = 0;

        close(client);

    }
#pragma clang diagnostic pop

    freeaddrinfo(res);
    return 0;
}
