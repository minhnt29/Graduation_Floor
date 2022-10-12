/* *********************************************************************************************************
 *******************                    GRADUATION FLOOR 1                              ********************
 **********************************************************************************************************/

//DHT22 - pin 4
//GAS sensor MQ2  - pin 34
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "DHT22.h"
#include "MQ2.h"

#define BROKER              "mqtt://test.mosquitto.org:1883"   
#define TOPIC_DOOR          "Prj/Door"
#define TOPIC_FLOOR1        "Prj/Floor1"
#define TOPIC_FLOOR2        "Prj/Floor2"
#define TOPIC_FLOOR3        "Prj/Floor3"
#define TOPIC_ACCOUNT       "Prj/Account"
#define TOPIC_PASSWORD      "Prj/Password"
#define TOPIC_FIREALARM     "Prj/Fire"

#define DOOR_CMD_LIGHT               '1'
#define DOOR_CMD_LIGHT_ON            '1'
#define DOOR_CMD_LIGHT_OFF           '0'
#define DOOR_CMD_DOOR                '2'
#define DOOR_CMD_DOOR_OPEN           '1'
#define DOOR_CMD_DOOR_CLOSE          '0'

#define FLOOR1_CMD_LIGHT        '4'
#define FLOOR1_CMD_LIGHT_ON     '1'
#define FLOOR1_CMD_LIGHT_OFF    '0'
#define FLOOR1_CMD_TEMP         '5'
#define FLOOR1_CMD_HUMI         '6'


#define DHT22_PINOUT  4

esp_mqtt_client_handle_t client;

static const char *TAG = "Graduation Floor";
static QueueHandle_t DHT_Temp_queue = NULL,
                     DHT_Hum_queue = NULL,
                     Gas_value_queue = NULL;

static void my_App_Init(void)
{
    DHT_Temp_queue = xQueueCreate(10, sizeof(float));

    DHT_Hum_queue = xQueueCreate(10, sizeof(float));

    Gas_value_queue = xQueueCreate(10, sizeof(float));

    setDHTgpio(DHT22_PINOUT);

    mq2Init();
}

static void get_Gas_Value (void* arg)
{
    float percent_gas_concentration = 0.0;
    char buffer_rcv[5];
    for(;;)
    {
        //get DHT Value
        percent_gas_concentration = mq2GetValue();
        //push DHT Value to topic Fire alarm
        sprintf(buffer_rcv, "%.2f", percent_gas_concentration);
        esp_mqtt_client_publish(client, TOPIC_FIREALARM, buffer_rcv, sizeof(buffer_rcv), 1, 0);

        vTaskDelay(pdTICKS_TO_MS(500));
    }
}

static void get_DHT_Value(void* arg)
{
    float humidity = 0,
          temp = 0;

    for(;;) {
        int rc = readDHT();
        errorHandler(rc);
        humidity = getHumidity();
        temp = getTemperature();

        xQueueSend(DHT_Temp_queue, &temp, 1000/portTICK_PERIOD_MS);
        xQueueSend(DHT_Hum_queue, &humidity, 1000/portTICK_PERIOD_MS);
        vTaskDelay(500);
    }
}

static void send_DHT_Value(void* arg)
{
    float humidity = 0,
          temp = 0;
    char buffer_rcv[5];
    for(;;) {
        if(xQueueReceive(DHT_Temp_queue, &temp, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Received data temp = %.1f", temp);
            sprintf(buffer_rcv, "%c%.1f", FLOOR1_CMD_TEMP, temp);
            esp_mqtt_client_publish(client, TOPIC_FLOOR1, buffer_rcv, sizeof(buffer_rcv), 1, 0);
        }

        if(xQueueReceive(DHT_Hum_queue, &humidity, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Received data humidity = %.1f", humidity);
            sprintf(buffer_rcv, "%c%.1f", FLOOR1_CMD_HUMI, humidity);
            esp_mqtt_client_publish(client, TOPIC_FLOOR1, buffer_rcv, sizeof(buffer_rcv), 1, 0);
        }
    }
}
/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        msg_id = esp_mqtt_client_subscribe(client, TOPIC_FLOOR1, 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "published ok");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = BROKER,
        .keepalive = 60,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}


void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();
    my_App_Init();
    xTaskCreate(get_DHT_Value, "get DHT value", 2048, NULL, configMAX_PRIORITIES-1, NULL);
    xTaskCreate(send_DHT_Value, "push DHT value", 2048, NULL, configMAX_PRIORITIES, NULL);
    xTaskCreate(get_Gas_Value, "get and push Gas value", 2048, NULL, configMAX_PRIORITIES, NULL);
}
