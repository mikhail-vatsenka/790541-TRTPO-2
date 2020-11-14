#ifndef __LOG_H
#define _LOG_H

#include <esp_log.h>

// Длина буфера для Live logs
//#define LIVE_LOG_BUFFER_LEN 128

// Тэги для логирования всех компонентов конечной системы 
typedef enum {
	TAG_ALL,
	TAG_WIFI,
	TAG_HTTPD,
	TAG_NMEA,
	TAG_ODO,
	TAG_OILER,
	TAG_CONFIG,
	TAG_GPS,
	TAG_HDS,
	TAG_MAX,
}log_tag_num_t;

// Имена тегов и уровень логирования
typedef struct {
	char* tag;
	esp_log_level_t level; //настройка логирования, чем больше, тем больше ТИПОВ сообщений будет обрабатываться
}log_tag_t;

extern void log_init();
extern char* log_name(log_tag_num_t tag);
extern esp_log_level_t log_level(log_tag_num_t tag);
extern void log_set_level(log_tag_num_t tag, esp_log_level_t level);

#endif