#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef _WIN32
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "onewire_bus.h"
#else

#define esp_err_t int
#define portTICK_PERIOD_MS 1
#define vTaskDelay(x) sleep(1)

#endif

// crypto
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/platform.h"
#include "mbedtls/ssl.h"

//#ifndef _DEBUG
//efine _DEBUG
//#endif
#include "fancylog2.h"

// modules
#include "DHT.h"
#include "ds18b20.h"
#include "mmmu.h"
#include "Mumble.pb-c.h"
#include "velo_counter.h"
