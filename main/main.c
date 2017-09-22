
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include <sys/socket.h>


#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/i2c.h"
#include "app_flash.h"

#define APP_LOG_EN		1

#if APP_LOG_EN
#define APP_LOGI(x,...)	ESP_LOGI("",x,##__VA_ARGS__)
#define APP_LOGD(x,...)	ESP_LOGD("",x,##__VA_ARGS__)
#define APP_LOGW(x,...)	ESP_LOGW("",x,##__VA_ARGS__)
#define APP_LOGE(x,...)	ESP_LOGE("",x,##__VA_ARGS__)
#else
#define APP_LOGI(x,...)
#define APP_LOGD(x,...)
#define APP_LOGW(x,...)
#define APP_LOGE(x,...)
#endif

#define WIFI_CONNECTED_BIT		BIT0

//Task handle for http task.
TaskHandle_t http_server_task_handle = NULL;

char s_device_id[64];
EventGroupHandle_t s_event_group;

/*
I2C Definitions
 */

 #define I2C_MASTER_SCL_IO    19    /*!< gpio number for I2C master clock */
 #define I2C_MASTER_SDA_IO    18    /*!< gpio number for I2C master data  */
 #define I2C_MASTER_NUM   I2C_NUM_1   /*!< I2C port number for master dev */
 #define I2C_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
 #define I2C_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
 #define I2C_MASTER_FREQ_HZ    100000     /*!< I2C master clock frequency */

 #define WRITE_BIT  I2C_MASTER_WRITE /*!< I2C master write */
 #define READ_BIT   I2C_MASTER_READ  /*!< I2C master read */
 #define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
 #define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
 #define ACK_VAL    0x0         /*!< I2C ack value */
 #define NACK_VAL   0x1         /*!< I2C nack value */

/*
I2C Functions
 */

esp_err_t i2c_master_check_slave(i2c_port_t i2c_num,uint8_t addr)
{
   i2c_cmd_handle_t cmd = i2c_cmd_link_create();
   i2c_master_start(cmd);
   i2c_master_write_byte(cmd, ( addr << 1 ) , ACK_CHECK_EN);
   i2c_master_stop(cmd);
   esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
   i2c_cmd_link_delete(cmd);
   return ret;
}


/**
* @brief i2c master initialization
*/
void i2c_init()
{
   int i2c_master_port = I2C_MASTER_NUM;
   i2c_config_t conf;
   conf.mode = I2C_MODE_MASTER;
   conf.sda_io_num = I2C_MASTER_SDA_IO;
   conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
   conf.scl_io_num = I2C_MASTER_SCL_IO;
   conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
   conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
   i2c_param_config(i2c_master_port, &conf);
   i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}



/**
* @brief Scan all i2c adresses
*/
void i2c_scan() {
	int address;
   int ret;
	int foundCount = 0;
	for (address=0x01; address< 0x77; address++) {
       ret=i2c_master_check_slave(I2C_MASTER_NUM,address);
       if (ret == ESP_OK) {
           printf("Found device address: %02x\n", address);
           foundCount++;
       }
   }
   printf("Done scanning.. found %d devices\n",foundCount);
}


 void http_server_task(void* arg)
{

	APP_LOGI("http_server_task start");

	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0)
	{
		APP_LOGE("Can't create server socket");
		vTaskDelete(NULL);
		return;
	}


	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	uint32_t socklen = sizeof(client_addr);


	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(80);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		APP_LOGE("Bind fail");
		close(server_socket);
		vTaskDelete(NULL);
		return;
	}


	if (listen(server_socket, 5) < 0)
	{
		APP_LOGE("Listen fail");
		close(server_socket);
		vTaskDelete(NULL);
		return;
	}

	while (1)
	{
		int connect_socket = accept(server_socket, (struct sockaddr*)&client_addr, &socklen);
		if (connect_socket >= 0)
		{
			char buffer[1024], c;
			int bytes_read = 0;
			memset(buffer, 0, 1024);
			while (recv(connect_socket, &c, 1, 0) > 0)
			{
				buffer[bytes_read++] = c;
				if (bytes_read == 1023)
					break;
				if (strstr(buffer, "\r\n\r\n"))
					break;
			}

			if (strncmp(buffer, "GET ", 4))
			{
				close(connect_socket);

				vTaskDelay(200 / portTICK_RATE_MS);
				continue;
			}



      ////////////////////////////////////////////////
      ////////////////////////////////////////////////

      char* ptr = strstr(buffer, "/msg?ssid=");
			if (ptr)
			{
				char* ptr2 = strstr(buffer, "&password=");
				char* ptr3 = strstr(buffer, " HTTP/1.");

				char ssid1[64], password1[64],ssid[64], password[64];
				memset(ssid, 0, 64);
				memset(password, 0, 64);

				strncpy(ssid1, ptr + 10, (ptr2 - ptr) - 10);
				strncpy(password1, ptr2 + 10, (ptr3 - ptr2) - 10);


				int specialCount = 0;


				for(int i=0; i <= strlen(ssid1);i++){
					//check for special Characters, grab next 2 Characters.

					if(ssid1[i] == '%'){
						specialCount++;
					}
				}

				int j=0;
				int ssidlength = strlen(ssid1) - ((3 * specialCount) - specialCount);
				for(int i=0; i <= strlen(ssid1);i++){
					//check for special Characters, grab next 2 Characters.
					if(ssid1[i] == '%'){
						i++;
            char high = ssid1[i];
            i++;
            char low = ssid1[i];

						// Convert ASCII 0-9A-F to a value 0-15
            if (high > 0x39) high -= 7;
            high &= 0x0f;

            // Same again for the low byte:
            if (low > 0x39) low -= 7;
            low &= 0x0f;

						if(j <= ssidlength){
							ssid[j] = (high << 4) | low;
						}
					}
					else{
						if(j <= ssidlength){
							ssid[j] = ssid1[i];
						}
					}
					j++;
				}

				j = 0;
				specialCount = 0;

				for(int i=0; i <= strlen(password1);i++){
					//check for special Characters
					if(password1[i] == '%'){
						specialCount++;
					}
				}

				int pwlength = strlen(password1) - ((3 * specialCount) - specialCount);

				for(int i=0; i <= strlen(password1);i++){
					//check for special Characters, grab next 2 Characters.
					if(password1[i] == '%'){
						i++;
            char high = password1[i];
            i++;
            char low = password1[i];

						// Convert ASCII 0-9A-F to a value 0-15
            if (high > 0x39) high -= 7;
            high &= 0x0f;

            // Same again for the low byte:
            if (low > 0x39) low -= 7;
            low &= 0x0f;

						if(j <= ssidlength){
							password[j] = (high << 4) | low;
						}
					}
					else{
						if(j <= pwlength){
							password[j] = password1[i];
						}
					}
					j++;
				}

				for(int i=0; i<=strlen(ssid); i++){
					if(ssid[i] == '+')
						ssid[i] = ' ';
				}

				for(int i=0; i<=strlen(password); i++){
					if(password[i] == '+')
						password[i] = ' ';
				}

				char* response =
					"HTTP/1.1 200 OK\r\n"
					"Content-Type: text/html\r\n\r\n"
					"<!DOCTYPE HTML>\r\n<html>\r\n"
					"<h1><center>Completed WiFi configuration</center></h1>"
					"<h1><center>Device is restarting...</center></h1>"
					"</html>\n";
				send(connect_socket, response, strlen(response), 0);
				close(connect_socket);

				if (strlen(ssid) > 0)
					app_flash_save_wifi_info(ssid, password);

				vTaskDelay(1000 / portTICK_RATE_MS);

				close(server_socket);
				//esp_wifi_deinit();
				esp_restart();
			}
			else
			{
				char* response =
					"HTTP/1.1 200 OK\r\n"
					"Content-Type: text/html\r\n\r\n"
					"<!DOCTYPE HTML>\r\n<html>\r\n"
					"<body>"
					"<p>"
					"<center>"
					"<h1>WiFi Configuration</h1>"
					"<div>"
					"</div>"
					"<form action='msg'><p>SSID:  <input type='text' name='ssid' size=50 autofocus></p>"
					"<p>Password: <input type='text' name='password' size=50 autofocus></p>"
					"<p><input type='submit' value='Submit'></p>"
					"</form>"
					"</center>"
					"</body></html>\n";
				send(connect_socket, response, strlen(response), 0);
				close(connect_socket);
			}
		}
		vTaskDelay(200 / portTICK_RATE_MS);
	}
	close(server_socket);
  APP_LOGI("HTTP TASK COMPLETED.")
	vTaskDelete(NULL);
}


 static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
 {
 	static bool ap_started = false;
     switch(event->event_id) {
     case SYSTEM_EVENT_AP_START:
     	APP_LOGI("SYSTEM_EVENT_AP_START");
     	if (!ap_started)
     	{
     		xTaskCreate(&http_server_task, "http_server_task", 4096, NULL, 5, NULL);
     		ap_started = true;
     	}
     	break;
     case SYSTEM_EVENT_STA_START:
         APP_LOGI("SYSTEM_EVENT_STA_START");
         esp_wifi_connect();
         break;
     case SYSTEM_EVENT_STA_GOT_IP: {
     	uint8_t mac[6];
 		esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
 		sprintf(s_device_id, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
         xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
         break;
     }
     case SYSTEM_EVENT_STA_DISCONNECTED:
         esp_wifi_connect();
 				xEventGroupClearBits(s_event_group, WIFI_CONNECTED_BIT);
         break;
     default:
         break;
     }
     return ESP_OK;
 }


 void app_wifi_init()
{
	tcpip_adapter_init();
	s_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL) );
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

	//give unique names to SSID of SCALEAP
	char wifi_ap_name[32];
	uint8_t sta_mac[6];
	esp_wifi_get_mac(ESP_IF_WIFI_STA, sta_mac);
	sprintf(wifi_ap_name, "Talon%02x%02x%02x",sta_mac[3], sta_mac[4], sta_mac[5]);
	APP_LOGI("%s", wifi_ap_name);

	wifi_config_t wifi_ap_config = {
		.ap = {
			.ssid = "",
			.ssid_len = 0,
			.max_connection = 1,
			.authmode = WIFI_AUTH_OPEN
		}
	};

	//copied ssid for AP here,due to type cast error in wifi_ap_config
	strcpy((char*)wifi_ap_config.ap.ssid, wifi_ap_name);

	char ssid[128] = { 0 }, password[128] = { 0 };
	if (app_flash_load_wifi_info(ssid, password))
	{
		APP_LOGI("Read wifi info from flash, Setting to STA, ssid=%s, pwd=%s", ssid, password);
		wifi_config_t wifi_sta_config;
		strcpy((char*)wifi_sta_config.sta.ssid, ssid);
		strcpy((char*)wifi_sta_config.sta.password, password);
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
		ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_sta_config));
		ESP_ERROR_CHECK(esp_wifi_start());

		xEventGroupWaitBits(s_event_group, WIFI_CONNECTED_BIT, false, true, 10000 / portTICK_RATE_MS);

    if (xEventGroupGetBits(s_event_group) & WIFI_CONNECTED_BIT) // WiFi Connection Success
		{
			APP_LOGI("WiFi Connection Success, Setting to STA");
			//ESP_ERROR_CHECK(esp_wifi_stop());
			ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
			//ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_ap_config));
			ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_sta_config));
			//ESP_ERROR_CHECK(esp_wifi_start());
		}
		else // WiFi Connection Fail
		{
			APP_LOGI("WiFi Connection Fail, Setting to SoftAP");
			//ESP_ERROR_CHECK(esp_wifi_stop());
			esp_wifi_disconnect();
			ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
			ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_ap_config));
			//ESP_ERROR_CHECK(esp_wifi_start());
		}
	}
	else
	{
		APP_LOGI("Not read wifi info from flash, Setting to Soft AP");
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
		ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_ap_config));
		ESP_ERROR_CHECK(esp_wifi_start());
	}
}




void app_main(){

  gpio_config_t io_conf;
	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	io_conf.pin_bit_mask = 1ULL << 22;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 0;
  gpio_config(&io_conf);

  gpio_config_t io_conf1;
	io_conf1.intr_type = GPIO_PIN_INTR_DISABLE;
	io_conf1.pin_bit_mask = 1ULL << 23;
	io_conf1.mode = GPIO_MODE_OUTPUT;
	io_conf1.pull_down_en = 0;
	io_conf1.pull_up_en = 0;
	gpio_config(&io_conf1);

  gpio_set_level(GPIO_NUM_22, 0);
  gpio_set_level(GPIO_NUM_23, 1);
  vTaskDelay(1000/portTICK_RATE_MS);


  ESP_LOGI("APP", "STARTING.....");
  ESP_LOGI("wifi", "STARTING WIFI");
  app_wifi_init();

  xEventGroupWaitBits(s_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

  while (xEventGroupGetBits(s_event_group) & WIFI_CONNECTED_BIT){
    ESP_LOGI("I2C", "Initialising I2C Bus.");

    i2c_init();

    ESP_LOGI("I2C", "Scanning I2C Devices.");
    i2c_scan();
    vTaskDelay(1000/portTICK_RATE_MS);
  }

}
