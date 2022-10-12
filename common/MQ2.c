#include "MQ2.h"

static const char *TAG = "ADC SINGLE";

void mq2Init(void)
{
    //ADC1 config
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC1_EXAMPLE_CHAN0, ADC_EXAMPLE_ATTEN));
}

float mq2GetValue(void)
{
    int voltage;
    voltage = adc1_get_raw(ADC1_EXAMPLE_CHAN0);

    //convert voltage to %
    float value_percent = (voltage/3.3)*100;
    ESP_LOGI(TAG, "Get percent value is : %.2f", value_percent);
    return value_percent;
}
