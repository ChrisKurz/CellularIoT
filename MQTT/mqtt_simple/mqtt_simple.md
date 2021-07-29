# Simple MQTT project - created from scratch

This document describes how to create the mqtt_simple project from scratch using nRF Connect SDK V1.6.0. Note that it follows the official nrf/samples/nrf9160/mqtt_simple sample, but some simplifications are done here... Subscribe is currently not included! Only uncrypted TCP is used here!

We will use the nRF9160DK kit for testing. Please use the Programmer to write the modem firmware image to the nRF9160. I am using Modem Firmware V1.3.0 and nRF9160DK V0.15.0.

Note: The completed project can be found in CellularIoT/MQTT/mqtt_simple/PRJ_mqtt_simple

## 1. Creating project folder and files

First step is to create project files:

- CMakeLists.txt

      cmake_minimum_required(VERSION 3.13.1)
      
      # Find external Zephyr project, and load its settings:ChrisKurz
      find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
      
      # Set project name:
      project(mqtt-simple)
      
      # add sources
      target_sources(app PRIVATE
                        src/main.c)

- prj.conf     =>   At the moment this file is empty!

- src/main.c   =>     At the moment this file is empty!  


## 2. Open the project and add main function

Start Segger Embedded Studio and open an nRF Connect SDK project.

Select the project folder and the board "nrf9160dk_nrf9160ns".

Add following code to main.c file:

    #include <zephyr.h>
    #include <stdio.h>
    #include <string.h>

    void main(void)
    {
       while(1)
       {
   
       }
    }

## 3. Let's add Zephyr Logging to the project

First, we have to enable the Zephyr Logging software module by adding following line into the proj.conf file:

    # Logging
    CONFIG_LOG=y
    CONFIG_LOG_MODE_MINIMAL=y

The CONFIG_LOG_MODE_MINIMAL configures the logging in a way that not too much overhead is sent with the message.

Then we have to register the Logging in our software module. This is done by adding following lines before the main function:

    #include <logging/log.h>
    
    LOG_MODULE_REGISTER(mqtt_simple, LOG_LEVEL_INF);

First parameter in LOG_MODULE_REGISTER defines a software module name, that can freely be defined by the user. The second parameter is used to define the log level. This means, depending on the setting different output function are enabled or disabled. Following settings are possible:

- LOG_LEVEL_NONE => no output, Logging is disabled
- LOG_LEVEL_ERR  => only LOG_ERR() messages are shown
- LOG_LEVEL_WRN  => only LOG_ERR() and LOG_WRN() messages are shown
- LOG_LEVEL_INF  => only LOG_ERR(), LOG_WRN(), and LOG_INF() messages are shown
- LOG_LEVEL_DBG  => only LOG_ERR(), LOG_WRN(), LOG_INF(), and LOG_DBG() messages are shown

 Let's add in main function following line, which print a start message:
 
       LOG_INF("The MQTT simple sample started");

## 4. Configure and initialization of modem using _LTE Link Controller_ Library

The LTE link controller library is intended to be used for simpler control of LTE connections on an nRF9160 SiP. This library provides the API and configurations for setting up an LTE connection with definite properties. Some of the properties that can be configured are:
- Access Point Name (APN)
- support for LTE modes (NB-IoT or LTE-M)
- support for GPS
- Power Saving Mode (PSM)
- extended Discontinuous Reception (eDRX) parameters
- the modem can be locked to use specific LTE bands

### LTE Link Controller KCONFIG Settings

Let's start with the necessary KCONIFG settings. Add following lines to prj.conf file:

    # Modem Settings:
    #--- LTE link control
    CONFIG_LTE_LINK_CONTROL=y
    CONFIG_LTE_AUTO_INIT_AND_CONNECT=n

CONFIG_LTE_AUTO_INIT_AND_CONNECT is disabled here. Note that by default this CONFIG symbol is set. In our case we want to control the settings by software and also trigger by software the connection process. 

When you try to open the project now, you will see that it fails. When you take a closer look into the CONFIG_LTE_LINK_CONTROL description you will see that there are other CONFIG symbols that are set and which have dependencies. Let's check the CONFIG_LTE_LINK_CONTROL description by clicking [here](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/1.6.0/kconfig/CONFIG_LTE_LINK_CONTROL.html?highlight=config_lte_link_control#cmdoption-arg-CONFIG_LTE_LINK_CONTROL).


Enabling LTE Link Control also enables CONFIG_AT_CMD, CONFIG_AT_CMD_PARSER, and CONFIG_AT_NOTIF. 

CONFIG_AT_CMD includes the _AT Command driver_. This driver has a dependency and requires to enable Modem library. So you have to set CONFIG_NRF_MODEM_LIB=y. (see AT_CMD description)

CONFIG_AT_CMD_PARSER includes the _AT command parser library_. This driver requires either to set CONFIG_NEWLIBC or CONFIG_EXTERNAL_LIBC. The Newlib C library is part of the nRF Connect SDK, so we add this one by setting CONFIG_NEWLIBC=y. (see AT_CMD_PARSER description)

CONFIG_AT_NOTIF includes the _AT command notification manager_. It requires the AT Command dirver, which we have already included above. 

So in the prj.conf file we have to add following lines:

    # Modem library (needed when application communicates with modem firmware,
    #                e.g. using AT commands or using LTE LC Link Control library)
    CONFIG_NRF_MODEM_LIB=y

    # NewLib C
    CONFIG_NEWLIB_LIBC=y

Note that the Modem library is needed as soon as the nRF9160 application wants to communicate with the modem firmware.

Opening the project is now done without an error. 

In main function declare the variable err:

       int err;

and add following lines before entering the entire loop:

       /* initialize modem and connect */
       do {
         err = modem_configure();
         if (err) {
             LOG_INF("Retrying in %d seconds", CONFIG_LTE_CONNECT_RETRY_DELAY_S);
             k_sleep(K_SECONDS(CONFIG_LTE_CONNECT_RETRY_DELAY_S));
          }
       } while (err);

Here we basically wait till a connection is established. 

Note that CONFIG_LTE_CONNECT_RETRY_DELAY_S is a user-defined KCONFIG symbol. In this example we simplify the code by just using a #define instruction. Add following at the top of the main.c file:
   
    #define CONFIG_LTE_CONNECT_RETRY_DELAY_S   120
    
Next, we use the LTE Link Control library to configure the modem and try to connect. Add this function before main function:

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

The usage of the lte_lc_psm_req, lte_lc_edrx_req and lte_lc_init_and_connect functions require to include the lte_lc.h header file. Add the following include instruction:

    #include <modem/lte_lc.h>

Before we can compile the project we have to add Networking and Socket support. 

### Networking, Socket support and Heap/Stack

First, we need to add Networking and Socket to our prj.conf file. Note that we alow have to reserve some memory for the heap. Add following lines to prj.conf:

    # Networking and Socket
    CONFIG_NETWORKING=y
    CONFIG_NET_SOCKETS=y

    # Memory
    CONFIG_MAIN_STACK_SIZE=4096
    CONFIG_HEAP_MEM_POOL_SIZE=2048

Note: definition of stack and heap size was take from the NCS mqtt_simple project. Most probably it can be optimized.

Compiling of the project should now be possible without any errors. 

Download the firmware to the nRF9160 board (select board: nrf9160dk_nrf9160ns), ensure a SIM card is put into the SIM card slot of the nRF9160DK board, connect USB to PC, open on PC a terminal program (settings: 115200 Bd, 8 data bits, no parity, 1 stop bit, no flow control). You should see in the terminal the following messages in case of a successful connection:

![](images/terminal.JPG)
    
## MQTT 

### initialize MQTT client

Next, we do the configuration of the MQTT client. Add following lines after modem initialization and connection in the main function. 

       /* initialize MQTT client */
       err = client_init(&client);
       if (err != 0) {
           LOG_ERR("client_init: %d", err);
       return;
       }

We have to add the mqtt.h header file, create buffers, and declare _client_ and _broker_ at the top of the main.c file:

    #include <net/mqtt.h>
    
    /* Buffers for MQTT client. */
    static uint8_t rx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
    static uint8_t tx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
        
    /* The mqtt client struct */
    static struct mqtt_client client;
    
    /* MQTT Broker details. */
    static struct sockaddr_storage broker;

The original mqtt_simple example project uses user-defined KCONFIG symbols. We simplify this in this project by defining the CONFIG_MQTT_MESSAGE_BUFFER_SIZE manually via the following define:

    #define CONFIG_MQTT_MESSAGE_BUFFER_SIZE    128
     
and we have to add the _client_init_ function:

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

The function _mqtt_client_init_ is a function form the Socket MQTT library. We have to enable this library. Beside that we want to ensure that the MQTT broker has no old messages and therefore we clean it by setting CONFIG_MQTT_CLEAN_SESSION. 

    # MQTT
    CONFIG_MQTT_LIB=y
    CONFIG_MQTT_LIB_TLS=n
    CONFIG_MQTT_CLEAN_SESSION=y    

The original mqtt_simple example project generates the client id for the nRF9160DK based on its IMEI number. We keep this simpler in this example and define it by adding following line to main.c file:

    #define CONFIG_MQTT_CLIENT_ID              "my-client-id"

The _client_init_ function calls the _broker_init_ function. This is also a user-defined function that does all the needed settings for connecting to a broker.
Add the _broker_init_ function to your project:

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

Note that here some socket defnitions are needed. To include these in the project add the following include:

    #include <net/socket.h>
    
Beside that you maybe have also recognized that there are again CONFIG symbols that has to be defined. Again, the original mqtt_simple project uses here user-defined CONFIGs. We simplify this in our project and add following defines in main.c file:

    #define CONFIG_MQTT_BROKER_HOSTNAME        "mqtt.eclipseprojects.io"
    #define CONFIG_MQTT_BROKER_PORT            1883

mqtt.eclipseprojects.io is a public test MQTT broker service. It currently listens on folloiwng ports:
- 1883:   MQTT over uncryted TCP  (this is what we use in our example here)
- 8883:   MQTT over encryted TCP
- 80:     MQTT over unencryted WebSockets
- 443:    MQTT over encryted WebSockets

### MQTT Event Handler

Next, we add the MQTT event handler. Let's start with the blank MQTT event handler:

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

The project should now be again in a stage that allows to build the project and run it on the nRF9160DK. The board now also finds the MQTT server (MQTT broker). This is shown by printing the IPv4 address in the terminal. 

![](images/terminal2.JPG)

### MQTT connect to server

The next step is to connect to the MQTT server (broker). In main function we add following lines just before the entire loop:

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

CONFIG_MQTT_RECONNECT_DELAY_S is in the original mqtt_simple example also a user-defined KCONFIG. We define it here jus with the following line:

    #define CONFIG_MQTT_RECONNECT_DELAY_S      60

If you have done this, you will see that the mqtt_event_handler is not yet called. The reason is because the application has to take care about getting updates. So in the application we have to either call _mqtt_input()_ or _poll()_ function. Note, that _mqtt_input()_ is a non-blocking function. And _poll()_ is blocking. The original mqtt_simple code example uses the poll function. In our project we use mqtt_input function. So add the following lines in the main's entire loop:

          mqtt_input(&client);
          k_msleep(1000);

Build the project, download on nRF9160DK, and start code execution. You should see that the LTE link is connected, the IPv4 address of the MQTT broker is seen, and the MQTT client connects to the MQTT broker. Note, that after 60 seconds the MQTT client is disconnecting. This disconnect happens because there is the timeout setting CONFIG_MQTT_KEEPALIVE in the MQTT library, which is set by default to 60 seconds.

![](images/terminal3.JPG)

There is the need of periodically calling the function _mqtt_live_ to upkeep the connection. We keep it simple and just add the following lines into the main's entire loop:

          err = mqtt_live(&client);
          if ((err != 0) && (err != -EAGAIN)) {
             LOG_ERR("ERROR: mqtt_live: %d", err);
    //         break;  => here a mqtt disconnect should be done. After that a reconnect can be tried. 
          }


### Send data (Publish)

A publisher sends a message to the network. This is what we want to do now. Let's add following function:

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

The original mqtt_simple example uses again a user-defined KCONFIG symbol for defining the topic that is used for publishing. We simplify this in this project and use the following define:

    #define CONFIG_MQTT_PUB_TOPIC              "my/publish/topic"
    
and we have to add following include instruction to allow us to use the function _sys_rand32()_:

    #include <random/rand32.h>

The publish is triggered by the following lines (do not yet copy these lines to your project!):

       ret = data_publish(&client,
                          MQTT_QOS_1_AT_LEAST_ONCE,
                          CONFIG_BUTTON_EVENT_PUBLISH_MSG,
                          sizeof(CONFIG_BUTTON_EVENT_PUBLISH_MSG)-1);
       if (ret) {
          LOG_ERR("Publish failed: %d", ret);
       }

A way to include these lines in our project is to use a button press event. Add following in prj.conf:

    # Button support
    CONFIG_DK_LIBRARY=y

and define the button callback handler by adding following line in main function:

    #if defined(CONFIG_DK_LIBRARY)
       dk_buttons_init(button_handler);
    #endif

Following inlcude is needed for the _dk_buttons_init()_ function:

    #include <dk_buttons_and_leds.h>

Now, add the button handler, which also includs pusblish of data:

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

and in main.c file we have to add following definitions:

    #define CONFIG_BUTTON_EVENT_PUBLISH_MSG    "The message to publish on a button event"
    #define CONFIG_BUTTON_EVENT_BTN_NUM        1




### Receive a message (Subscribe)

Subscribe is currently not included in this project! Please check original mqtt_simple project.




