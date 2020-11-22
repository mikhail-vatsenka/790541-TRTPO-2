#include <esp_http_server.h>

#include <esp_log.h>
#include <esp_wifi.h>

#include "defines.h"
#include "log.h"

#include "httpd.h"

static esp_err_t httpd_root_handler(httpd_req_t *req) {
  	httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
	httpd_resp_set_hdr(req, "Pragma","no-cache");
  	httpd_resp_set_hdr(req, "Expires","0");

	httpd_resp_sendstr_chunk(req, "<html lang='en' xml:lang='en' xmlns='http://www.w3.org/1999/xhtml'>");
	httpd_resp_sendstr_chunk(req, "<head>");
	httpd_resp_sendstr_chunk(req, "<meta name='viewport' content='width=device-width, initial-scale=1'>");

	httpd_resp_sendstr_chunk(req, "</head><body>");
	httpd_resp_sendstr_chunk(req, "</body>");

	httpd_resp_send_chunk(req, NULL, 0);
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