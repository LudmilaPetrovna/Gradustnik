/*
* SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Unlicense OR CC0-1.0
*/
#include "common.h"
#include "ds18b20.h"

#define TAG "ds18b20"

#ifdef ESP_PLATFORM

onewire_bus_handle_t handle;

void ds18b20_init(int gpio_pin){
	onewire_rmt_config_t config={
		.gpio_pin=gpio_pin,
		.max_rx_bytes=10, // 10 tx bytes(1byte ROM command + 8byte ROM number + 1byte device command)
	};
	
	//install new 1-wire bus
	ESP_ERROR_CHECK(onewire_new_bus_rmt(&config, &handle));
	ds18b20_set_resolution(NULL,DS18B20_RESOLUTION_12B);
	
}

esp_err_t ds18b20_trigger_temperature_conversion(const uint8_t *rom_number){
	ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid 1-wire handle");
	
	ESP_RETURN_ON_ERROR(onewire_bus_reset(handle), TAG, "error while resetting bus"); // reset bus and check if the device is present
	
	uint8_t tx_buffer[10];
	uint8_t tx_buffer_size;
	
	if (rom_number){
		//specify rom id
		tx_buffer[0] = ONEWIRE_CMD_MATCH_ROM;
		tx_buffer[9] = DS18B20_CMD_CONVERT_TEMP;
		memcpy(&tx_buffer[1], rom_number, 8);
		tx_buffer_size=10;
	} else { // skip rom id
		tx_buffer[0] = ONEWIRE_CMD_SKIP_ROM;
		tx_buffer[1] = DS18B20_CMD_CONVERT_TEMP;
		tx_buffer_size=2;
	}
	
	ESP_RETURN_ON_ERROR(onewire_bus_write_bytes(handle, tx_buffer, tx_buffer_size),
	TAG, "error while triggering temperature convert");
	
	return ESP_OK;
}

esp_err_t ds18b20_get_temperature(const uint8_t *rom_number, float *temperature){
	ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid 1-wire handle");
	ESP_RETURN_ON_FALSE(temperature, ESP_ERR_INVALID_ARG, TAG, "invalid temperature pointer");
	
	*temperature=-10000;
	
	ESP_RETURN_ON_ERROR(onewire_bus_reset(handle), TAG, "error while resetting bus"); // reset bus and check if the device is present
	
	ds18b20_scratchpad_t scratchpad;
	
	uint8_t tx_buffer[10];
	uint8_t tx_buffer_size;
	
	if (rom_number){
		//specify rom id
		tx_buffer[0] = ONEWIRE_CMD_MATCH_ROM;
		tx_buffer[9] = DS18B20_CMD_READ_SCRATCHPAD;
		memcpy(&tx_buffer[1], rom_number, 8);
		tx_buffer_size=10;
	} else {
		tx_buffer[0] = ONEWIRE_CMD_SKIP_ROM;
		tx_buffer[1] = DS18B20_CMD_READ_SCRATCHPAD;
		tx_buffer_size=2;
	}
	
	if(onewire_bus_write_bytes(handle, tx_buffer, tx_buffer_size)){return DS18B20_ERROR_TIMEOUT;}
	if(onewire_bus_read_bytes(handle, (uint8_t *)&scratchpad, sizeof(scratchpad))){return DS18B20_ERROR_TIMEOUT;}
	if(onewire_check_crc8((uint8_t *)&scratchpad, 8)!=scratchpad.crc_value){return DS18B20_ERROR_CHECKSUM;}
	
	//Espressif, you are stupid idiots???
	//static const uint8_t lsb_mask[4] = { 0x07, 0x03, 0x01, 0x00 };
	//uint8_t lsb_masked=scratchpad.temp_lsb & (~lsb_mask[scratchpad.configuration >> 5]); // mask bits not used in low resolution
	//*temperature=(((int16_t)scratchpad.temp_msb << 8) | lsb_masked)  / 16.0f;
	
	int16_t temperature_int=(scratchpad.temp_msb<<8)|scratchpad.temp_lsb;
	*temperature=(double)temperature_int/16.0;
	
	return DS18B20_ERROR_OK;
}

esp_err_t ds18b20_set_resolution(const uint8_t *rom_number, ds18b20_resolution_t resolution){
	ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid 1-wire handle");
	
	ESP_RETURN_ON_ERROR(onewire_bus_reset(handle), TAG, "error while resetting bus"); // reset bus and check if the device is present
	
	uint8_t tx_buffer[10];
	uint8_t tx_buffer_size;
	
	if (rom_number){
		//specify rom id
		tx_buffer[0] = ONEWIRE_CMD_MATCH_ROM;
		tx_buffer[9] = DS18B20_CMD_WRITE_SCRATCHPAD;
		memcpy(&tx_buffer[1], rom_number, 8);
		tx_buffer_size=10;
	} else {
		tx_buffer[0] = ONEWIRE_CMD_SKIP_ROM;
		tx_buffer[1] = DS18B20_CMD_WRITE_SCRATCHPAD;
		tx_buffer_size=2;
	}
	
	ESP_RETURN_ON_ERROR(onewire_bus_write_bytes(handle, tx_buffer, tx_buffer_size),
	TAG, "error while sending read scratchpad command");
	
	tx_buffer[0] = 0;
	tx_buffer[1] = 0;
	tx_buffer[2] = resolution;
	ESP_RETURN_ON_ERROR(onewire_bus_write_bytes(handle, tx_buffer, 3),
	TAG, "error while sending write scratchpad command");
	
	return ESP_OK;
}

#else

void ds18b20_init(int gpio_pin){
	trace("inited with %d gpio",gpio_pin);
}
esp_err_t ds18b20_trigger_temperature_conversion(const uint8_t *rom_number){
	trace("sent temperature req");
	return 0;
}
esp_err_t ds18b20_get_temperature(const uint8_t *rom_number, float *temperature){
	*temperature=(double)rand()/100.0;
	return 0;
}
esp_err_t ds18b20_set_resolution(const uint8_t *rom_number, ds18b20_resolution_t resolution){
	trace("set resolution to %d",resolution);
	return 0;
}
#endif
