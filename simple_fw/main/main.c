/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_vfs_dev.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "sys/param.h"

static const char *TAG = "simple";

static EventGroupHandle_t s_connect_event_group;
static esp_ip4_addr_t s_ip_addr;
static const char *s_connection_name;
static esp_netif_t *s_example_esp_netif = NULL;

/* set up connection, Wi-Fi or Ethernet */
static void start(void);

/* tear down connection, release resources */
static void stop(void);

static void on_got_ip(void *arg, esp_event_base_t event_base,
            int32_t event_id, void *event_data) {
  ESP_LOGI(TAG, "Got IP event!");
  ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
  memcpy(&s_ip_addr, &event->ip_info.ip, sizeof(s_ip_addr));
  xEventGroupSetBits(s_connect_event_group, 1);
}

esp_err_t connect(void) {
  if (s_connect_event_group != NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  s_connect_event_group = xEventGroupCreate();
  start();
  ESP_ERROR_CHECK(esp_register_shutdown_handler(&stop));
  ESP_LOGI(TAG, "Waiting for IP");
  xEventGroupWaitBits(s_connect_event_group, 1, true, true, portMAX_DELAY);
  ESP_LOGI(TAG, "Connected to %s", s_connection_name);
  ESP_LOGI(TAG, "IPv4 address: " IPSTR, IP2STR(&s_ip_addr));
  return ESP_OK;
}

esp_err_t example_disconnect(void) {
  if (s_connect_event_group == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  vEventGroupDelete(s_connect_event_group);
  s_connect_event_group = NULL;
  stop();
  ESP_LOGI(TAG, "Disconnected from %s", s_connection_name);
  s_connection_name = NULL;
  return ESP_OK;
}

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
  esp_err_t err = esp_wifi_connect();
  if (err == ESP_ERR_WIFI_NOT_STARTED) {
    return;
  }
  ESP_ERROR_CHECK(err);
}

static void start(void) {
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_WIFI_STA();

  esp_netif_t *netif = esp_netif_new(&netif_config);

  assert(netif);

  esp_netif_attach_wifi_station(netif);
  esp_wifi_set_default_wifi_sta_handlers();

  s_example_esp_netif = netif;

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));

  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  wifi_config_t wifi_config = {
    .sta = {
      .ssid = "PLACEHOLDER_FOR_WIFI_SSID*6789",
      .password = "PLACEHOLDER_FOR_WIFI_PASSWORD*012345678901234567890123456789",
    },
  };
  ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_connect());
  s_connection_name = "wifi";
}

static void stop(void) {
  ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect));
  ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip));
  esp_err_t err = esp_wifi_stop();
  if (err == ESP_ERR_WIFI_NOT_INIT) {
    return;
  }
  ESP_ERROR_CHECK(err);
  ESP_ERROR_CHECK(esp_wifi_deinit());
  ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(s_example_esp_netif));
  esp_netif_destroy(s_example_esp_netif);
  s_example_esp_netif = NULL;
}

static esp_err_t hello_get_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "Serving hello");
  const char* resp_str = "PLACEHOLDER_FOR_MESSAGE*456789012345678901234567890123456789";
  httpd_resp_send(req, resp_str, strlen(resp_str));
  return ESP_OK;
}

static const httpd_uri_t hello = {
  .uri     = "/hello",
  .method  = HTTP_GET,
  .handler   = hello_get_handler,
};

static httpd_handle_t start_webserver(void) {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Start the httpd server
  ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK) {
    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &hello);
    return server;
  }

  ESP_LOGI(TAG, "Error starting server!");
  return NULL;
}

static void stop_webserver(httpd_handle_t server) {
  // Stop the httpd server
  httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base, 
                 int32_t event_id, void* event_data) {
  httpd_handle_t* server = (httpd_handle_t*) arg;
  if (*server) {
    ESP_LOGI(TAG, "Stopping webserver");
    stop_webserver(*server);
    *server = NULL;
  }
}

static void connect_handler(void* arg, esp_event_base_t event_base, 
              int32_t event_id, void* event_data) {
  httpd_handle_t* server = (httpd_handle_t*) arg;
  if (*server == NULL) {
    ESP_LOGI(TAG, "Starting webserver");
    *server = start_webserver();
  }
}

void app_main(void) {
  static httpd_handle_t server = NULL;

  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(connect());
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

  server = start_webserver();
}
