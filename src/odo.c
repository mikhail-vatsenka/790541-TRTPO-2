#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "defines.h"
#include "log.h"
#include "gps.h"
#include "odo.h"

void odometer_task(void *pvData) {
	exchange_t *exData = (exchange_t*)pvData;
	gps point[2];
	point[0].valid = false;
	point[1].valid = false;
	uint8_t errCount = 0;
	odometer_data odo = {
		.distance = 0,
		.avgSpeed = 0,
	};
    while(1) {
		EventBits_t uxBits = xEventGroupWaitBits(exData->mainEventGroup, 
			GPS_POINT_READY, true, false, 1000 / portTICK_PERIOD_MS);

		
		if((uxBits & GPS_POINT_READY) == 0) continue;
		if(xQueuePeek(exData->gpsQueue, (void*)&point[1], 200) == pdFALSE) continue;

		if (!(point[0].valid & point[1].valid)) {
			// array is not filled, mean task is just started and/or we got first point
			ESP_LOGI(log_name(TAG_ODO), "Got first point");
			memcpy((void*)&point[0], (void*)&point[1], sizeof(gps));
			continue;
		} 

		if (point[1].gsa.hdop > 4) continue;

		float dDist = gps_dist(&point[0], &point[1]);
		float errRadius =  GPS_HDOP_TO_METERS * (point[0].gsa.hdop > point[1].gsa.hdop ? point[0].gsa.hdop : point[1].gsa.hdop);

		// Расстояние между «базовой» нулевой точкой и новой < суммы радиусов погрешности
		// http://www.arts-union.ru/articles/altmer/gpsfilter.htm
		if( dDist * 1000 < errRadius ) {
			errCount++;
			// пробуем отфильтровать дрейф GPS. Если получаем последвотельные ошибки, перемещаем на последнюю базовую точку 
			if (errCount > 100) {
				memcpy((void*)&point[0], (void*)&point[1], sizeof(gps));
				errCount=0;
			}
			continue;
		};

		errCount=0;

		odo.distance += dDist;
		odo.avgSpeed = (odo.avgSpeed + point[1].rmc.speed*3.6 )/2;

		ESP_LOGD(log_name(TAG_ODO), "Point: [%f %f] [%2.1f %d] [D: %.3f S:%.1f]", 
			point[1].rmc.lat, point[1].rmc.lon, 
			point[1].gsa.hdop, point[1].gga.sat, 
			odo.distance, odo.avgSpeed);

		memcpy((void*)&point[0], (void*)&point[1], sizeof(gps)); // shift points

		xQueueReset(exData->odoQueue);
		xQueueSend(exData->odoQueue, &odo, 10); 
		xEventGroupSetBits(exData->mainEventGroup, ODO_UPDATED);

	}
}