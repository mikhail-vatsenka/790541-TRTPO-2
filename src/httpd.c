#include <esp_http_server.h>

#include <esp_log.h>
#include <esp_wifi.h>

#include "defines.h"
#include "log.h"

#include "httpd.h"

// декодирование строк формата %xx
// https://ru.wikipedia.org/wiki/URL#%D0%9A%D0%BE%D0%B4%D0%B8%D1%80%D0%BE%D0%B2%D0%B0%D0%BD%D0%B8%D0%B5_URL
char* urldecode(char* str) {
	int input_length = strlen(str);

	static size_t output_length = 0;
	static char *buf = NULL;

	if(input_length > output_length+1 || buf == NULL) {
		// неизвестно сколько в строке кодированных байт, поэтому выделяем буфер под всю длину
		char *nb = realloc(buf, input_length+1);
		if (nb != NULL) {
			output_length = input_length + 1;
			buf = nb;
		}
	}

	char *output = buf;

	size_t i=output_length;
	while(*str && i > 0) {
		if(*str == '%') { // после симпола % идет код символа ASCII в шестнадцатиричном виде
			char buffer[3] = { str[1], str[2], 0 };
			*output++ = strtol(buffer, NULL, 16);
			str += 3;
		} else {
			*output++ = *str++;
		}
		*output=0;
		i --;
	}	
	return buf;
}

// URL-кодирование
char* urlencode(char* str) {
	int input_length = strlen(str);
    const char hex[] = "0123456789ABCDEF";

	static size_t output_length;
	static char *buf = NULL;
	
	// выходной буфер как 3x входной, а то мало ли...
	if(input_length > 3*output_length+1 || buf == NULL) {
		char *nb = realloc(buf, 3*input_length + 1);
		if(nb != NULL) {
			output_length = 3*input_length + 1;
			buf = nb;
		}
	}
	char *output = buf;

	size_t i=output_length;
	while(*str && i > 0) {
		// кодируется все, что не соответствует [a-zA-Z0-9]
		if((*str>='0' && *str <='9') || (*str>='A' && *str <='Z') || (*str>='a' && *str <='z')) {
			*output++ = *str++;
			i--;
		} else {
			char sym = *str++;
			*output++ = '%';
			*output++ = hex[(sym >> 4)];
			*output++ = hex[(sym & 0x0F)];
			i-=3;
		}
		*output=0;
	}	

	return buf;
}

// форматированный вывод на страницу. До 511 байт
// если нужно больше - разбивайnt на куски
static void httpd_resp_printf(httpd_req_t *req, const char *format, ...) {
	char buf[512];
	va_list args;
    va_start(args, format);

	vsnprintf(buf, 511, format, args);
	httpd_resp_sendstr_chunk(req, buf);
	va_end(args);
}

// Общие стили
void httpd_css_content(httpd_req_t *req) {

	httpd_resp_sendstr_chunk(req,"<style>\n");
	httpd_resp_sendstr_chunk(req,".bold {font-weight: bold;}\n");
	httpd_resp_sendstr_chunk(req,"</style>\n");
}

// JavaScript секция
void httpd_js_content(httpd_req_t *req) {

	httpd_resp_sendstr_chunk(req,"<script>\n");
	static const char* jsCode = "\n";

	httpd_resp_sendstr_chunk(req, jsCode);
	httpd_resp_sendstr_chunk(req,"</script>\n");
}

// Отправка базовых заголовков страницы
static void httpd_base_header(httpd_req_t *req, uint8_t refresh, char* rfUrl) {
  	httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
	httpd_resp_set_hdr(req, "Pragma","no-cache");
  	httpd_resp_set_hdr(req, "Expires","0");

	httpd_resp_sendstr_chunk(req, "<html lang='en' xml:lang='en' xmlns='http://www.w3.org/1999/xhtml'>");

	httpd_resp_printf(req, "<head>");

	httpd_resp_printf(req, "<title>%s</title>", _sysName);

	httpd_css_content(req);
	httpd_js_content(req);

	// Это нужно, чтобы на мобилках отображалось в нормальном масштабе
	httpd_resp_sendstr_chunk(req, "<meta name='viewport' content='width=device-width, initial-scale=1'>");

	// meta.refresh - обновление страницы каждые N секунд
	if (refresh > 0) {
		httpd_resp_printf(req, "<meta http-equiv='refresh' content='%d ", refresh);
		if(rfUrl != NULL)
			httpd_resp_printf(req, "; url=%s", rfUrl);
		httpd_resp_sendstr_chunk(req, "'>");
	}

	httpd_resp_sendstr_chunk(req, "</head>");

	httpd_resp_sendstr_chunk(req, "<body>");

	// видимый пользователю "заголовок" со ссылкой на начальную страницу
	httpd_resp_printf(req, "<a class='bold' href='/'>&lt; home</a> <span class='bold'>%s</span><hr>", _sysName);

}

// Закрытие тэгов, флаг окончания html содержимого
static void httpd_body_close(httpd_req_t *req) {
	httpd_resp_sendstr_chunk(req, "</body>");
	httpd_resp_sendstr_chunk(req, "</html>");

	httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t httpd_root_handler(httpd_req_t *req) {

	httpd_base_header(req, 0, NULL);

	httpd_resp_printf(req, "%s", "just test for httpd_resp_printf");
	httpd_resp_sendstr_chunk(req, "static string");

	httpd_body_close(req);

	return ESP_OK;
}

void httpd_watch_task(void *pvData) {
	httpd_handle_t server = NULL; // указатель на http сервер
	httpd_config_t config = HTTPD_DEFAULT_CONFIG(); // дефолнтые значения конфигурации сервера

	exchange_t *exData = initExchange();

	// Обработчики для урлов
	httpd_uri_t httpd_config[] = {
		{.uri = "/",.method = HTTP_GET,.handler = httpd_root_handler,.user_ctx = (void*)exData},
	};

	// .max_uri_handlers по умолчанию 8
	config.max_uri_handlers = (sizeof(httpd_config)/sizeof(httpd_config[0]));

	while(1) {
		EventBits_t uxBits = xEventGroupWaitBits(exData->mainEventGroup, 
			WIFI_CONNECTED_BIT | WIFI_DISCONNECT_BIT, 
			true, false, 5000 / portTICK_PERIOD_MS);

		if ((uxBits & WIFI_CONNECTED_BIT)  && (server == NULL)) {
			// Подключение к вайфаю, старт сервера
			ESP_LOGI(log_name(TAG_HTTPD), "Starting server on port: '%d'", config.server_port);

			if (httpd_start(&server, &config) == ESP_OK) {
				// Set URI handlers

				ESP_LOGI(log_name(TAG_HTTPD), "Registering URI handlers");

				esp_err_t err;
				for(uint16_t i=0; i<config.max_uri_handlers; i++){
					err = httpd_register_uri_handler(server, &httpd_config[i]);

					ESP_LOGI(log_name(TAG_HTTPD), "httpd '%s': %d %s", 
						httpd_config[i].uri, 
						err, esp_err_to_name(err)
					);
				}

			} else {
				ESP_LOGE(log_name(TAG_HTTPD), "Error starting server!");
				server = NULL;
			}

		} else if (uxBits & WIFI_DISCONNECT_BIT && server != NULL) {
			// Отключился от вайфая последний юзер, останавливаем сервер
			ESP_LOGI(log_name(TAG_HTTPD), "WiFi fail, stopping server!");
		}
	}
}