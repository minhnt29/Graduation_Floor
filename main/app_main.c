/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
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
#define DOOR_CMD_DOOR_KEYPAD         '3'
#define DOOR_CMD_DOOR_CHANGEPASSWORD '0'
#define DOOR_CMD_DOOR_ADDACCOUNT     '1'

#define FLOOR1_CMD_LIGHT        '4'
#define FLOOR2_CMD_LIGHT_ON     '1'
#define FLOOR2_CMD_LIGHT_OFF    '0'

esp_mqtt_client_handle_t client;

static const char *TAG = "Graduation Floor";
static QueueHandle_t DHT_Temp_queue = NULL,
                     DHT_Hum_queue = NULL,
                     Gas_value_queue = NULL;

static void my_App_Init(void)
{
    DHT_Temp_queue = xQueueCreate(10, sizeof(int ));

    DHT_Hum_queue = xQueueCreate(10, sizeof(int));

    Gas_value_queue = xQueueCreate(10, sizeof(int));
}

static void get_DHT_Value(void* arg)
{
    int data_packet = 0;
    for(;;) {
        ESP_LOGI(TAG, "%d", data_packet);
        data_packet++;
        xQueueSend(DHT_Temp_queue, &data_packet, 1000/portTICK_PERIOD_MS);
    }
}

static void send_DHT_Value(void* arg)
{
    int data_packet = 0;
    for(;;) {
        if(xQueueReceive(DHT_Temp_queue, &data_packet, portMAX_DELAY)) {
           esp_mqtt_client_publish(client, TOPIC_FLOOR1, &data_packet, sizeof(int), 1, 0);
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
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
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

    char *io_num = "hello";
    mqtt_app_start();
    esp_mqtt_client_publish(client, TOPIC_FLOOR1, io_num, strlen(io_num), 1, 0);
    my_App_Init();
    xTaskCreate(get_DHT_Value, "gpio_task_example", 2048, NULL, configMAX_PRIORITIES-1, NULL);
    xTaskCreate(send_DHT_Value, "gpio_task_example", 2048, NULL, configMAX_PRIORITIES, NULL);
}
