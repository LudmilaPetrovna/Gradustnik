/* 
	DHT22 temperature sensor driver
*/

#ifndef DHT_H_
#define DHT_H_

#define DHT_OK 0
#define DHT_CHECKSUM_ERROR -1
#define DHT_TIMEOUT_ERROR -2

int   DHT_read(int gpio, float *temp, float *hum);
char *DHT_error(int response);

#endif
