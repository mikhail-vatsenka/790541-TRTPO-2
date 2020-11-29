#include <esp_http_server.h>

#include <esp_log.h>
#include <esp_wifi.h>

#include "cfg.h"
#include "defines.h"
#include "gps.h"
#include "httpd.h"
#include "log.h"


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

esp_err_t httpd_resp_print_gps_n_odo(httpd_req_t *req) { //функция отображения данных из очередей обмена
	exchange_t *exData = initExchange(); //получаем ссылку на очереди обмена данными

	odometer_data odo; //переменная под данные одометра
	httpd_resp_sendstr_chunk(req, "Odometer: "); //подпись раздела
	if (xQueuePeek(exData->odoQueue, (void*)&odo, 50 / portTICK_PERIOD_MS) == pdTRUE) { //если получили данные
		httpd_resp_printf(req, "<b>%.3f</b>km, Speed: <b>%.1f</b>km/h", odo.distance, odo.avgSpeed); //выводим в браузер
	} else {
		httpd_resp_sendstr_chunk(req, "<span class='red'>No data </span>"); //если данных пока нет (при старте устройства)
	}
	httpd_resp_sendstr_chunk(req, "<hr>"); //визуальный разделитель

	gps point; //переменная под координаты
	httpd_resp_sendstr_chunk(req, "GPS: "); //подпись раздела
	if(ts_get(exData->nmeaTS) < 3) { //если валидное сообщение получено менее трёх секунд назад
		if (xQueuePeek(exData->gpsQueue, (void*)&point, 50 / portTICK_PERIOD_MS) == pdTRUE) { //считать данные из очереди
			httpd_resp_printf(req, "<b>%f %f</b> hDOP: %2.1f", point.rmc.lat, point.rmc.lon, point.gsa.hdop); //вывести две координаты и фактор точности
		} else {
			httpd_resp_sendstr_chunk(req, "<span class='red'>No data </span>"); //Нет данных
		}
	} else {
		httpd_resp_sendstr_chunk(req, "<span class='red'>Lost GPS signal</span>&nbsp;&nbsp;"); //выводит "потеря GPS сигнала"
		if(ts_get(exData->uartTS) > 3) //проверяем таймаут UART, не в проводах ли дело...
			httpd_resp_sendstr_chunk(req, "<span class='red bold'>Lost UART signal</span>"); 
	}
	httpd_resp_sendstr_chunk(req, "<hr>"); //визуальный разделитель

	oiler_config_t *oCfg = config_oil(); //получили указатель на настройки 
	uint8_t dew = 0; //
	if (oCfg->dewKf > 0 && xQueuePeek(exData->humidityTimerQueue, &dew, 10 / portTICK_PERIOD_MS) != pdTRUE) { //если в очереди нет сообщений
		dew = 0; //обнуляем значение влажности
	}
	httpd_resp_printf(req, "Humidity: <b>%d%%</b>", dew); //выводим данные от датчика влажности

	httpd_resp_sendstr_chunk(req, "<hr>"); //визуальный разделитель

	return ESP_OK;
}

static esp_err_t httpd_xhr_gps_odo_handler(httpd_req_t *req) { //обработчик фонового запроса
	httpd_resp_print_gps_n_odo(req); //функция отображения данных из очередей обмена

	httpd_body_close(req);
	return ESP_OK;
}

static esp_err_t httpd_root_handler(httpd_req_t *req) { //обработчик главной страницы

	httpd_base_header(req, 0, NULL); //вывод базовых заголовков

	httpd_resp_sendstr_chunk(req, "<div id='odo'>"); //формируем "контейнер" для отображения текущих данных
	httpd_resp_sendstr_chunk(req, "<script>document.addEventListener('DOMContentLoaded', function(){setTimeout(gpsDataUpdate, 3000)});</script>"); //активация функции фонового обновления данных
	httpd_resp_print_gps_n_odo(req);
	httpd_resp_sendstr_chunk(req, "</div>");
	

	httpd_resp_sendstr_chunk(req, "<a href='/logs'>Logs config</a><hr>");
	httpd_resp_sendstr_chunk(req, "<a href='/oiler'>Oiler config</a><hr>");

	httpd_body_close(req);
	return ESP_OK;
}

static esp_err_t httpd_oiler_handler_xhr(httpd_req_t *req) { //обработка функции oilFill(time) из jsCode 
	uint16_t u16=0;
	exchange_t *exData = initExchange(); //получаем ссылку на очередь обмена данными
	oiler_config_t *oilCfg = config_oil(); //получаем ссылку на конфигурацию смазчика

	char* query = strchr(req->uri, '?'); //перескочить на часть запроса за URI
	if (query != NULL) {
		char buf[15];
		query++;
		if( httpd_query_key_value( query , "fill", (char*)buf, sizeof(buf)) == ESP_OK) { //если в запросе переданно "заполнить систему"

			if(sscanf(buf, "%"SCNu16, &u16) == 0) //и если не пришло значение времени (если jsCode задействован кнопкой "Test")
				u16 = oilCfg->pulseTime; // if `fill` is not set use default from config
		}

		ESP_LOGD(log_name(TAG_HTTPD), "Fill value: %d", u16);
		xQueueSend(exData->oilTimerQueue, &u16, 100/portTICK_PERIOD_MS); //подаём в очередь команду для насоса с временем работы
	}

	httpd_resp_sendstr_chunk(req, NULL);
	return ESP_OK;
}

static esp_err_t httpd_oiler_handler_set(httpd_req_t *req) { //функция обработки полей ввода и кнопок на странице настроек автосмазчика
	httpd_base_header(req, 2, "/oiler");
	httpd_resp_sendstr_chunk(req, "test set");
	return ESP_OK;
}

static esp_err_t httpd_oiler_handler(httpd_req_t *req) { //обработчик URL /oiler
	oiler_config_t *oilCfg = config_oil(); //получаем указатель на структуру настроек

	httpd_base_header(req, 0, NULL); //выводим базовые заголовки без рефреша страницы

	httpd_resp_sendstr_chunk(req, "<style>"); //прописываем стили для элементов формы
	httpd_resp_sendstr_chunk(req, "form#oilerConfig label {display: inline-block;width: 200px;}");
	httpd_resp_sendstr_chunk(req, "form#oilerConfig input {padding-left: 3px;width: 100px;}");
	httpd_resp_sendstr_chunk(req, "</style>");

	httpd_resp_sendstr_chunk(req, "<form id='oilerConfig' method=GET action='/oiler/set'>"); //компонуем форму
	httpd_resp_sendstr_chunk(req, "<input type=hidden name=hack>");

	httpd_resp_sendstr_chunk(req, "<label for='pulseTime'>Pulse time, ms:"); //подпись поля
	httpd_resp_sendstr_chunk(req, "<button style='float: right;margin-right: 15px;height: 21px' onclick='return oilFill();' >Test</button>"); //кнопа Test = функция oilFill() в jsCode
	httpd_resp_sendstr_chunk(req, "</label>");
	httpd_resp_printf(req, "<input id='pulseTime' type='number' min='10' max='1000' name='pulseTime' value='%d'><br />", oilCfg->pulseTime); //поле ввода с данными из конфигурации

	httpd_resp_sendstr_chunk(req, "<span class='gray small'>Time for one pump pulse. Adjust it to set ounce size."); //поясняющая надпись
	httpd_resp_sendstr_chunk(req, "<br/>'Test' button is used to test ounce size (pulse time) time without saving config</span>"); //поясняющая надпись

	httpd_resp_sendstr_chunk(req, "<hr>"); //визуальный разделитель

	httpd_resp_sendstr_chunk(req, "<label for='baseDistance'>Distance, m: </label>");
	httpd_resp_printf(req, "<input type='number' id='baseDistance' min='1' max='16484' name='baseDistance' value='%d'><br />", oilCfg->baseDistance);
	httpd_resp_sendstr_chunk(req, "<span class='gray small'>Basic distance between pump pulses</span>");

	httpd_resp_sendstr_chunk(req, "<hr>");

	httpd_resp_sendstr_chunk(req, "<label for='hwSpeed'>Highway speed, km/h: </label>");
	httpd_resp_printf(req, "<input type='number' id='hwSpeed' name='hwSpeed' value='%d'><br />", oilCfg->hwSpeed);
	httpd_resp_sendstr_chunk(req, "<span class='gray small'>Minimal speed to switch to 'highway' mode</span><br/>");

	httpd_resp_sendstr_chunk(req, "<label for='hwKf'>Highway factor: </label>");
	httpd_resp_printf(req, "<input type='number' id='hwKf' name='hwKf' min=0 max='2.4' step='0.01' value='%.2f'><br />", (float)oilCfg->hwKf/100.0);
	httpd_resp_sendstr_chunk(req, "<span class='gray small'>Multiplier that will be applied to basic distance. ");
	httpd_resp_sendstr_chunk(req, "As an example, for basic distance 1000m and koeff 0.9 you'll get 900m distance between drops. ");
	httpd_resp_sendstr_chunk(req, "Set it to 1 to avoid pump mode change. </span>");
	httpd_resp_sendstr_chunk(req, "<span class='red small'>There might be some drift because of conversion between int and float values.</span>");


	httpd_resp_sendstr_chunk(req, "<hr>");

	httpd_resp_sendstr_chunk(req, "<label for='dewValue'>Humidity, %: </label>");
	httpd_resp_printf(req, "<input type='number' id='cdewValue' name='dewValue' value='%d'><br />", oilCfg->dewValue);
	httpd_resp_sendstr_chunk(req, "<span class='gray small'>Minimal humidity to apply it's koeff</span><br/>");

	httpd_resp_sendstr_chunk(req, "<label for='dewKf'>Humidity factor: </label>");
	httpd_resp_printf(req, "<input type='number' id='dewKf' name='dewKf' min=0 max='2.4' step='0.01' value='%.2f'><br />", (float)oilCfg->dewKf/100.0);
	httpd_resp_sendstr_chunk(req, "<span class='gray small'>As for 'highway' mode, basic (or highway) distance this will be multiplied with this factor. ");
	httpd_resp_sendstr_chunk(req, "As an example, in case of rain on highway you'll get distance 1000*0.9(highway factor)*0.9(humidity factor) ~ 800m. ");
	httpd_resp_sendstr_chunk(req, "Set it to 1 to avoid pump mode change. </span>");
	httpd_resp_sendstr_chunk(req, "<span class='red small'>There might be some drift because of conversion between int and float values.</span>");

	httpd_resp_sendstr_chunk(req, "<hr>");

	httpd_resp_sendstr_chunk(req, "<label for='gpsTimeout'>GPS timeout, sec: </label>");
	httpd_resp_printf(req, "<input type='number' id='gpsTimeout' name='gpsTimeout' value='%d'><br />", oilCfg->gpsTimeout);
	httpd_resp_sendstr_chunk(req, "<span class='gray small'>Wait this time when GPS signal lost before switch to timer mode</span><br/>");

	httpd_resp_sendstr_chunk(req, "<label for='timerPulseDelay'>Pump period, sec: </label>");
	httpd_resp_printf(req, "<input type='number' id='timerPulseDelay' name='timerPulseDelay' value='%d'><br />", oilCfg->timerPulseDelay);
	httpd_resp_sendstr_chunk(req, "<span class='gray small'>Time between pulses in 'timer' mode </span><br/>");

	httpd_resp_sendstr_chunk(req, "<hr>");
	httpd_resp_sendstr_chunk(req, "<input type='submit' name='save' value='Save'>&nbsp;&nbsp;"); // кнопка Save = method=GET action='/oiler/set'
	httpd_resp_sendstr_chunk(req, "&nbsp;&nbsp;<input type='submit' name='fill' value='Fill' onclick='return oilFill(5000);'>"); // кнопа Fill = функция oilFill(time) в jsCode
	httpd_resp_sendstr_chunk(req, "&nbsp;&nbsp;<input type='submit' name='reset' value='Reset config'>"); // кнопка Reset config = 

	httpd_resp_sendstr_chunk(req, "</form>"); //конец формы

	httpd_body_close(req);
	return ESP_OK;
}

void httpd_watch_task(void *pvData) {
	httpd_handle_t server = NULL; // указатель на http сервер
	httpd_config_t config = HTTPD_DEFAULT_CONFIG(); // дефолнтые значения конфигурации сервера

	exchange_t *exData = initExchange();

	// Обработчики для урлов
	httpd_uri_t httpd_config[] = {
		{.uri = "/",.method = HTTP_GET,.handler = httpd_root_handler,.user_ctx = (void*)exData},  // главная страница
		{.uri = "/data/gps",	.method = HTTP_GET, .handler = httpd_xhr_gps_odo_handler,	.user_ctx  = (void*)exData}, // фоновый запрос обновления данных на главной странице, вызывается через jsCode
		{.uri = "/oiler",		.method = HTTP_GET, .handler = httpd_oiler_handler,			.user_ctx  = (void*)exData}, // страница с настройками смазчика
		{.uri = "/oiler/set",	.method = HTTP_GET, .handler = httpd_oiler_handler_set,		.user_ctx  = (void*)exData}, // сохранение и сброс настроек смазчика
		{.uri = "/oiler/xhr",	.method = HTTP_GET, .handler = httpd_oiler_handler_xhr,		.user_ctx  = (void*)exData}, // кнопки команд для насоса
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