#ifndef _MQ2_H_
#define _MQ2_H_

#include "driver/adc.h"
#include "esp_system.h"
#include "esp_log.h"

//ADC Channels
#define ADC1_EXAMPLE_CHAN0          ADC1_CHANNEL_6

//ADC Attenuation
#define ADC_EXAMPLE_ATTEN           ADC_ATTEN_DB_11

void mq2Init(void);
float mq2GetValue(void);

#endif
