#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"

#include "tftp-server.h"
#include "fancylog2.h"
#include "wifiPRO.h"

#define ACTION_NONE 0
#define ACTION_READ 1
#define ACTION_WRITE 2

/*
ENABLE IN PROJECT:
SPI_FLASH_DANGEROUS_WRITE
*/

/* TODO:

/hello.txt -> Hello!
/compiled.txt -> __DATE__ __TIME__
/time.txt -> ctime(time(0))
/info.txt -> system info and partitions
/reboot(.txt;.bin) -> system reboot
/part_ota.bin -> read/write, erase + set partition on write
/part_ota.bin.gz -> read/write using gzip compression
/part_ota.sha256 -> read checksum
/erase_ota
/activate_ota

*/

#define HELLO_MSG "Hello!"

int serve_hello(char *buf){
	int l=strlen(HELLO_MSG);
	strcpy(buf,"Hello!");
	return(l);
}

int serve_compiled(char *buf){
	char *msg="Compiled at: " __DATE__ " " __TIME__;
	int l=strlen(msg);
	strcpy(buf,msg);
	return(l);
}

int serve_time(char *buf){
	time_t now=time(0);
	char *msg=ctime(&now);
	int l=strlen(msg);
	strcpy(buf,msg);
	return(l);
}

int serve_info(char *buf){
	char *p=(char*)buf;
	p+=sprintf(p,"System info:\n\n");
	
	uint32_t chip_id;
	esp_flash_read_id(NULL, &chip_id);
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	p+=sprintf(p,"Chip id: %d\nModel: %d (%s)\nCores: %d\nRevision: %d\nFeatures: ",(int)chip_id,(int)chip_info.model,CONFIG_IDF_TARGET,(int)chip_info.cores,(int)chip_info.revision);
	if(chip_info.features&CHIP_FEATURE_BT){
	p+=sprintf(p,"BT; ");}
	if(chip_info.features&CHIP_FEATURE_BLE){
	p+=sprintf(p,"BLE; ");}
	if(chip_info.features&CHIP_FEATURE_WIFI_BGN){
	p+=sprintf(p,"WIFI_BGN; ");}
	if(chip_info.features&CHIP_FEATURE_IEEE802154){
	p+=sprintf(p,"802.15.4 (Zigbee/Thread); ");}
	if(chip_info.features&CHIP_FEATURE_EMB_FLASH){
	p+=sprintf(p,"EMB_FLASH; ");}
	if(chip_info.features&CHIP_FEATURE_EMB_PSRAM){
	p+=sprintf(p,"EMB_PSRAM; ");}
	p+=sprintf(p,"\n\n");
	
	const esp_app_desc_t *app=esp_app_get_description();
	p+=sprintf(p,"Application: %s (%s)\nCreated at: %s %s by %s\n\n",app->project_name,app->version,app->date,app->time,app->idf_ver);
	
	uint32_t flash_size;
	if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
		p+=sprintf(p,"ROM: Can't get flash size!\n");
	} else {
		p+=sprintf(p,"ROM: %" PRIu32 "MB\n",flash_size/1024/1024);
	}
	
	p+=sprintf(p,"RAM: min heap: %" PRIu32 " bytes, free: %" PRIu32 "\n",esp_get_minimum_free_heap_size(),esp_get_free_heap_size());
	p+=sprintf(p,"Last reset reason: %d\n",(int)esp_reset_reason());
	p+=sprintf(p,"Compiled at " __DATE__ " " __TIME__ " with %d",(int)esp_get_idf_version());
	return(strlen(buf));
}

int opcode,read_id=0;
char *filename;
int ret_len,ret_code,ret_block;
esp_partition_t *part=NULL;
int action=-1;
int block_id;
int chunk;

void tftp_process_packet(UDP_SERVER *udp, uint8_t *buf, int len){
	opcode=buf[1];
	ret_block=-1;
	ret_len=-1;
	ret_code=-1;
	
	if(opcode>TFTP_TYPE_MAX){
		trace("Invalid TFTP command");
		ret_code=TFTP_TYPE_ERROR;
		ret_block=TFTP_ERROR_ILLEGAL_TFTP_OPERATION;
		strcpy((char*)(buf+4),"Illegal OP");
		ret_len=strlen((char*)(buf+4))+5; // string + NULL
	}
	
	if(opcode==TFTP_TYPE_RRQ){
		//somebody want read from our server!
		filename=(char*)(buf+2);
		part=NULL;
		action=ACTION_NONE;
		
		if(strstr(filename,"hello.txt")){
			ret_len=serve_hello((char*)(buf+4))+4;
			ret_code=TFTP_TYPE_DATA;
			ret_block=1;
		}
		
		if(strstr(filename,"compiled.txt")){
			ret_len=serve_compiled((char*)(buf+4))+4;
			ret_code=TFTP_TYPE_DATA;
			ret_block=1;
		}
		
		if(strstr(filename,"time.txt")){
			ret_len=serve_time((char*)(buf+4))+4;
			ret_code=TFTP_TYPE_DATA;
			ret_block=1;
		}
		
		if(strstr(filename,"info.txt")){
			ret_len=serve_info((char*)(buf+4))+4;
			ret_code=TFTP_TYPE_DATA;
			ret_block=1;
		}
		
		if(strstr(filename,"reboot")){
			esp_restart();
		}
		
		if(strstr(filename,"part_")==filename){
			part=esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY,filename+5);
			if(part){
				action=ACTION_READ;
				read_id=1;
			} else {
				ret_code=TFTP_TYPE_ERROR;
				ret_block=TFTP_ERROR_FILE_NOT_FOUND;
			}
		}
		
	}
	
	if(opcode==TFTP_TYPE_WRQ){
		//somebody want to write to our server!
		//we got new firmware
		//flasher_init();
		filename=(char*)(buf+2);
		part=NULL;
		action=ACTION_NONE;
		
		if(strstr(filename,"reboot")==filename){
			esp_restart();
		}
		
		if(strstr(filename,"erase_")==filename){
			part=esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY,filename+6);
			if(part){
				esp_partition_erase_range(part, 0, part->size);
			}
		}
		
		if(strstr(filename,"activate_")==filename){
			part=esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY,filename+9);
			if(part){
				esp_ota_set_boot_partition(part);
			}
		}
		
		if(strstr(filename,"part_")==filename){
			trace("searching for partition");
			part=esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY,filename+5);
			if(part){
				trace("found %p, erasing...",part);
				esp_partition_erase_range(part, 0, part->size);
				trace("erased, settiing active...");
				esp_ota_set_boot_partition(part);
				trace("active, setting write session...");
				
				action=ACTION_WRITE;
				block_id=1;
			} else {
				ret_code=TFTP_TYPE_ERROR;
				ret_block=TFTP_ERROR_FILE_NOT_FOUND;
			}
			
		}
		
		ret_code=TFTP_TYPE_ACK;
		ret_block=0;
		ret_len=4;
	}
	
	if(opcode==TFTP_TYPE_DATA && action==ACTION_WRITE && part){
		//we got new data packet!
		block_id=(buf[2]<<8)|buf[3];
		//we got new data block
		//flasher_write(tftp_buf+4,len-4,block_id);
		chunk=len-4;
		if(chunk>0){
			ESP_ERROR_CHECK(esp_partition_write(part, (block_id-1)*512, buf+4, chunk));
		}
		
		if(chunk!=512){
			trace("disconnecting...");
			
			wifipro_disconnect();
			trace("setting partition as active...");
			ESP_ERROR_CHECK(esp_ota_set_boot_partition(part));
			trace("done...");
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			trace("reboot...");
			esp_restart();
		}
		
		ret_code=TFTP_TYPE_ACK;
		ret_block=block_id;
		ret_len=4;
	}
	
	if(opcode==TFTP_TYPE_ACK){
		//somebody got out packet
		if(action!=ACTION_READ){
			return;
		}
		//we can send next packet
		
	}
	
	if(opcode==TFTP_TYPE_ERROR){
		//something goes totally wrong
		action=ACTION_NONE;
		block_id=0;
		return;
	}
	
	if(action==ACTION_READ && part && read_id>0){
		chunk=part->size-(read_id-1)*512;
		if(chunk>512){
		chunk=512;}
		if(chunk>0){
			ESP_ERROR_CHECK(esp_partition_read(part, (read_id-1)*512, buf+4, chunk));
		}
		ret_len=chunk+4;
		ret_code=TFTP_TYPE_DATA;
		ret_block=read_id;
		read_id++;
	}
	
	if(ret_code==TFTP_TYPE_ERROR && ret_len<0){
		buf[4]=0;
		ret_len=5;
	}
	
	buf[0]=0;
	buf[1]=ret_code;
	buf[2]=ret_block>>8;
	buf[3]=ret_block&0xff;
	
	if(ret_len>0){
		//trace("Sending reply %d bytes",ret_len);
		logbuf(tftp_buf,ret_len);
		
		if(sendto(udp->sock,buf,ret_len,0,(struct sockaddr*)&udp->client_addr,udp->client_addr_length)<0){
			trace("Can't send reply");
		}
	}
	
}
