/* PPPoS Client Example with GSM
 *  (tested with SIM800)
 *  Author: LoBo (loboris@gmail.com, loboris.github)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */


#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "driver/uart.h"

#include "netif/ppp/pppos.h"
#include "netif/ppp/ppp.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "netif/ppp/pppapi.h"

#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

#include <esp_event.h>
#include <esp_wifi.h>

#include "lwip/apps/sntp.h"
#include "cJSON.h"

#include "libGSM.h"

#define EXAMPLE_TASK_PAUSE	300		// pause between task runs in seconds
#define TASK_SEMAPHORE_WAIT 140000	// time to wait for mutex in miliseconds

QueueHandle_t http_mutex;

static const char *SMS_TAG = "[SMS]";

//======================================
static void sms_task(void *pvParameters)
{
	if (!(xSemaphoreTake(http_mutex, TASK_SEMAPHORE_WAIT))) {
		ESP_LOGE(SMS_TAG, "*** ERROR: CANNOT GET MUTEX ***n");
		while (1) {
            vTaskDelay(10000 / portTICK_PERIOD_MS);
		}
	}

	SMS_Messages messages;
	uint32_t sms_time = 0;
	char buf[160];

	goto start;

	while(1) {
        if (!(xSemaphoreTake(http_mutex, TASK_SEMAPHORE_WAIT))) {
			ESP_LOGE(SMS_TAG, "===== ERROR: CANNOT GET MUTEX ==================================\n");
            vTaskDelay(30000 / portTICK_PERIOD_MS);
			continue;
		}
start:
		ESP_LOGI(SMS_TAG, "===== SMS TEST =================================================\n");

		// ** For SMS operations we have to off line **
		ppposDisconnect(0, 0);
		gsm_RFOn();  // Turn on RF if it was turned off
		vTaskDelay(2000 / portTICK_RATE_MS);

		if (clock() > sms_time) {
			if (smsSend(CONFIG_GSM_SMS_NUMBER, "Hi from ESP32 via GSM\rThis is the test message.") == 1) {
				printf("SMS sent successfully\r\n");
			}
			else {
				printf("SMS send failed\r\n");
			}
			sms_time = clock() + CONFIG_GSM_SMS_INTERVAL; // next sms send time
		}

		smsRead(&messages, -1);
		if (messages.nmsg) {
			printf("\r\nReceived messages: %d\r\n", messages.nmsg);
			SMS_Msg *msg;
			for (int i=0; i<messages.nmsg; i++) {
				msg = messages.messages + (i * sizeof(SMS_Msg));
				struct tm * timeinfo;
				timeinfo = localtime (&msg->time_value );
				printf("-------------------------------------------\r\n");
				printf("Message #%d: idx=%d, from: %s, status: %s, time: %s, tz=GMT+%d, timestamp: %s\r\n",
						i+1, msg->idx, msg->from, msg->stat, msg->time, msg->tz, asctime(timeinfo));
				printf("Text: [\r\n%s\r\n]\r\n\r\n", msg->msg);

				// Check if SMS text contains known command
				if (strstr(msg->msg, "Esp32 info") == msg->msg) {
					char buffer[80];
					time_t rawtime;
					time(&rawtime);
					timeinfo = localtime( &rawtime );
					strftime(buffer,80,"%x %H:%M:%S", timeinfo);
					sprintf(buf, "Hi, %s\rMy time is now\r%s", msg->from, buffer);
					if (smsSend(CONFIG_GSM_SMS_NUMBER, buf) == 1) {
						printf("Response sent successfully\r\n");
					}
					else {
						printf("Response send failed\r\n");
					}
				}
				// Free allocated message text buffer
				if (msg->msg) free(msg->msg);
				if ((i+1) == messages.nmsg) {
					printf("Delete message at index %d\r\n", msg->idx);
					if (smsDelete(msg->idx) == 0) printf("Delete ERROR\r\n");
					else printf("Delete OK\r\n");
				}
			}
			free(messages.messages);
		}
		else printf("\r\nNo messages\r\n");

		// ** We can turn off GSM RF to save power
		gsm_RFOff();
		// ** We can now go back on line, or stay off line **
        //ppposInit();

        ESP_LOGI(SMS_TAG, "Waiting %d sec...", EXAMPLE_TASK_PAUSE);
        ESP_LOGI(SMS_TAG, "================================================================\n\n");

        xSemaphoreGive(http_mutex);
        for(int countdown = EXAMPLE_TASK_PAUSE; countdown >= 0; countdown--) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}


//=============
void app_main()
{
    http_mutex = xSemaphoreCreateMutex();


    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1ULL<<GPIO_NUM_25 | 1ULL<<GPIO_NUM_4;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_25, 0);
    gpio_set_level(GPIO_NUM_4, 1);

	ppposInit();
	#ifdef CONFIG_GSM_SEND_SMS
	// ==== Create SMS task ====
    xTaskCreate(&sms_task, "sms_task", 4096, NULL, 3, NULL);
    #endif
    
    while(1)
	{
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
}
