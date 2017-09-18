#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

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
#include "BME280.h"


/*
I2C Definitions
 */

 #define I2C_MASTER_SCL_IO    19    /*!< gpio number for I2C master clock */
 #define I2C_MASTER_SDA_IO    18    /*!< gpio number for I2C master data  */
 #define I2C_MASTER_NUM   I2C_NUM_0   /*!< I2C port number for master dev */
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
   conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
   conf.scl_io_num = I2C_MASTER_SCL_IO;
   conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
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
	for (address=1; address<127; address++) {
       ret=i2c_master_check_slave(I2C_MASTER_NUM,address);
       if (ret == ESP_OK) {
           printf("Found device addres: %02x\n", address);
           foundCount++;
       }
   }
   printf("Done scanning.. found %d devices\n",foundCount);
}


/*
BME data Struct
 */
typedef struct tempHumiditParameters {
     int temp;
     int humidity;
 } tempHumidityParameters;

void app_main(){

  ESP_LOGI("APP", "STARTING.....");
  ESP_LOGI("I2C", "Initialising I2C Bus.");

  i2c_init();

  ESP_LOGI("I2C", "Scanning I2C Devices.");
  i2c_scan();

  tempHumidityParameters bme280;

  ESP_LOGI("BME", "******   BME DATA   ******");

  vTaskDelay(3000 / portTICK_RATE_MS);

  while(1){
    i2c_bme280_begin();
    vTaskDelay(1500 / portTICK_RATE_MS);
    bme280.temp= (int)i2c_bme280_read_temp();
    float pressure= i2c_bme280_read_pressure();
    bme280.humidity= (int)i2c_bme280_read_rh();
    ESP_LOGI("", "Temp : %d \t Humidity : %d \t Pressure : %.1f", bme280.temp, bme280.humidity, pressure);
    i2c_bme280_end();
    vTaskDelay(1500 / portTICK_RATE_MS);
  }

}

#if 0
esp_err_t event_handler(void *ctx, system_event_t *event)
{
    return ESP_OK;
}

void app_main(void)
{
    nvs_flash_init();
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    wifi_config_t sta_config = {
        .sta = {
            .ssid = "access_point_name",
            .password = "password",
            .bssid_set = false
        }
    };
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect() );

    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
    int level = 0;
    while (true) {
        gpio_set_level(GPIO_NUM_4, level);
        level = !level;
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}
#endif
