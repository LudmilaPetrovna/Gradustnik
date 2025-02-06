#ifndef MMMU_H
#define MMMU_H

#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"

#include "Mumble.pb-c.h"

#define MUMBLE_PROTO_PACKET_TYPE_VERSION 0
#define MUMBLE_PROTO_PACKET_TYPE_UDPTUNNEL 1
#define MUMBLE_PROTO_PACKET_TYPE_AUTHENTICATE 2
#define MUMBLE_PROTO_PACKET_TYPE_PING 3
#define MUMBLE_PROTO_PACKET_TYPE_REJECT 4
#define MUMBLE_PROTO_PACKET_TYPE_SERVERSYNC 5
#define MUMBLE_PROTO_PACKET_TYPE_CHANNELREMOVE 6
#define MUMBLE_PROTO_PACKET_TYPE_CHANNELSTATE 7
#define MUMBLE_PROTO_PACKET_TYPE_USERREMOVE 8
#define MUMBLE_PROTO_PACKET_TYPE_USERSTATE 9
#define MUMBLE_PROTO_PACKET_TYPE_BANLIST 10
#define MUMBLE_PROTO_PACKET_TYPE_TEXTMESSAGE 11
#define MUMBLE_PROTO_PACKET_TYPE_PERMISSIONDENIED 12
#define MUMBLE_PROTO_PACKET_TYPE_ACL 13
#define MUMBLE_PROTO_PACKET_TYPE_QUERYUSERS 14
#define MUMBLE_PROTO_PACKET_TYPE_CRYPTSETUP 15
#define MUMBLE_PROTO_PACKET_TYPE_CONTEXTACTIONMODIFY 16
#define MUMBLE_PROTO_PACKET_TYPE_CONTEXTACTION 17
#define MUMBLE_PROTO_PACKET_TYPE_USERLIST 18
#define MUMBLE_PROTO_PACKET_TYPE_VOICETARGET 19
#define MUMBLE_PROTO_PACKET_TYPE_PERMISSIONQUERY 20
#define MUMBLE_PROTO_PACKET_TYPE_CODECVERSION 21
#define MUMBLE_PROTO_PACKET_TYPE_USERSTATS 22
#define MUMBLE_PROTO_PACKET_TYPE_REQUESTBLOB 23
#define MUMBLE_PROTO_PACKET_TYPE_SERVERCONFIG 24
#define MUMBLE_PROTO_PACKET_TYPE_SUGGESTCONFIG 25
#define MUMBLE_PROTO_PACKET_TYPE_UNUSED 26

#define MUMBLE_PROTO_PACKET_HEADER_LEN 6

typedef struct{
char *server_host;
char *server_port;
char *client_nickname;
const unsigned char *client_pem_cert;
const unsigned char *client_pem_key;
char *client_comment;
int client_is_recording;
int is_connected;

int time_connected;
int time_alive;
int root_channel;
int session;
int errors;
uint64_t last_packet;
mbedtls_ssl_context ssl;
mbedtls_net_context net;
} MMMU_CLIENT_STATE;

#pragma pack(1)
typedef struct{
uint16_t type;
uint32_t size;
}MUMBLE_PACKET_HEADER;

void mmmu_init(MMMU_CLIENT_STATE *client);
void mmmu_set_server(MMMU_CLIENT_STATE *client,char *host, char *port, char *nickname);
void mmmu_set_cert(MMMU_CLIENT_STATE *client,const unsigned char *cert, const unsigned char *key);
void mmmu_set_cert_file(MMMU_CLIENT_STATE *client,char *cert_filename, char *key_filename);
void mmmu_set_comment(MMMU_CLIENT_STATE *client,char *comment);
void mmmu_set_recording(MMMU_CLIENT_STATE *client,int state);

void mmmu_say(MMMU_CLIENT_STATE *client,char *message);
void mmmu_send_text(char *text, uint32_t channel);
void mmmu_send_ping();

void mmmu_connect(MMMU_CLIENT_STATE *client);
void mmmu_disconnect(MMMU_CLIENT_STATE *client);
void mmmu_step(MMMU_CLIENT_STATE *client);

#endif
