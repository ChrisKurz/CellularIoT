#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <logging/log.h>
#include <modem/lte_lc.h>
#include <net/mqtt.h>
#include <net/socket.h>
#include <random/rand32.h>
#include <dk_buttons_and_leds.h>

LOG_MODULE_REGISTER(mqtt_simple, LOG_LEVEL_INF);

/*** User-defined Symbols (these could also be handled as user-defined KCONFIG, 
 *                         see original mqtt_simple example) ***/
#define CONFIG_LTE_CONNECT_RETRY_DELAY_S   120
#define CONFIG_MQTT_MESSAGE_BUFFER_SIZE    128
#define CONFIG_MQTT_CLIENT_ID              "my-client-id"
#define CONFIG_MQTT_BROKER_HOSTNAME        "mqtt.eclipseprojects.io"
#define CONFIG_MQTT_BROKER_PORT       1883
#define CONFIG_MQTT_RECONNECT_DELAY_S      60
#define CONFIG_MQTT_PUB_TOPIC              "my/publish/topic"
#define CONFIG_BUTTON_EVENT_PUBLISH_MSG    "The message to publish on a button event"
#define CONFIG_BUTTON_EVENT_BTN_NUM        1

/* Buffers for MQTT client. */
static uint8_t rx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t tx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
    
/* The mqtt client struct */
static struct mqtt_client client;

/* MQTT Broker details. */
static struct sockaddr_storage broker;


/**@brief Function to publish data on the configured topic */
static int data_publish(struct mqtt_client *c, enum mqtt_qos qos, uint8_t *data, size_t len)
{
   struct mqtt_publish_param param;

   param.message.topic.qos = qos;
   param.message.topic.topic.utf8 = CONFIG_MQTT_PUB_TOPIC;
   param.message.topic.topic.size = strlen(CONFIG_MQTT_PUB_TOPIC);
   param.message.payload.data = data;
   param.message.payload.len = len;
   param.message_id = sys_rand32_get();
   param.dup_flag = 0;
   param.retain_flag = 0;

   LOG_INF("Publishing: %s", log_strdup(data));
   LOG_INF("to topic: %s", CONFIG_MQTT_PUB_TOPIC);

   return mqtt_publish(c, &param);
}

#if defined(CONFIG_DK_LIBRARY)
static void button_handler(uint32_t button_states, uint32_t has_changed)
{
   if (has_changed & button_states &
          BIT(CONFIG_BUTTON_EVENT_BTN_NUM - 1)) {
      int ret;

      ret = data_publish(&client,
                         MQTT_QOS_1_AT_LEAST_ONCE,
                         CONFIG_BUTTON_EVENT_PUBLISH_MSG,
                         sizeof(CONFIG_BUTTON_EVENT_PUBLISH_MSG)-1);
      if (ret) {
         LOG_ERR("Publish failed: %d", ret);
      }
   }
}
#endif

/**@brief MQTT client event handler */
void mqtt_evt_handler(struct mqtt_client *const c, const struct mqtt_evt *evt)
{
   switch (evt->type) {
   case MQTT_EVT_CONNACK:
      if (evt->result != 0) {
         LOG_ERR("MQTT connect failed: %d", evt->result);
         break;
      }
      LOG_INF("MQTT client connected");
      break;

   case MQTT_EVT_DISCONNECT:
      LOG_INF("MQTT client disconnected: %d", evt->result);
      break;

   case MQTT_EVT_PUBLISH: 
      LOG_INF("MQTT client publish event!");
      break;

   case MQTT_EVT_PUBACK:
      if (evt->result != 0) {
         LOG_ERR("MQTT PUBACK error: %d", evt->result);
         break;
      }
      LOG_INF("PUBACK packet id: %u", evt->param.puback.message_id);
      break;

   case MQTT_EVT_SUBACK:
      if (evt->result != 0) {
         LOG_ERR("MQTT SUBACK error: %d", evt->result);
         break;
      }
      LOG_INF("SUBACK packet id: %u", evt->param.suback.message_id);
      break;

   case MQTT_EVT_PINGRESP:
      if (evt->result != 0) {
         LOG_ERR("MQTT PINGRESP error: %d", evt->result);
      }
      break;

   default:
      LOG_INF("Unhandled MQTT event type: %d", evt->type);
      break;
   }
}

/**@brief Resolves the configured hostname and
 * initializes the MQTT broker structure */
static int broker_init(void)
{
   int err;
   struct addrinfo *result;
   struct addrinfo *addr;
   struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM
   };

   err = getaddrinfo(CONFIG_MQTT_BROKER_HOSTNAME, NULL, &hints, &result);
   if (err) {
      LOG_ERR("getaddrinfo failed: %d", err);
      return -ECHILD;
   }

   addr = result;

   /* Look for address of the broker. */
   while (addr != NULL) {
      /* IPv4 Address. */
      if (addr->ai_addrlen == sizeof(struct sockaddr_in)) {
         struct sockaddr_in *broker4 = ((struct sockaddr_in *)&broker);
         char ipv4_addr[NET_IPV4_ADDR_LEN];

         broker4->sin_addr.s_addr = ((struct sockaddr_in *)addr->ai_addr)->sin_addr.s_addr;
         broker4->sin_family = AF_INET;
         broker4->sin_port = htons(CONFIG_MQTT_BROKER_PORT);

         inet_ntop(AF_INET, &broker4->sin_addr.s_addr, ipv4_addr, sizeof(ipv4_addr));
         LOG_INF("IPv4 Address found %s", log_strdup(ipv4_addr));

         break;
      } 
      else 
      {
         LOG_ERR("ai_addrlen = %u should be %u or %u",
                      (unsigned int)addr->ai_addrlen,
                      (unsigned int)sizeof(struct sockaddr_in),
                      (unsigned int)sizeof(struct sockaddr_in6));
      }

      addr = addr->ai_next;
   }

   /* Free the address. */
   freeaddrinfo(result);

   return err;
}

/**@brief Initialize the MQTT client structure */
static int client_init(struct mqtt_client *client)
{
   int err;

   mqtt_client_init(client);

   err = broker_init();
   if (err) {
      LOG_ERR("Failed to initialize broker connection");
      return err;
   }

   /* MQTT client configuration */
   client->broker = &broker;
   client->evt_cb = mqtt_evt_handler;
   client->client_id.utf8 = CONFIG_MQTT_CLIENT_ID;
   client->client_id.size = strlen(CONFIG_MQTT_CLIENT_ID);       
   client->password = NULL;
   client->user_name = NULL;
   client->protocol_version = MQTT_VERSION_3_1_1;

   /* MQTT buffers configuration */
   client->rx_buf = rx_buffer;
   client->rx_buf_size = sizeof(rx_buffer);
   client->tx_buf = tx_buffer;
   client->tx_buf_size = sizeof(tx_buffer);

   /* MQTT transport configuration */
   client->transport.type = MQTT_TRANSPORT_NON_SECURE;

   return err;
}

/**@brief Configures modem to provide LTE link. Blocks until link is
 * successfully established.
 */
static int modem_configure(void)
{
   int err;
   
   /* Turn off LTE power saving features for a more responsive demo. Also,
    * request power saving features before network registration. Some
    * networks rejects timer updates after the device has registered to the
    * LTE network.
    */
   LOG_INF("Disabling PSM and eDRX");
   lte_lc_psm_req(false);
   lte_lc_edrx_req(false);

   LOG_INF("LTE Link Connecting...");
   err = lte_lc_init_and_connect();
   if (err) {
      LOG_INF("Failed to establish LTE connection: %d", err);
      return err;
   }
   LOG_INF("LTE Link Connected!");
   return 0;
}

void main(void)
{
   int err;

   LOG_INF("The MQTT simple sample started");

#if defined(CONFIG_DK_LIBRARY)
   dk_buttons_init(button_handler);
#endif

   /* initialize modem and connect */
   do {
     err = modem_configure();
     if (err) {
         LOG_INF("Retrying in %d seconds", CONFIG_LTE_CONNECT_RETRY_DELAY_S);
         k_sleep(K_SECONDS(CONFIG_LTE_CONNECT_RETRY_DELAY_S));
      }
   } while (err);

   /* initialize MQTT client */
   err = client_init(&client);
   if (err != 0) {
       LOG_ERR("client_init: %d", err);
   return;
   }

   uint32_t connect_attempt = 0;
   
do_connect:
    if (connect_attempt++ > 0) {
       LOG_INF("Reconnecting in %d seconds...", CONFIG_MQTT_RECONNECT_DELAY_S);
       k_sleep(K_SECONDS(CONFIG_MQTT_RECONNECT_DELAY_S));
    }
    err = mqtt_connect(&client);
    if (err != 0) {
       LOG_ERR("mqtt_connect %d", err);
       goto do_connect;
    }

   while(1)
   {
      mqtt_input(&client);
      k_msleep(1000);

      err = mqtt_live(&client);
      if ((err != 0) && (err != -EAGAIN)) {
         LOG_ERR("ERROR: mqtt_live: %d", err);
//         break;  => here a mqtt disconnect should be done. After that a reconnect can be tried. 
      }

   }
}
