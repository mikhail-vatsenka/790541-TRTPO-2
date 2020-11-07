#include <driver/adc.h>
#include <esp_log.h>

#include "defines.h"

#define HDS_CHANNEL ADC1_CHANNEL_7

void adc_init(void) {
	adc1_config_width(ADC_WIDTH_BIT_10); // 10 бит точности
    adc1_config_channel_atten(HDS_CHANNEL, ADC_ATTEN_DB_11); // для канала 7 входное напряжение до 3.6 вольта
}

void hds_task(void *pvData) {
	exchange_t *exData = initExchange();

	while(1) {
		int val = 1024 - adc1_get_raw(HDS_CHANNEL); // Прочитать значение ADC

		float vout=(float)val * 3.3 / 1024.0; // рассчет напряжения, опорное 3.3 с выхода DC-DC платы
		float res = ((3.3-vout) < 0.05 ? 100.0 : 100.0 * vout / (3.3-vout) ); // рассчет сопротивления датчика

		ESP_LOGI(TAG_HDS, "read: %d | %.2f kOM", val, res);

		vTaskDelay( 3000 / portTICK_PERIOD_MS ); // пауза 3 секунды
	}
}
