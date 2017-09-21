
#ifndef _DATA_TABLE_H_
#define _DATA_TABLE_H_

esp_err_t app_flash_init();

bool app_flash_load_wifi_info(char* ssid, char* password);
bool app_flash_save_wifi_info(char* ssid, char* password);

#endif
