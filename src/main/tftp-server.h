#ifndef TFTP_SERVER_H
#define TFTP_SERVER_H

#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>


#define TFTP_TYPE_RRQ 1
#define TFTP_TYPE_WRQ 2
#define TFTP_TYPE_DATA 3
#define TFTP_TYPE_ACK 4
#define TFTP_TYPE_ERROR 5
#define TFTP_TYPE_MAX 5

#define TFTP_ERROR_NOT_DEFINED_SEE_ERROR_MESSAGE 0
#define TFTP_ERROR_FILE_NOT_FOUND 1
#define TFTP_ERROR_ACCESS_VIOLATION 2
#define TFTP_ERROR_DISK_FULL_OR_ALLOCATION_EXCEEDED 3
#define TFTP_ERROR_ILLEGAL_TFTP_OPERATION 4
#define TFTP_ERROR_UKNOWN_TRANSFER_ID 5
#define TFTP_ERROR_FILE_ALREADY_EXISTS 6
#define TFTP_ERROR_NO_SUCH_USER 7

typedef struct{
int sock;
int shutdown;
struct sockaddr_in server_addr,client_addr;
uint32_t client_addr_length;
} UDP_SERVER;

void tftp_server_main(void* arg);
void tftp_server_start();
void tftp_server_stop();
void tftp_server_handle_packets(UDP_SERVER *udp);
void tftp_process_packet(UDP_SERVER *udp, uint8_t *buf, int len);

#endif

