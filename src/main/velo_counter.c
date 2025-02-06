#include "common.h"

volatile int velo_counter=0;
volatile int velo_counter_delay=0;

#ifdef ESP_PLATFORM

//static void IRAM_ATTR counter_isr(void* arg){
static void counter_isr(void* arg){
	uint32_t gpio_num=(uint32_t) arg;
	if(gpio_num==COUNTER_PIN && velo_counter_delay==0){
		velo_counter++;
		velo_counter_delay=3;
		//printf("Tick!!!\n");
		//fflush(stdout);
	}
}

static void counter_delay_task(void *arg){
	while(1){
		if(velo_counter_delay>0){
			velo_counter_delay--;
		}
		vTaskDelay(80/portTICK_PERIOD_MS);
	}
}

/*
static void counter_task(void *arg){
	int cnt=0;
	while(1){
		printf("cnt: %d, value: %d\n", cnt++, counter);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}
*/
void velo_counter_init(){
	gpio_config_t io_conf={};
	io_conf.intr_type=GPIO_INTR_POSEDGE;
	io_conf.pin_bit_mask=1<<COUNTER_PIN;
	io_conf.mode=GPIO_MODE_INPUT;
	io_conf.pull_up_en=1;
	gpio_config(&io_conf);
	
	//gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
	gpio_install_isr_service(0);
	gpio_isr_handler_add(COUNTER_PIN,counter_isr, (void*) COUNTER_PIN);
	xTaskCreate(counter_delay_task, "counter_delay_task", 2048, NULL, 10, NULL);
	//xTaskCreate(counter_task, "counter_task", 2048, NULL, 10, NULL);
}

#else

static void *worker(void *ptr){
	while(1){
		velo_counter=rand();
		sleep(1);
	}
	return NULL;
}

void velo_counter_init(){
	pthread_t wt;
	pthread_create(&wt,NULL,&worker,0);
}

#endif
