#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "flasher.h"

#define DEFAULT_FLASH_OFFSET (0x08000000)

#define PIN_STM32_BOOT GPIO_NUM_12
#define PIN_STM32_RESET GPIO_NUM_14

enum{
	PROTO_ACK=0x79,
	PROTO_NACK=0x1F,
	PROTO_ABORT=0x5F,
	PROTO_HANDSHAKE=0x7F,
	PROTO_GETVERSION=0x00
};

enum{
	STATE_UNKNOWN,
	STATE_BOOTLOADER,
	STATE_FIRMWARE
};

static int is_handshaked=0;
static int is_port_configured=0;

uint8_t fbuf[1024];

void flasher_init(){
	//int len=f_send_cmd(PROTO_GETVERSION,fbuf);
	if(is_port_configured){
	return;}
	
	uart_config_t uart_config={
		.baud_rate=115200,
		.data_bits=UART_DATA_8_BITS,
		.parity=UART_PARITY_EVEN,
		.stop_bits=UART_STOP_BITS_1,
		.flow_ctrl=UART_HW_FLOWCTRL_DISABLE,
		.source_clk=UART_SCLK_DEFAULT,
	};
	ESP_ERROR_CHECK(uart_driver_install(2,1024,0,0,NULL,0));
	ESP_ERROR_CHECK(uart_param_config(2,&uart_config));
	ESP_ERROR_CHECK(uart_set_pin(2,10,9,0,0));
	
	/*
	$port->read_char_time(0);
	$port->read_const_time($timeout * 1000); 
	*/
	
	gpio_config_t io_conf={};
	io_conf.intr_type=GPIO_INTR_DISABLE;
	io_conf.mode=GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask=(1<<PIN_STM32_BOOT)|(1<<PIN_STM32_RESET);
	//disable pull-down mode
	io_conf.pull_down_en=GPIO_PULLDOWN_DISABLE;
	//disable pull-up mode
	io_conf.pull_up_en=GPIO_PULLUP_ONLY;
	//configure GPIO with the given settings
	gpio_config(&io_conf);
	
	gpio_pullup_en(PIN_STM32_BOOT);
	gpio_pullup_en(PIN_STM32_RESET);
	
	is_port_configured=1;
	is_handshaked=0;
	
}

void f_send_handshake(){
	if(is_handshaked){
	return;}
	
	flasher_init();
	
	f_reset(STATE_BOOTLOADER);
	fbuf[0]=PROTO_HANDSHAKE;
	uart_write_bytes(2,(const char *)fbuf,1);
	is_handshaked=1;
	f_check_ack();
}

void f_erase_chip(){
	f_send_cmd(0x43); // erase chip
	f_send_cmd(0xff); // global erase
}

void flasher_write(uint8_t *buf, int len, uint16_t block_id){
	uint32_t offset=(uint32_t)DEFAULT_FLASH_OFFSET+((block_id-1)*512);
	int packet_len=0,is_last=(len!=512)?1:0;
	printf("We got WRT request, %d bytes, block id: %d, writing at 0x%08lx\n",len,block_id,offset);
	if(block_id==1 && len>0){
		f_send_handshake();
		f_erase_chip();
	}
	
	while(len>0){
		packet_len=len;
		if(packet_len>128){
		packet_len=128;}
		printf("Writing %d bytes to %08lx (%ld) flash\n",packet_len,offset,offset);
		f_send_cmd(0x31); // write memory
		f_send_addr(offset);
		f_send_data(buf,packet_len);
		len-=packet_len;
		offset+=packet_len;
		buf+=packet_len;
	}
	
	if(is_last){
		f_reset(STATE_FIRMWARE);
	}
	
}

void f_reset(int state){
	gpio_set_level(PIN_STM32_RESET,0);
	gpio_set_level(PIN_STM32_BOOT,0);
	is_handshaked=0;
	vTaskDelay(100/portTICK_PERIOD_MS);
	
	if(state==STATE_BOOTLOADER){
		gpio_set_level(PIN_STM32_BOOT,1);
	}
	
	if(state==STATE_FIRMWARE){
		gpio_set_level(PIN_STM32_BOOT,0);
	}
	
	vTaskDelay(100/portTICK_PERIOD_MS);
	gpio_set_level(PIN_STM32_RESET,1);
	vTaskDelay(100/portTICK_PERIOD_MS);
}

void f_send_cmd(int cmd){
	fbuf[0]=cmd;
	fbuf[1]=cmd^0xFF;
	printf("Sending CMD %02x\n",cmd);
	uart_write_bytes(2,(const char *)fbuf,2);
	f_check_ack();
}

void f_send_addr(uint32_t addr){
	printf("Sending ADDR %08lx\n",addr);
	uint8_t *r=(uint8_t *)&addr;
	fbuf[0]=r[3];
	fbuf[1]=r[2];
	fbuf[2]=r[1];
	fbuf[3]=r[0];
	f_add_xor(fbuf,4);
	uart_write_bytes(2,(const char *)fbuf,5);
	f_check_ack();
}

void f_send_data(uint8_t *buf, int len){
	printf("Sending DATA %08x bytes\n",len);
	fbuf[0]=len-1;
	memcpy(fbuf+1,buf,len);
	f_add_xor(fbuf,len+1);
	uart_write_bytes(2,(const char *)fbuf,len+2);
	f_check_ack();
}

void f_add_xor(uint8_t *buf, int len){
	int sum=0,q;
	for(q=0;q<len;q++){
		sum^=buf[q];
	}
	buf[len]=sum;
}

int f_check_ack(){
	vTaskDelay(100/portTICK_PERIOD_MS);
	printf("FLASHER: waiting ACK from STM32...\n");
	int len=uart_read_bytes(2,fbuf,1,200);
	printf("FLASHER: ...got %d bytes\n",len);
	
	if(len<1){
		//timeout
		printf("...FLASHER can't get data from STM32!!!\n");
		return(-1);
	}
	
	if(fbuf[0]==PROTO_ACK){
		printf("...FLASHER GOT ACK!!!\n");
		return(1);
	}
	if(fbuf[0]==PROTO_NACK){
		//bad
		printf("...FLASHER GOT NACK!!!\n");
		return(-1);
	}
	if(fbuf[0]==PROTO_ABORT){
		//stop it
		printf("...FLASHER GOT ABORT!!!\n");
		return(-2);
	}
	
	printf("...FLASHER Unknown answer!!! code: %d (0x%02x)\n",fbuf[0],fbuf[0]);
	return(-3);
	
}
