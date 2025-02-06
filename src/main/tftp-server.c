#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

#ifndef ESP_IDF_VERSION
#include <pthread.h>
#endif

#include "tftp-server.h"
#include "fancylog2.h"

uint8_t tftp_buf[768];
UDP_SERVER tftp;

void udp_listen_port(UDP_SERVER *udp, int port){
	int opt=1;
	udp->shutdown=0;
	udp->sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(udp->sock<0){
		crash("Can't create socket");
	}
	udp->server_addr.sin_family=AF_INET;
	udp->server_addr.sin_port=htons(port);
	udp->server_addr.sin_addr.s_addr=inet_addr("0.0.0.0");
	udp->client_addr_length=sizeof(udp->client_addr);
	#ifndef _WIN32
	setsockopt(udp->sock, SOL_SOCKET, SO_REUSEPORT,(const char*) &opt, sizeof(opt));
	#endif
	setsockopt(udp->sock, SOL_SOCKET, SO_REUSEADDR,(const char*) &opt, sizeof(opt));
	
	//Bind to the set port and IP:
	if(bind(udp->sock, (struct sockaddr*)&udp->server_addr, sizeof(udp->server_addr)) < 0){
		crash("Can't bind to the port %d",port);
	}
	
	trace("Socket created successfully, waiting at port %d",port);
}

void udp_release(UDP_SERVER *udp){
	shutdown(udp->sock,0);
	close(udp->sock);
	udp->sock=-1;
}

void tftp_server_handle_packets(UDP_SERVER *udp){
	int len;
	
	while(!udp->shutdown){
		//Receive client's message:
		len=recvfrom(udp->sock, tftp_buf, sizeof(tftp_buf), 0,(struct sockaddr*)&udp->client_addr, &udp->client_addr_length);
		
		//trace("Received message from IP: %s and port: %i, packet %d bytes",inet_ntoa(udp->client_addr.sin_addr),ntohs(udp->client_addr.sin_port),len);
		//logbuf(tftp_buf,len);
		
		if(len<0){
			//something strange happened
			continue;
		}
		
		tftp_process_packet(udp, tftp_buf, len);
		
	}
}

void tftp_server_main(void* arg){
	udp_listen_port(&tftp,69);
	tftp_server_handle_packets(&tftp);
	udp_release(&tftp);
}

#ifdef ESP_IDF_VERSION
void tftp_server_start(){
	xTaskCreate(tftp_server_main, "tftp_server_main", 2048, NULL, 10, NULL);
}
#endif

void tftp_server_stop(){
	tftp.shutdown=1;
}

/*
int main(){
	//thread_t tftp_server
	//pthread_create(&f_tftp_server, NULL, l, NULL);
	tftp_server_main(0);
	
	return 0;
}
*/
