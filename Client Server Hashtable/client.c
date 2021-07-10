
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#define GET_REQ 0b00000100
#define SET_REQ 0b00000010
#define DEL_REQ 0b00000001
#define GET "GET"
#define SET "SET"
#define DELETE "DELETE"

struct Packet {
    char flags;
    uint16_t keyLen;
    uint32_t valueLen;
    char *key;
    char *value;
} __attribute__((packed));





int recv_req(int s, struct Packet* P){

    char buffer[7];
    int bytes_received = 0;
    int nbytes = 0;
    //7 bytes abrufen für header + val/key längen
    while(true){
        nbytes = 0;
        //7 bytes abrufen für header + key_len + val_len
        nbytes = recv(s, buffer, 7, 0);
        //printf("Received %i header-bytes. \n",nbytes);
        memcpy(&P->flags + bytes_received,buffer,nbytes);
        bytes_received+=nbytes;
        //Nichts (mehr) bekommen
        if (bytes_received == 7){
            break;
        }
    }

    P->keyLen = ntohs(P->keyLen);
    P->valueLen = ntohl(P->valueLen);
    //printf("Keylen: %i. \n Valuelen: %i \n",P->keyLen, P->valueLen);
    return bytes_received;
}

//receiven des Keys

int recv_keyval(int s, struct Packet* P){

    if(P->keyLen > 0) {P->key = calloc(P->keyLen,sizeof(char));}

    if(P->valueLen > 0) {P->value = calloc(P->valueLen,sizeof(char));}

    char *buffer = calloc(P->keyLen+P->valueLen,sizeof(char));

    char *solid_buffer = calloc(P->keyLen+P->valueLen,sizeof(char));

    int bytes_received = 0;

    while(true){

        int nbytes = 0;

        //Wenn noch nicht alle erwarteten Bytes angekommen sind.
        if(bytes_received<P->keyLen+P->valueLen){
            nbytes = recv(s, buffer, P->keyLen+P->valueLen, 0);

            //Wenn keine Bytes mehr gesendet wurden
            if (nbytes == 0){
                break;
            }

            //Aus dem flüchtigen in den festen Buffer kopieren
            memcpy(solid_buffer+bytes_received,buffer,nbytes);

            //Empfangene Bytes hochzählen
            bytes_received+=nbytes;
        } else {
            break;
        }
    }
    //Key aus dem festen Buffer in das Packet kopieren
    memcpy(P->key,solid_buffer,P->keyLen);
    memcpy(P->value,solid_buffer+P->keyLen,P->valueLen);
    free(solid_buffer);
    free(buffer);

    return bytes_received;

}

// receiven des Values

// receiven von header value und key kombiniert. Value wird nur received, wenn set bit gesetzt ist.
int recv_all(int s, struct Packet* P){
    int numbyte1 = recv_req(s,P);
    int numbyte2 = recv_keyval(s,P);
/*    int numbyte3 = 0;
    if(P->flags == SET_REQ){
        numbyte3 = recv_value(s,P);
    }*/
    return numbyte1+numbyte2;
}


int send_buf(int client, void* buf, int len){
    int bytes_sent = 0;
    while(bytes_sent < len){
        int bytes = send(client, buf+bytes_sent, len-bytes_sent,0);
        if(bytes < 0){
            fprintf(stderr, "Sending data failed! \n");
        }
        bytes_sent += bytes;
    }
    return bytes_sent;
}







int send_pack (int client, struct Packet* P, int ack){


    // Flags senden und prüfen
    int flag_status = send_buf(client, &P->flags,1);
    if(flag_status < 0) {
        //Error
    }
    // Keylen und ValueLen in network byteorder konvertieren
    uint16_t keylen_bo = htons(P->keyLen);
    uint32_t vallen_bo = htonl(P->valueLen);

    // Keylen senden
    int key_len_status = send_buf(client,&keylen_bo,2);
    if(key_len_status < 0) {
        //Error
    }

    // ValueLen senden
    int val_len_status = send_buf(client,&vallen_bo,4);
    if(val_len_status < 0) {
        //Error
    }

        int key_status = send_buf(client,P->key,P->keyLen);
        if(key_status < 0) {
            //Error
        }

        // Value senden
        int value_status = send_buf(client,P->value,P->valueLen);
        if(value_status < 0) {
            //Error
        }


}int freepacket(struct Packet *P){
    if(P != NULL) {
        if(P->value != NULL){
            free(P->value);
        }
        if(P->key != NULL){
            free(P->key);
        }
        free(P);
        return 0;
    }
    return -1;
}







int main(int argc, char** argv) {
    if (argc != 5){
        fprintf(stderr,"%s\n", "Usage: ./client host port CMD key");
        return 1;
    }

    char* host = argv[1];
    char* service = argv[2];
    char* operation = argv[3];
    char* key = argv[4];
    uint16_t keylen_input = strlen(argv[4]);
    uint32_t no_value_op = -1;


    //Command prüfen und flags setzen
    uint8_t flags = 0;
    if(strcmp(operation,GET) == 0) {
        flags = GET_REQ;
        no_value_op = 0;
    } else if(strcmp(operation,SET) == 0){
        flags = SET_REQ;
    } else if(strcmp(operation,DELETE) == 0){
        flags = DEL_REQ;
        no_value_op = 0;
    } else {
        printf("%s is not a valid command! \n",operation);
        //fprintf(stderr,"%s\n", "Not a valid command!");
        return 1;
    }
    //Allocate Packet
    struct Packet* packet_to_send = calloc(1,sizeof(struct Packet));

    //READ INPUT
    if(no_value_op == -1) {
        char *input_buffer = calloc(512, sizeof(char));

        int bytes_written_to_buffer = 0;
        int space_in_buffer = 512;

        while (true) {
            int read_status = read(STDIN_FILENO, input_buffer, space_in_buffer);

            bytes_written_to_buffer += read_status;
            space_in_buffer -= read_status;
            if (read_status == 0) {
                break;
            }
            if (read_status == -1) {
                //Error
            }
            if (space_in_buffer == 0) {
                input_buffer = realloc(input_buffer, bytes_written_to_buffer + 512);
                space_in_buffer += 512;
            }
        }

        packet_to_send->value = calloc(bytes_written_to_buffer, sizeof(char));
        memcpy(packet_to_send->value, input_buffer, bytes_written_to_buffer);
        packet_to_send->valueLen = bytes_written_to_buffer;
        free(input_buffer);
    } else {
        packet_to_send->valueLen = 0;
    }

    packet_to_send->keyLen = keylen_input;
    packet_to_send->key = calloc(keylen_input,sizeof(char));
    memcpy(packet_to_send->key,key,keylen_input);

    packet_to_send->flags = flags;




    //NETWORK

    struct addrinfo hints;
    struct addrinfo* res;
    struct addrinfo* p;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


    int status = getaddrinfo(host, service, &hints, &res);
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


        status = connect(s, res->ai_addr, res->ai_addrlen);
        if (status != 0){
            fprintf(stderr, "%s\n", "connect() failed!");
            continue;
        }

        break;
    }

    if (s == -1){
        fprintf(stderr, "%s\n", "socket() failed!");
        return 1;
    }

    if(status != 0) return 1;


    fprintf(stderr, "Connected to %s:%s\n", host, service);

    //Sending Request
    send_pack(s,packet_to_send,0);

    //Receive Answer
    struct Packet* response = calloc(1, sizeof(struct Packet));
    recv_all(s,response);

    //Print Answer if GET Request
    if(strcmp(operation,GET) == 0) {
        fwrite(response->value, sizeof(char), response->valueLen, stdout);
    }

    //free Packet
    freepacket(response);
    freepacket(packet_to_send);
    freeaddrinfo(res);

    return 0;
}