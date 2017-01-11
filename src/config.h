#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

/**************************************
* Data logging/streaming out
**************************************/

// enable(1)/disable(0) data logging (if SD card is present)
#define ENABLE_DATA_LOG 1
#define FILE_NAME_FORMAT "/DAT%05d.CSV"

// enable(1)/disable(0) data streaming
#define ENABLE_DATA_OUT 1

#define ENABLE_DATA_CACHE 1
#define CACHE_SIZE 256 /* bytes */

// followings define the format of data streaming, enable one of them only
// FORMAT_BIN is required by Freematics OBD iOS App
//#define STREAM_FORMAT FORMAT_BIN
// FORMAT_TEXT is for readable text output
#define STREAM_FORMAT FORMAT_TEXT

// serial baudrate for data out streaming
#define STREAM_BAUDRATE 115200

// maximum size per file, a new file will be created on reaching this size
#define MAX_LOG_FILE_SIZE 1024 /* KB */

/**************************************
* WIFI and networking settings
**************************************/

// enable(1)/disable(0) WIFI connection
#define USE_ESP8266 1

#define XBEE_BAUDRATE 9600

// change SSID and PASSWORD to your own
#define WIFI_SSID "WLAN FritzBox 7490"
#define WIFI_PASSWORD "xxx"

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

// enable(1)/disable(0) OBD connection
#define USE_OBD 0

// OBD-II UART baudrate
#define OBD_UART_BAUDRATE 115200L

// max allowed time for connecting OBD-II (0 for forever)
#define OBD_ATTEMPT_TIME 0 /* seconds */

/**************************************
* Accelerometer & Gyro
**************************************/

// enable(1)/disable(0) MEMS sensor
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
#define ID_STR "#FREEMATICS"

#endif // CONFIG_H_INCLUDED