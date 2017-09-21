/*
 * app_flash.c
 *
 * Interface for Data Table
 *
 * Created by:
 *       K.C.Y
 * Date:
 *       2017/06
 */

#include "esp_system.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "nvs.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "app_flash.h"

#define SECTION_NAME	"storage"

esp_err_t app_flash_init()
{
	return nvs_flash_init();
}



bool app_flash_load_wifi_info(char* ssid, char* password)
{
	nvs_handle my_handle;
	esp_err_t err;

	err = nvs_open(SECTION_NAME, NVS_READWRITE, &my_handle);
	if (err != ESP_OK)
	{
		printf("app_flash_load_wifi_info: Can't open nvs\n");
		return false;
	}

	char key[32];
	char buffer[256];
	size_t required_size = 0;

	strcpy(key, "wifi");
	memset(buffer, 0, 256);

	err = nvs_get_blob(my_handle, key, NULL, &required_size);
	if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
	{
		printf("app_flash_load_wifi_info: Can't get wifi info size\n");
		return false;
	}

	if (required_size > 0)
	{
		err = nvs_get_blob(my_handle, key, buffer, &required_size);
		if (err != ESP_OK)
		{
			printf("app_flash_load_wifi_info: Can't get wifi info data, size=%d\n", required_size);
			return false;
		}
		strcpy(ssid, &buffer[0]);
		strcpy(password, &buffer[128]);
	}
	else
		return false;

	nvs_close(my_handle);
	return true;
}

bool app_flash_save_wifi_info(char* ssid, char* password)
{
	nvs_handle my_handle;
	esp_err_t err;

	err = nvs_open(SECTION_NAME, NVS_READWRITE, &my_handle);
	if (err != ESP_OK)
	{
		printf("app_flash_save_wifi_info: Can't open nvs\n");
		return false;
	}

	char key[32];
	char buffer[256];
	size_t required_size = 256;

	strcpy(key, "wifi");
	memset(buffer, 0, 256);
	strcpy(&buffer[0], ssid);
	strcpy(&buffer[128], password);

	err = nvs_set_blob(my_handle, key, buffer, required_size);

	if (err != ESP_OK)
	{
		printf("app_flash_save_wifi_info: Can't save wifi info data\n");
		return false;
	}

	err = nvs_commit(my_handle);
	if (err != ESP_OK)
		return false;

	nvs_close(my_handle);
	return true;
}
