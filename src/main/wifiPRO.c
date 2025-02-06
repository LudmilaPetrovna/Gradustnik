#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#include "esp_err.h"
#include "esp_attr.h"
#include "esp_log.h"

#include "esp_spiffs.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"
#include "esp_wifi.h"

#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define BUFSIZE 1024

char buf[BUFSIZE];

static const char *TAG="wifiPRO";

int wifipro_connected=0;
int wifipro_shutdown=0;
int wifipro_sntp_inited=0;

typedef struct{
	char *ssid;
	uint8_t bssid[6];
	int is_hidden;
	int channel;
	char *key;
	uint8_t sta_mac[6];
	uint32_t ip;
	uint32_t netmask;
	uint32_t gw;
	uint32_t dns1;
	uint32_t dns2;
	char *ntp;
	char *portal;
	//on fail: try again, use next, be AP???
	//set channel/20-40mhz
	//use DHCP: mac of DHCP
	//esp_wifi_get_country_code
	
} WIFI_OPTS;

WIFI_OPTS w;
esp_netif_t *sta_netif;

void hex2mac(char *src, uint8_t *dst){
	uint8_t b=0,sp=0,cn;
	while(sp<12){
		cn=src[sp++];
		if(cn>='a' && cn<='z'){
		cn-='a'-'A';}
		if(!cn){
		break;}
		b=(b<<4)|(cn>='0'&&cn<='9'?cn-'0':(cn>='A'&&cn<='F'?cn-'A'+10:0));
		if((sp&1)==0){
			*dst++=b;
			b=0;
		}
	}
}

void wifipro_read_conf(){
	esp_vfs_spiffs_conf_t conf={
		.base_path="/wifiPRO",
		.partition_label="wifiPRO",
		.max_files=1,
		.format_if_mount_failed=false
	};
	esp_err_t ret=esp_vfs_spiffs_register(&conf);
	if (ret != ESP_OK){
		if (ret == ESP_FAIL){
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
			} else if (ret == ESP_ERR_NOT_FOUND){
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return;
	}
	
	ESP_LOGI(TAG, "Opening file");
	
	char *lines[20];
	
	int f=open("/wifiPRO/wifi.txt",O_RDONLY);
	int s=read(f,buf,BUFSIZE);
	close(f);
	
	if(memcmp(buf,"##WIFI##",8)!=0){
		printf("Broken file config!!!\n");
		abort();
	}
	
	int p=0;
	int pp=0;
	int nl=1;
	while(p<s && pp<20){
		if(buf[p]=='\n'){
			nl=1;
			buf[p++]=0;
			continue;
		}
		if(nl){
			lines[pp++]=buf+p;
			nl=0;
		}
		p++;
	}
	
	w.ssid=lines[1];
	hex2mac(lines[2],w.bssid);
	w.is_hidden=memcmp(lines[3],"ISHIDDEN",8)==0?1:0;
	w.channel=atoi(lines[4]);
	w.key=lines[5];
	hex2mac(lines[6],w.sta_mac);
	w.ip=ipaddr_addr(lines[7]);
	w.netmask=ipaddr_addr(lines[8]);
	w.gw=ipaddr_addr(lines[9]);
	w.dns1=ipaddr_addr(lines[10]);
	w.dns2=ipaddr_addr(lines[11]);
	w.ntp=lines[12];
	w.portal=lines[13];
	//printf("Connecting to %.6s (%.6s) (hidden? %d)\n",w.ssid,w.bssid,w.is_hidden);
	//printf("IP %lx/%lx, gw %lx, DNS %lx/%lx, ntp: %s\n",w.ip,w.netmask,w.gw,w.dns1,w.dns2,w.ntp);
	
}

void wifipro_check_inet_connection(){
	int sockfd, portno=80, n;
	struct sockaddr_in serveraddr;
	struct hostent *server;
	char *hostname="clients1.google.com";
	server=gethostbyname(hostname);
	if (server == NULL){
		fprintf(stderr,"ERROR, no such host as %s\n", hostname);
		exit(0);
	}
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family=AF_INET;
	bcopy((char *)server->h_addr,(char *)&serveraddr.sin_addr.s_addr, server->h_length);
	serveraddr.sin_port=htons(portno);
	printf("server resolved as: %s\n", inet_ntoa(serveraddr.sin_addr));
	sockfd=socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	
	if (sockfd < 0){
		ESP_LOGE(TAG, "ERROR opening socket");
	}
	if (connect(sockfd,(const struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
		ESP_LOGE(TAG, "ERROR connecting");
	}
	snprintf(buf,BUFSIZE,"GET /generate_204 HTTP/1.0\r\n\r\n");
	
	n=send(sockfd, buf, strlen(buf),0);
	if (n < 0){
		ESP_LOGE(TAG, "ERROR writing to socket");
	}
	bzero(buf, BUFSIZE);
	n=recv(sockfd, buf, BUFSIZE,0);
	if (n < 0){
		ESP_LOGE(TAG, "ERROR reading from socket");
	}
	printf("Google service:\r\n%s", buf);
	shutdown(sockfd, SHUT_RDWR);
	close(sockfd);
}

static void wifipro_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
		printf("EVENT: STA_START\n");
		} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED){
		printf("EVENT: WIFI_EVENT_STA_CONNECTED\n");
		
		//wifi_event_sta_connected_t *ce=( wifi_event_sta_connected_t*)event_data;
		
		//printf("EVENT: WIFI_EVENT_STA_CONNECTED to: %.*s, channel %d, bssid: %.6s\n",ce->ssid_len,ce->ssid,ce->channel,ce->bssid);
		
		esp_netif_dhcpc_stop(sta_netif);
		
		esp_netif_ip_info_t ip;
		memset(&ip, 0 , sizeof(esp_netif_ip_info_t));
		ip.ip.addr=w.ip;
		ip.netmask.addr=w.netmask;
		ip.gw.addr=w.gw;
		if(esp_netif_set_ip_info(sta_netif,&ip)!=ESP_OK) {
			ESP_LOGE(TAG, "Failed to set ip info");
			return;
		}
		
		esp_netif_dns_info_t dns;
		dns.ip.type=IPADDR_TYPE_V4;
		
		dns.ip.u_addr.ip4.addr=w.dns1;
		ESP_ERROR_CHECK(esp_netif_set_dns_info(sta_netif,ESP_NETIF_DNS_MAIN,&dns));
		dns.ip.u_addr.ip4.addr=w.dns2;
		ESP_ERROR_CHECK(esp_netif_set_dns_info(sta_netif,ESP_NETIF_DNS_BACKUP,&dns));
		
		//if(!wifipro_sntp_inited){
		esp_sntp_config_t config=ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(0,{});
		config.start=false;
		config.server_from_dhcp=false;
		esp_sntp_setservername(0,strdup(w.ntp));
		esp_netif_sntp_init(&config);
		wifipro_sntp_inited=1;
		
		esp_netif_sntp_start();
		
		wifipro_connected=1;
		
		} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
		wifipro_connected=0;
		esp_netif_sntp_deinit();
		
		printf("EVENT: WIFI_EVENT_STA_DISCONNECTED\n");
		if(!wifipro_shutdown){
			ESP_ERROR_CHECK(esp_wifi_connect());
		}
		
		} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
		printf("EVENT: IP_EVENT_STA_GOT_IP\n");
		
		ip_event_got_ip_t* event=(ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
	}
}

void wifipro_connect(){
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK( esp_event_loop_create_default() );
	sta_netif=esp_netif_create_default_wifi_sta();
	assert(sta_netif);
	
	wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
	cfg.nvs_enable=0;
	cfg.nano_enable=0;
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&wifipro_event_handler,NULL,NULL));
	
	wifi_config_t wifi_config={};
	memcpy(wifi_config.sta.bssid,w.bssid,sizeof(wifi_config.sta.bssid));
	strlcpy((char *) wifi_config.sta.ssid, (char *)w.ssid, sizeof(wifi_config.sta.ssid));
	strlcpy((char *) wifi_config.sta.password, (char *)w.key, sizeof(wifi_config.sta.password));
	wifi_config.sta.channel=w.channel;
	wifi_config.sta.bssid_set=1;
	wifi_config.sta.threshold.authmode=WIFI_AUTH_WPA2_PSK;
	wifi_config.sta.failure_retry_cnt=0;
	wifi_config.sta.scan_method=WIFI_FAST_SCAN;
	
	/*
	scan.ssid=wifi_config.sta.ssid;
	scan.bssid=wifi_config.sta.bssid;
	scan.channel=w.channel;
	scan.show_hidden=1;
	scan.scan_type=WIFI_SCAN_TYPE_ACTIVE;
	*/
	
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_set_mac(WIFI_IF_STA,w.sta_mac));
	ESP_ERROR_CHECK(esp_wifi_start() );
	ESP_ERROR_CHECK(esp_wifi_connect());
	
}

int wifipro_disconnect(){
	wifipro_shutdown=1;
	esp_wifi_stop();
	return 0;
}
