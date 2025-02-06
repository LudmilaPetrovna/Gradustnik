#include "common.h"
#include "keys.inc"

enum{
	INDC_EXTERNAL_TEMP=0,
	INDC_INTERNAL_TEMP,
	INDC_INTERNAL_HUMD,
	INDC_VELO
};

typedef struct{
	char *name;
	char *unit;
	int update_every;
	int is_persistent;
	float current_value;
	float prev_value;
	int error;
	int has_prev_value;
	uint64_t last_tick;
}INDICATOR;

MMMU_CLIENT_STATE mmmu;

INDICATOR indicators[4]={
	{"На улице","°C",180,1},
	{"Температура в комнате","°C",180,1},
	{"Влажность в комнате","%",180,1},
	{"Оборотов велотренажера"," оборотов",1,0}
};

char *smileys[]={":)",";)",":D","8-)","-=8[]"};

int mumble_app(){
	uint64_t now;
	int q;
	char message[512];
	char *mout;
	char *color;
	char *symbol;
	float diff;
	int try_count;
	uint64_t current_tick;
	
	mmmu_init(&mmmu);
	mmmu_set_cert(&mmmu,CLIENT_CERT,CLIENT_KEY);
	mmmu_set_server(&mmmu,"mumble.shiziki.com","64738","Gradustnik911/33");
	mmmu_set_recording(&mmmu,0);
	
	ds18b20_init(23);
	velo_counter_init();
	
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	
	while(1){
		now=(uint64_t)time(0);
		
		trace("Mumble app: connected:%d, velo: %d/%d, %s", mmmu.is_connected, velo_counter,velo_counter_delay, ctime((time_t*)&now));
		
		now/=60;
		
		if(!mmmu.is_connected){
			mmmu_connect(&mmmu);
		}
		
		mout=message;
		for(q=0;q<4;q++){
			current_tick=now/indicators[q].update_every;
			if(indicators[q].last_tick!=current_tick){
				trace("updating indicator %d",q);
				
				if(q==INDC_EXTERNAL_TEMP){
					trace("getting external temp");
					mmmu_send_ping();
					ds18b20_trigger_temperature_conversion(NULL);
					vTaskDelay(1000 / portTICK_PERIOD_MS);
					indicators[q].error=ds18b20_get_temperature(NULL, &indicators[q].current_value);
				}
				
				if(q==INDC_INTERNAL_TEMP){
					trace("getting internal temp");
					mmmu_send_ping();
					try_count=10;
					do{
						vTaskDelay(1234 / portTICK_PERIOD_MS);
						indicators[INDC_INTERNAL_TEMP].error=
						indicators[INDC_INTERNAL_HUMD].error=DHT_read(22, &indicators[INDC_INTERNAL_TEMP].current_value, &indicators[INDC_INTERNAL_HUMD].current_value);
					} while(indicators[q].error && try_count-->0);
				}
				
				if(q==INDC_VELO){
					trace("getting velo");
					indicators[q].current_value=velo_counter;
				}
				
				color="blue";
				symbol="■";
				
				diff=indicators[q].current_value-indicators[q].prev_value;
				if(fabs(diff)<0.01){diff=0;}
				
				trace("new value indicator: %.2f (%.2f), error %d",indicators[q].current_value,diff,indicators[q].error);
				
				if(diff>0){
					color="green";
					symbol="▲";
				}
				
				if(diff<0){
					color="red";
					symbol="▼";
				}
				
				if(diff!=0 || indicators[q].is_persistent){
					mout+=sprintf(mout,"<br>");
					trace("%s: %f",indicators[q].name,indicators[q].current_value);
					mout+=sprintf(mout,"%s: ",indicators[q].name);
					if(indicators[q].error){
						mout+=sprintf(mout,"ошибка %d (%s)",indicators[q].error,DHT_error(indicators[q].error));
					} else {
						mout+=sprintf(mout,"<b>%.2f%s</b>",indicators[q].current_value,indicators[q].unit);
						if(diff!=0 && indicators[q].has_prev_value){
							mout+=sprintf(mout," <font color=%s><b>%s</b> (%s%.2f)</font>",color,symbol,diff>0?"+":"",diff);
						}
					}
				}
				
				indicators[q].last_tick=current_tick;
				if(!indicators[q].error){
					indicators[q].prev_value=indicators[q].current_value;
					indicators[q].has_prev_value=1;
				} else {
					indicators[q].prev_value=0;
					indicators[q].has_prev_value=0;
				}
				
			}
		}
		
		if(mout!=message){
			trace("posting message: %s",message);
			//mout+=sprintf(mout,"<br>len %d",strlen(message));
			mmmu_send_text(message,mmmu.root_channel);
		}
		
		mmmu_step(&mmmu);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	
	return 0;
}

#ifndef ESP_PLATFORM
int main(){
	mumble_app();
	return 0;
}
#endif
