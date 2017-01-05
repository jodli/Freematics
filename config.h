#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

/**************************************
* Data logging settings
**************************************/
#define ENABLE_DATA_CACHE 1
#define CACHE_SIZE 384 /* bytes */
#define ENABLE_DATA_OUT 0
#define STREAM_BAUDRATE 115200
#define STREAM_FORMAT FORMAT_TEXT

/**************************************
* WIFI and networking settings
**************************************/
#define USE_ESP8266 1

#define XBEE_BAUDRATE 9600
// change SSID and PASSWORD to your own
#define WIFI_SSID "WLAN FritzBox 7490"
#define WIFI_PASSWORD "xxx"
// change YOUR_SERVER_KEY to your server key
#define SERVER_KEY "YOUR_SERVER_KEY"
#define SERVER_URL "5wodf3rbojgxdiqe.myfritz.net"
#define SERVER_PORT 443

// maximum consecutive errors before performing a reconnection
#define MAX_ERRORS_RECONNECT 3
// maximum consecutive errors before performing a module reset
#define MAX_ERRORS_RESET 6
// maximum allowed connecting time
#define MAX_CONN_TIME 5000 /* ms */
// maximum consecutive HTTP requests on a TCP connection (keep-alive)
#define MAX_HTTP_CONNS 99

/**************************************
* OBD
**************************************/
#define USE_OBD 0

/**************************************
* Accelerometer & Gyro
**************************************/
#define USE_MEMS 1
#define ACC_DATA_RATIO 172
#define GYRO_DATA_RATIO 256
#define COMPASS_DATA_RATIO 8

/**************************************
* Motion detection
**************************************/
#define START_MOTION_THRESHOLD 200000 /* for wakeup on movement */
#define RECALIBRATION_TIME 3000 /* ms */ 

/**************************************
* Other options
**************************************/
#define MIN_LOOP_TIME 500 /* ms */
#define COOLING_DOWN_TEMP 65 /* celsius degrees */

#endif // CONFIG_H_INCLUDED