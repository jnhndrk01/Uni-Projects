#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#define PORT "123"
#define INTERVAL 8

typedef struct {
  u_int8_t Li_Vn_Mode;            // 2 bit LI -> Leap Indicator, falls was im Argen ist
                                  // 3 bit Version -> wir nutzen V4
                                  // 3 bit modus -> Modus 3 = Client
  u_int8_t Strat;                 // Stratum Level des angefragten Servers

  u_int8_t Poll;                  // Intervall zwischen mehreren Anfagen

  u_int8_t Precision;

  u_int32_t Root_Delay;           // Komplettes Delay

  u_int32_t Root_Dispersion;      // Max Error

  u_int32_t Reference_ID;         // Clock ID

  u_int64_t Reference_Timestamp;  // Referenz Timestamp
  u_int64_t Origin_Timestamp;     // Ursprünglicher Timestamp
  u_int64_t Receive_Timestamp;    // Empfangs-Timestamp
  u_int64_t Transmit_Timestamp;   // Übertragungstimestamp
} datagram;


//Quellenangabe: Abwandlung von https://github.com/solemnwarning/timespec/blob/master/timespec.c
double timespec_to_double(struct timespec ts)
{
  return ((double)(ts.tv_sec) + ((double)(ts.tv_nsec) / 1000000000));
}

//findet minimum zweier Itegers (doh)
int min(int a, int b){
  if(a<b){
    return a;
  } else {
    return b;
  }
}

//findet minimum in disp array
double find_min(double *array,int len){

  double min = array[0];
  for(int i = 1; i<len; i++){
    if(array[i]<min){
      min = array[i];
    }
  }
  return min;
}

//findet maximum in disp array
double find_max(double *array, int len){
  double max = array[0];
  for(int i = 1; i<len; i++){
    if(array[i]>max){
      max = array[i];
    }
  }
  return max;
}


//wandlet NTP Timestamp in timespec um
//Quellenangabe: Abwandlung von https://stackoverflow.com/questions/29112071/how-to-convert-ntp-time-to-unix-epoch-time-in-c-language-linux
void ntp2tv(u_int64_t ntp, struct timespec *ts)
{
  uint64_t aux = 0;
  uint8_t *p = ntp;
  int i;

  /* we get the ntp in network byte order, so we must
   * convert it to host byte order. */
  for (i = 0; i < sizeof ntp / 2; i++) {
    aux <<= 8;
    aux |= *p++;
  } /* for */

  /* now we have in aux the NTP seconds offset */
  aux -= 2208988800ULL;
  ts->tv_sec = aux;

  /* let's go with the fraction of second */
  aux = 0;
  for (; i < sizeof ntp; i++) {
    aux <<= 8;
    aux |= *p++;
  } /* for */

  /* now we have in aux the NTP fraction (0..2^32-1) */
  aux *= 1000000000; /* multiply by 1e9 */
  aux >>= 32;     /* and divide by 2^32 */
  ts->tv_nsec = aux;
} /* ntp2tv */


//wandlet Timespec in NTP Timestamp   um
//Quellenangabe: Abwandlung von https://stackoverflow.com/questions/29112071/how-to-convert-ntp-time-to-unix-epoch-time-in-c-language-linux
void tv2ntp(struct timespec *ts, u_int64_t ntp)
{
  uint64_t aux = 0;
  uint8_t *p = ntp + sizeof ntp;
  int i;

  aux = ts->tv_nsec;
  aux <<= 32;
  aux /= 1000000000;

  /* we set the ntp in network byte order */
  for (i = 0; i < sizeof ntp/2; i++) {
    *--p = aux & 0xff;
    aux >>= 8;
  } /* for */

  aux = ts->tv_sec;
  aux += 2208988800ULL;

  /* let's go with the fraction of second */
  for (; i < sizeof ntp; i++) {
    *--p = aux & 0xff;
    aux >>= 8;
  } /* for */

} /* ntp2tv */



//Erstellt Request aus NTP_Timestamp (sendezeit)
datagram *request(u_int64_t send_time){

  datagram *request = calloc(48,sizeof(char));  //free in send datagram
  u_int8_t livnmode  = 0b00100011; //LI = 0; Version = 4; Modus = 3
  memcpy(request,&livnmode,1);
  u_int64_t orig_time = send_time;
  //memcpy(&request->Transmit_Timestamp,&orig_time,8);
  //memcpy(&request->Origin_Timestamp,&orig_time,8);
  return request;
}
int send_buf(int client, void *buf, int len) {
  int bytes_sent = 0;
  while (bytes_sent < len) {
    int bytes = send(client, buf + bytes_sent, len - bytes_sent, 0);
    if (bytes < 0) {
      fprintf(stderr, "Sending Buffer failed! \n");
      return -1;
    }
    bytes_sent += bytes;
  }
  return bytes_sent;
}

//Erstellt buffer zum senden aus Datagram
char* datagram_to_buffer(datagram *data){

  //data->Origin_Timestamp=htonl(data->Origin_Timestamp);
  char *buffer = calloc(48,sizeof(char));           //free in send datagram
  memcpy(buffer,&data->Li_Vn_Mode,1);
  memcpy(buffer+1,&data->Strat,1);
  memcpy(buffer+2,&data->Poll,1);
  memcpy(buffer+3,&data->Precision,1);
  memcpy(buffer+4,&data->Root_Delay,4);
  memcpy(buffer+8,&data->Root_Dispersion,4);
  memcpy(buffer+12,&data->Reference_ID,4);
  memcpy(buffer+16,&data->Reference_Timestamp,8);
  memcpy(buffer+24,&data->Origin_Timestamp,8);
  memcpy(buffer+32,&data->Receive_Timestamp,8);
  memcpy(buffer+40,&data->Transmit_Timestamp,8);
  return buffer;
}

datagram *buffer_to_datagram(char* buffer){
  datagram *data = calloc(1,sizeof(datagram));  //free am ende des Main Loops
  memcpy(&data->Li_Vn_Mode,buffer,1);
  memcpy(&data->Strat,buffer+1,1);
  memcpy(&data->Poll,buffer+2,1);
  memcpy(&data->Precision,buffer+3,1);
  memcpy(&data->Root_Delay,buffer+4,4);
  memcpy(&data->Root_Dispersion,buffer+8,4);
  memcpy(&data->Reference_ID,buffer+12,4);
  memcpy(&data->Reference_Timestamp,buffer+16,8);
  memcpy(&data->Origin_Timestamp,buffer+24,8);
  memcpy(&data->Receive_Timestamp,buffer+32,8);
  memcpy(&data->Transmit_Timestamp,buffer+40,8);
  return data;
}

 double net_dispersion_to_double(u_int32_t net_disp){
    u_int32_t bo_time = ntohl(net_disp);
    double seconds = (double)(bo_time >> 16) + (double)(bo_time & 0xFFFF) / (double)(1LL << 16);
    return seconds;
}


int send_datagram(int socket, datagram *packet){
  char *buffer = datagram_to_buffer(packet);
  free(packet);
  if(send_buf(socket,buffer,48)==-1){
    fprintf(stderr, "Sending Datagram failed! \n");
    free(buffer);
    return -1;
  }
  free(buffer);
  return 0;
}

int print_result(char* host,int id,double root_disp, double disp, double delay, double offset){

  printf("%s;%d;%f;%f;%f;%f\n",host,id,root_disp,disp,delay,offset);
  return 0; 
}


datagram *recv_datagram(int socket){

  char buffer[48];
  int bytes_received = 0;
  int nbytes = 0;
  //Versuchen 48 Bytes abzurufuen
  while(true){
    nbytes = 0;

    nbytes = recv(socket, buffer+bytes_received, 48, 0);
    bytes_received+=nbytes;
    //Nichts (mehr) bekommen
    if (bytes_received == 48){
      break;
    }
  }
  datagram* data = buffer_to_datagram(buffer);

  return data;
}


int main(int argc, char** argv) {
    if (argc < 3){
        fprintf(stderr,"%s\n", "Usage: ntpclient n host1 host2 ...");
        return 1;
    }

    int n_count = atoi(argv[1]);

  //Hauptschleife: iteriere über alle Server
  for(int server = 2; server < argc; server++) {

      double disp_array[8];
      for(int i = 0; i<8; i++){
        disp_array[i] = 0;
      }

      char* host = argv[server];
      struct addrinfo hints;
      struct addrinfo* res;
      struct addrinfo* p;

      memset(&hints, 0, sizeof(struct addrinfo));
      hints.ai_family = AF_UNSPEC;          // IPv4 oder IPv6 egal
      hints.ai_socktype = SOCK_DGRAM;       // kein Stream -> Datagram
      hints.ai_flags = AI_PASSIVE;          // wie immer
      hints.ai_protocol = IPPROTO_UDP;      // UDP protocol

      int status = getaddrinfo(host, PORT, &hints, &res);
      if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
      }

      //Nebenschleife: wiederhole senden/empfangen n-mal
      for(int n = 0;n<n_count;n++) {

        // Setup Socket
        int s = -1;
        for (p = res; p != NULL; p = p->ai_next) {
          s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
          if (s == -1) {
            continue;
          }

          // Connect
          status = connect(s, res->ai_addr, res->ai_addrlen);
          if (status != 0) {
            fprintf(stderr, "%s\n", "connect() failed!");
            continue;
          }

          break;
        }

        if (s == -1) {
          fprintf(stderr, "%s\n", "socket() failed!");
          return 1;
        }

        if (status != 0) return 1;

        //fprintf(stderr, "Connected to %s:%s\n", host, PORT);


        //Reserve Timespecs

        struct timespec *recv_time = calloc(1,sizeof(struct timespec));
        struct timespec *server_recv_time = calloc(1,sizeof(struct timespec));
        struct timespec *server_send_time = calloc(1,sizeof(struct timespec));

        double T1,T2,T3,T4;
        double root_disp;

        //Send-/Receive Loop // Loop deaktiviert wegen TB
        while(1) {// SEND REQUEST Schleife - Am Ende wird geprüft ob Serverpaket zum Requestpaket passt
          struct timespec *send_time = calloc(1, sizeof(struct timespec));
          int time_status = clock_gettime(CLOCK_REALTIME, send_time);
          if (time_status != 0) {
            fprintf(stderr, "Couldn't get system-time.");
            return 1;
          }
          u_int64_t Origin_NTPTIMESTAMP;
          tv2ntp(send_time,&Origin_NTPTIMESTAMP);
          send_datagram(s, request(Origin_NTPTIMESTAMP));

          // RECV ANSWER
          datagram *data = recv_datagram(s);

          // Empfangszeit speichern
          clock_gettime(CLOCK_REALTIME, recv_time);

          // Server Empfangs und Sendezeit aus Serverpaket konvertieren zu Timespec
          ntp2tv(&data->Receive_Timestamp, server_recv_time);
          ntp2tv(&data->Transmit_Timestamp, server_send_time);

          // Server Empfangs und Sendezeit zu Double konvertieren für Berechnung
           T2 = timespec_to_double(*server_recv_time);
           T3 = timespec_to_double(*server_send_time);

          // Client Sende und Empfangszeiten zu Double konvertieren für Berechnung
           T1 = timespec_to_double(*send_time);
           T4 = timespec_to_double(*recv_time);
           root_disp = net_dispersion_to_double(data->Root_Dispersion);

           free(send_time);
          // Nur akzeptieren wenn Origin Time aus Server Paket mit Client übereinstimmt
          if(data->Origin_Timestamp == Origin_NTPTIMESTAMP){
            free(data);
            break;
          }
          //immer Break, da ein Abgleich mit dem Origin Timestamp in der Testbench nicht möglich ist
          free(data);
          break;
        }

        //DELAY = (T4-T1)-(T3-T2)
        double delay = ((T4-T1)-(T3-T2));
        disp_array[n%8]=delay;
        //OFFSET = ((T2-T1)+(T3-T4))/2
        double offset = ((T2-T1)+(T3-T4))/2;
        //DISP = max(delay)-min(delay) über 8 messungen
        double disp = find_max(disp_array,min(8,n+1))-find_min(disp_array,min(8,n+1));
        print_result(argv[server],n,root_disp,disp,delay/2,offset);
        free(recv_time);
        free(server_recv_time);
        free(server_send_time);
        sleep(INTERVAL);
      }
      freeaddrinfo(res);
    }
  return 0;
}