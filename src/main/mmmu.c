#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* TODO:
+add timeout on read
-add flush data from server
+buffer 1k
+move send commands to functions
+remove hardcoded creds
+send smiley every 5 min
-send anek every 12 min
+move state to struct
+not ping, if we posting smilies
+make -Wall
+check under valgrind
+make step();
+launch on ESP
*/

//#define _DEBUG

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef _WIN32
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"

#include "mmmu.h"

//#define _DEBUG
#include "fancylog2.h"

#define big2little16(num) (((num>>8)&0xFF) | ((num&0xFF)<<8))
#define big2little32(num) ((num>>24)&0xff) | ((num<<8)&0xff0000) | ((num>>8)&0xff00) | ((num<<24)&0xff000000);

#ifdef _DEBUG
char *mumble_packet_names[]={"Version", "UDPTunnel", "Authenticate", "Ping", "Reject", "ServerSync", "ChannelRemove", "ChannelState", "UserRemove", "UserState", "BanList", "TextMessage", "PermissionDenied", "ACL", "QueryUsers", "CryptSetup", "ContextActionModify", "ContextAction", "UserList", "VoiceTarget", "PermissionQuery", "CodecVersion", "UserStats", "RequestBlob", "ServerConfig", "SuggestConfig"};
#endif

#ifdef ESP_PLATFORM

#define IS_EINTR( ret ) ( ( ret ) == EINTR )

int mbedtls_net_poll( mbedtls_net_context *ctx, uint32_t rw, uint32_t timeout ){
	int ret;
	struct timeval tv;
	
	fd_set read_fds;
	fd_set write_fds;
	
	int fd=ctx->fd;
	
	if( fd < 0 )
	return( MBEDTLS_ERR_NET_INVALID_CONTEXT );
	
	#if defined(__has_feature)
	#if __has_feature(memory_sanitizer)
	/* Ensure that memory sanitizers consider read_fds and write_fds as
	* initialized even on platforms such as Glibc/x86_64 where FD_ZERO
	* is implemented in assembly. */
	memset( &read_fds, 0, sizeof( read_fds ) );
	memset( &write_fds, 0, sizeof( write_fds ) );
	#endif
	#endif
	
	FD_ZERO( &read_fds );
	if( rw & MBEDTLS_NET_POLL_READ ){
		rw &=~MBEDTLS_NET_POLL_READ;
		FD_SET( fd, &read_fds );
	}
	
	FD_ZERO( &write_fds );
	if( rw & MBEDTLS_NET_POLL_WRITE ){
		rw &=~MBEDTLS_NET_POLL_WRITE;
		FD_SET( fd, &write_fds );
	}
	
	if( rw != 0 )
	return( MBEDTLS_ERR_NET_BAD_INPUT_DATA );
	
	tv.tv_sec=timeout / 1000;
	tv.tv_usec=( timeout % 1000 ) * 1000;
	
	do
	{
		ret=select( fd + 1, &read_fds, &write_fds, NULL,
		timeout== (uint32_t) -1 ? NULL : &tv );
	}
	while( IS_EINTR( ret ) );
	
	if( ret < 0 )
	return( MBEDTLS_ERR_NET_POLL_FAILED );
	
	ret=0;
	if( FD_ISSET( fd, &read_fds ) )
	ret |=MBEDTLS_NET_POLL_READ;
	if( FD_ISSET( fd, &write_fds ) )
	ret |=MBEDTLS_NET_POLL_WRITE;
	
	return( ret );
}
#endif

MMMU_CLIENT_STATE *c;

static uint8_t tmpbuf[1024*8];

//global TLS state
mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context drbg;
mbedtls_x509_crt crt;
mbedtls_ssl_config ssl_conf;
mbedtls_pk_context pk_key;
static int tls_inited=0;

#ifdef _DEBUG
void my_mbedtls_debug(void *ctx, int level, const char *file, int line, const char *str){
	const char *p, *basename;
	for(p=basename=file;*p!='\0';p++){
		if(*p=='/'||*p=='\\'){
		basename=p+1;}
	}
	mbedtls_fprintf((FILE*)ctx,"%s:%04d: |%d| %s",basename,line,level,str);
	fflush((FILE *) ctx);
}

void my_mbedtls_strerror(int code){
	static char error_buf[100];
	mbedtls_strerror(code,error_buf,sizeof(error_buf));
	trace("mbedtls error: %s",error_buf);
	crash();
}

#else
void my_mbedtls_debug(void *ctx, int level, const char *file, int line, const char *str){
}
void my_mbedtls_strerror(int code){
}
#endif

int mmmu_packet_send(int packet_type, int message_size){
	static MUMBLE_PACKET_HEADER header;
	int can_write;
	
	header.type=big2little16(packet_type);
	header.size=big2little32(message_size);
	memcpy(tmpbuf,&header,6);
	
	can_write=mbedtls_net_poll(&c->net,MBEDTLS_NET_POLL_WRITE,0);
	if(!(can_write&MBEDTLS_NET_POLL_WRITE)){
		return -1;
	}
	mbedtls_ssl_write(&c->ssl,tmpbuf,message_size+MUMBLE_PROTO_PACKET_HEADER_LEN);
	c->last_packet=(uint64_t)time(0);
	return 0;
}

void mmmu_send_auth(){
	MumbleProto__Authenticate auth=MUMBLE_PROTO__AUTHENTICATE__INIT;
	auth.username=c->client_nickname;
	auth.password="";
	auth.n_celt_versions=1;
	auth.celt_versions=malloc(8);
	auth.celt_versions[0]=0x8000000B;
	auth.has_opus=1;
	auth.opus=1;
	auth.client_type=1;// bot
	int auth_size=mumble_proto__authenticate__get_packed_size(&auth);
	mumble_proto__authenticate__pack(&auth,tmpbuf+MUMBLE_PROTO_PACKET_HEADER_LEN);
	mmmu_packet_send(MUMBLE_PROTO_PACKET_TYPE_AUTHENTICATE,auth_size);
	free(auth.celt_versions);
}

void mmmu_send_state(){
	MumbleProto__UserState state=MUMBLE_PROTO__USER_STATE__INIT;
	state.session=c->session;
	state.has_self_mute=1;
	state.has_self_deaf=1;
	state.has_recording=1;
	state.self_mute=1;
	state.self_deaf=1;
	state.recording=c->client_is_recording;
	state.comment=c->client_comment;
	
	int state_size=mumble_proto__user_state__get_packed_size(&state);
	mumble_proto__user_state__pack(&state,tmpbuf+MUMBLE_PROTO_PACKET_HEADER_LEN);
	mmmu_packet_send(MUMBLE_PROTO_PACKET_TYPE_USERSTATE,state_size);
}

void mmmu_send_text(char *text, uint32_t channel){
	MumbleProto__TextMessage tm=MUMBLE_PROTO__TEXT_MESSAGE__INIT;
	tm.message=text;
	tm.n_channel_id=1;
	tm.channel_id=&channel;
	
	int text_size=mumble_proto__text_message__get_packed_size(&tm);
	mumble_proto__text_message__pack(&tm,tmpbuf+MUMBLE_PROTO_PACKET_HEADER_LEN);
	mmmu_packet_send(MUMBLE_PROTO_PACKET_TYPE_TEXTMESSAGE,text_size);
}

void mmmu_send_ping(){
	MumbleProto__Ping ping=MUMBLE_PROTO__PING__INIT;
	ping.has_timestamp=1;
	ping.timestamp=(uint64_t)time(0);
	int ping_size=mumble_proto__ping__get_packed_size(&ping);
	mumble_proto__ping__pack(&ping,tmpbuf+MUMBLE_PROTO_PACKET_HEADER_LEN);
	mmmu_packet_send(MUMBLE_PROTO_PACKET_TYPE_PING,ping_size);
}

void mmmu_send_version(){
	MumbleProto__Version ver=MUMBLE_PROTO__VERSION__INIT;
	ver.has_version_v1=1;
	ver.has_version_v2=1;
	ver.version_v1=66304;
	ver.version_v2=66304;
	ver.release="MMMU (Micro Mini Mumble)";
	ver.os="FreeRTOS";
	ver.os_version="ESP32";
	
	int ver_size=mumble_proto__version__get_packed_size(&ver);
	mumble_proto__version__pack(&ver,tmpbuf+MUMBLE_PROTO_PACKET_HEADER_LEN);
	mmmu_packet_send(MUMBLE_PROTO_PACKET_TYPE_VERSION,ver_size);
}

void mmmu_tls_init(MMMU_CLIENT_STATE *client){
	int ret;
	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_init(&drbg);
	mbedtls_x509_crt_init(&crt);
	mbedtls_ssl_config_init(&ssl_conf);
	mbedtls_pk_init(&pk_key);
	
	if(mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, NULL, 0)){
		crash();
	}
	
	if(mbedtls_ssl_config_defaults(&ssl_conf,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)){
		crash();
	}
	mbedtls_ssl_conf_authmode(&ssl_conf, MBEDTLS_SSL_VERIFY_NONE);
	mbedtls_ssl_conf_rng(&ssl_conf, mbedtls_ctr_drbg_random, &drbg);
	
	#ifdef ESP_PLATFORM
	
	#else
	
	mbedtls_ssl_conf_min_tls_version(&ssl_conf, MBEDTLS_SSL_VERSION_TLS1_2);
	mbedtls_ssl_conf_max_tls_version(&ssl_conf, MBEDTLS_SSL_VERSION_TLS1_3);
	mbedtls_ssl_conf_read_timeout(&ssl_conf,100);
	#ifdef _DEBUG
	mbedtls_ssl_conf_dbg(&ssl_conf,my_mbedtls_debug,stdout);
	mbedtls_debug_set_threshold(5);
	#endif
	
	#endif
	
	if(client->client_pem_cert && client->client_pem_cert[0]){
		ret=mbedtls_x509_crt_parse(&crt,client->client_pem_cert,strlen((const char*)client->client_pem_cert)+1);
		if(ret<0){
			my_mbedtls_strerror(ret);
			crash();
		}
		ret=mbedtls_pk_parse_key(&pk_key,client->client_pem_key,strlen((const char*)client->client_pem_key)+1,NULL,0, mbedtls_ctr_drbg_random, &drbg);
		if(ret<0){
			my_mbedtls_strerror(ret);
			crash();
		}
		ret=mbedtls_ssl_conf_own_cert(&ssl_conf, &crt, &pk_key);
		if(ret<0){
			my_mbedtls_strerror(ret);
			crash();
		}
	}
	
	tls_inited=1;
}

void mmmu_tls_shutdown(){
	mbedtls_ssl_config_free(&ssl_conf);
	mbedtls_x509_crt_free(&crt);
	mbedtls_pk_free(&pk_key);
	mbedtls_entropy_free(&entropy);
	mbedtls_ctr_drbg_free(&drbg);
	tls_inited=0;
}

void mmmu_init(MMMU_CLIENT_STATE *client){
	client->client_comment="";
	client->client_pem_cert=NULL;
	client->client_pem_key=NULL;
}

void mmmu_set_server(MMMU_CLIENT_STATE *client,char *host, char *port, char *nickname){
	client->server_host=host;
	client->server_port=port;
	client->client_nickname=nickname;
}

void mmmu_set_cert(MMMU_CLIENT_STATE *client,const unsigned char *cert, const unsigned char *key){
	client->client_pem_cert=cert;
	client->client_pem_key=key;
}

void mmmu_set_cert_file(MMMU_CLIENT_STATE *client,char *cert_filename, char *key_filename){
	crash();
}

void mmmu_set_comment(MMMU_CLIENT_STATE *client,char *comment){
	client->client_comment=comment;
}

void mmmu_set_recording(MMMU_CLIENT_STATE *client,int state){
	client->client_is_recording=state;
}

void mmmu_net_connect(MMMU_CLIENT_STATE *client){
	if(!tls_inited){
		mmmu_tls_init(client);
	}
	
	mbedtls_ssl_init(&client->ssl);
	mbedtls_net_init(&client->net);
	
	int ret=mbedtls_ssl_setup(&client->ssl, &ssl_conf);
	if(ret){
		my_mbedtls_strerror(ret);
		crash();
	}
	
	mbedtls_ssl_set_bio(&client->ssl, &client->net, mbedtls_net_send, mbedtls_net_recv, NULL);
	if(mbedtls_net_connect(&client->net,client->server_host,client->server_port,MBEDTLS_NET_PROTO_TCP)){
		crash();
	}
	
	mbedtls_ssl_handshake(&client->ssl);
	
	client->time_connected=client->time_alive=(int)time(0);
	client->root_channel=-1;
	client->session=-1;
	client->is_connected=0;
	client->errors=0;
	client->last_packet=0;
}

void mmmu_net_disconnect(MMMU_CLIENT_STATE *client){
	c->is_connected=0;
	//mbedtls_net_close(&c->net);
	mbedtls_net_free(&c->net);
	mbedtls_ssl_free(&c->ssl);
	//mmmu_tls_shutdown();
	//crash();
}

void mmmu_connect(MMMU_CLIENT_STATE *client){
	c=client;
	
	mmmu_net_connect(client);
	trace("connected");
	mmmu_send_version(client);
	trace("version sent");
	mmmu_step(client);
}

void mmmu_on_connect(){
	time_t now=time(0);
	mmmu_send_state();
	mmmu_send_text(ctime(&now),c->root_channel);
}

void mmmu_step(MMMU_CLIENT_STATE *client){
	static MUMBLE_PACKET_HEADER header;
	int chunk;
	int total_read=0, now_read;
	uint64_t now;
	
	while(1){
		now=(uint64_t)time(0);
		trace("now %llu",now);
		
		if(c->errors>5){
			trace("too many errors");
			mmmu_net_disconnect(client);
			crash();
		}
		
		if(now-c->last_packet>20){
			mmmu_send_ping();
			continue;
		}
		
		int can_read=mbedtls_net_poll(&client->net,MBEDTLS_NET_POLL_READ,0);
		trace("here new packet? %s",can_read?"yes":"no");
		if(!(can_read&MBEDTLS_NET_POLL_READ)){
			
			if(c->is_connected){
				return;
			} else {
				sleep(1);
			}
			
		}
		
		int header_len=mbedtls_ssl_read(&client->ssl,(void*)&header,MUMBLE_PROTO_PACKET_HEADER_LEN);
		
		trace("we have header: %d",header_len);
		if(header_len==0){
			mmmu_net_disconnect(client);
			crash();
		}
		
		if(header_len!=6){
			c->errors++;
			continue;
		}
		
		header.type=big2little16(header.type);
		header.size=big2little32(header.size);
		#ifdef _DEBUG
		trace("we got packet, type: %d (%s), len:%d",(int)header.type,mumble_packet_names[header.type>=MUMBLE_PROTO_PACKET_TYPE_UNUSED?0:header.type],(int)header.size);
		#endif
		
		if(header.type>=MUMBLE_PROTO_PACKET_TYPE_UNUSED){
			c->errors++;
			continue;
		}
		
		if(header.size>sizeof(tmpbuf)){
			//skip packet
			trace("we skipping too large packet");
			while(header.size){
				chunk=header.size;
				if(chunk>sizeof(tmpbuf)){
					chunk=sizeof(tmpbuf);
				}
				now_read=mbedtls_ssl_read(&client->ssl,tmpbuf,chunk);
				if(now_read>0){
					header.size-=now_read;
				} else {
					break;
				}
			}
			continue;
		}
		
		total_read=0;
		while(total_read!=header.size){
			now_read=mbedtls_ssl_read(&client->ssl,tmpbuf+total_read,header.size-total_read);
			trace("we read %d of %d",(int)total_read,(int)header.size);
			if(now_read>0){
				total_read+=now_read;
				continue;
			} else {
				c->errors++;
				continue;
			}
		}
		
		if(header.type==MUMBLE_PROTO_PACKET_TYPE_VERSION){
			mmmu_send_auth();
			continue;
		}
		
		if(header.type==MUMBLE_PROTO_PACKET_TYPE_REJECT){
			MumbleProto__Reject *reject=mumble_proto__reject__unpack(NULL,header.size,tmpbuf);
			trace("Our connection was declined: \"%s\"",reject->reason&&reject->reason[0]?reject->reason:"[no reason]");
			mumble_proto__reject__free_unpacked(reject,NULL);
			mmmu_net_disconnect(client);
			crash();
			break;
		}
		
		if(header.type==MUMBLE_PROTO_PACKET_TYPE_USERREMOVE){
			MumbleProto__UserRemove *rm=mumble_proto__user_remove__unpack(NULL,header.size,tmpbuf);
			if(rm->session==c->session){
				trace("We was kicked: \"%s\"",rm->reason&&rm->reason[0]?rm->reason:"[no reason]");
				mumble_proto__user_remove__free_unpacked(rm,NULL);
				mmmu_net_disconnect(client);
				crash();
				break;
			}
			mumble_proto__user_remove__free_unpacked(rm,NULL);
		}
		
		if(header.type==MUMBLE_PROTO_PACKET_TYPE_CHANNELSTATE){
			MumbleProto__ChannelState *cs=mumble_proto__channel_state__unpack(NULL,header.size,tmpbuf);
			if(cs->has_channel_id && cs->has_can_enter && cs->can_enter==1 && !cs->has_parent){
				c->root_channel=cs->channel_id;
				trace("Setting ROOT to %d",c->root_channel);
			}
			mumble_proto__channel_state__free_unpacked(cs,NULL);
			continue;
		}
		
		if(header.type==MUMBLE_PROTO_PACKET_TYPE_SERVERSYNC){
			MumbleProto__ServerSync *ss=mumble_proto__server_sync__unpack(NULL,header.size,tmpbuf);
			
			if(ss->has_session){
				c->is_connected=1;
				c->session=ss->session;
			}
			mumble_proto__server_sync__free_unpacked(ss,NULL);
			mmmu_on_connect();
			continue;
		}
		
	}
}

#ifdef _DEBUG0
void http_test(){
	char reb[4096];
	#define SERVER "opennet.ru"
	#define SERVER_PORT "443"
	
	char *req="GET / HTTP/1.0\r\nHost: " SERVER "\r\n\r\n";
	int w,s;
	net_sock_connect();
	w=mbedtls_ssl_write(&ssl,req,strlen(req));
	s=mbedtls_ssl_read(&ssl,(unsigned char*)reb,sizeof(reb));
	printf("ret: %.*s",s,reb);
	crash();
}
#endif
