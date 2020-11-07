#include "defines.h"
#include "log.h"


log_tag_t* log_list() { //инициализация массиава для настроек логирования с минимальным базовым значением уровня
	static log_tag_t logTags[TAG_MAX] = {
		{ .tag="*",		 .level = ESP_LOG_ERROR},
		{ .tag="wifi", 	 .level = ESP_LOG_ERROR},
		{ .tag="httpd",	 .level = ESP_LOG_ERROR},
		{ .tag="nmea",	 .level = ESP_LOG_ERROR},
		{ .tag="odo",	 .level = ESP_LOG_ERROR},
		{ .tag="oiler",	 .level = ESP_LOG_ERROR},
		{ .tag="config", .level = ESP_LOG_ERROR},
		{ .tag="gps",	 .level = ESP_LOG_ERROR},
		{ .tag="hds",    .level = ESP_LOG_ERROR},
	};

	return logTags; //Возвращаем указатель на массив настроек
}

void log_set_level(log_tag_num_t tag, esp_log_level_t level) { //замена уровня конкретной настройки логирования
	if(tag > 0 && tag < TAG_MAX && level >=0 && level <= ESP_LOG_VERBOSE) {//проверяем на корректность параметры функции
		log_tag_t* list = log_list(); //получаем указатель на массив настроек

		(list[tag]).level = level; //заменяем значение уровня конкретного типа лога в памяти
		esp_log_level_set( (list[tag]).tag, (list[tag]).level); //отправляем изменённую настройку в систему
	}
}
char* log_name(log_tag_num_t tag) { //расшифровка номера тега из enum (log.h) в название, например: 3 = "nmea"
	if(tag > 0 && tag < TAG_MAX) return (log_list()[tag]).tag ;
	return (log_list()[TAG_ALL]).tag;
}

esp_log_level_t log_level(log_tag_num_t tag) {//считывание настройки уровня по номеру тега
	if(tag > 0 && tag < TAG_MAX) return (log_list()[tag]).level ;
	return (log_list()[TAG_ALL]).level;
}

void log_init() {
	
	// Suggetion: https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/system/log.html#_CPPv419esp_log_set_vprintf14vprintf_like_t

	log_tag_t* list = log_list(); //инициализируем настройки базовыми значениями

	for(uint8_t i=0; i<TAG_MAX; i++) 
		esp_log_level_set( (list[i]).tag, (list[i]).level); //передаём базовые значения в систему
}