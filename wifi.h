uint32_t nextConnTime = 0;
uint16_t connCount = 0;
byte accCount = 0; // count of accelerometer readings
long accSum[3] = {0}; // sum of accelerometer data
int accCal[3] = {0}; // calibrated reference accelerometer data
byte deviceTemp = 0; // device temperature
int lastSpeed = 0;
uint32_t lastSpeedTime = 0;
uint32_t distance = 0;

typedef enum 
{
    WIFI_DISCONNECTED = 0,
    WIFI_READY,
    WIFI_CONNECTING,
    WIFI_SENDING,
    WIFI_RECEIVING,
    WIFI_HTTP_ERROR,
} WIFI_STATES;

typedef enum 
{
  HTTP_GET = 0,
  HTTP_POST,
} HTTP_METHOD;

class COBDWIFI : public COBDSPI 
{
public:
  COBDWIFI() : wifiState(WIFI_DISCONNECTED), connErrors(0) 
  { 
    buffer[0] = 0; 
  }

  void resetWifi()
  {
    sendWifiCommand("AT+RST\r\n");
  }

  void disconnectWifi()
  {
    sendWifiCommand("AT+CWQAP\r\n");
  }

  bool initWifi()
  {
    // set xBee module serial baudrate
    bool success = false;
    // test the module by issuing AT command and confirming response of "OK"
    for (byte n = 0; !(success = sendWifiCommand("ATE0\r\n")) && n < 10; n++) 
    {
      delay(100);
    }

    if (success) 
    {
      // send ESP8266 initialization commands
      sendWifiCommand("AT+CWMODE=1\r\n", 100);
      sendWifiCommand("AT+CIPMUX=0\r\n");
      return true;
    }
    else 
    {
      return false;
    }
  }

  bool setupWifi()
  {
    // generate and send AT command for joining AP
    sprintf_P(buffer, PSTR("AT+CWJAP=\"%s\",\"%s\"\r\n"), WIFI_SSID, WIFI_PASSWORD);
    byte ret = sendWifiCommand(buffer, 10000, "OK");
    if (ret == 1) 
    {
      // get IP address
      if (sendWifiCommand("AT+CIFSR\r\n", 1000, "OK") && !strstr(buffer, "0.0.0.0")) 
      {
        char *p = strchr(buffer, '\r');
        if (p) *p = 0;
        Serial.println(buffer);
        // output IP address
        return true;
      } 
      else 
      {
        // output error message
        Serial.println(buffer); 
      }
    } 
    else if (ret == 2) 
    {
      Serial.println("Failed to join AP"); 
    }
    return false;
  }

  void httpClose()
  {
    sendWifiCommand("AT+CIPCLOSE\r\n", 1000, "Unlink");
    Serial.println("TCP closed");
  }

  void httpConnect()
  {
    // start TCP connection
    sprintf_P(buffer, PSTR("AT+CIPSTART=\"TCP\",\"%s\",%d\r\n"), SERVER_URL, SERVER_PORT);
    xbWrite(buffer);
    Serial.print(buffer);
    // clear reception buffer
    buffer[0] = 0;
    bytesRecv = 0;
    // reset reception timeout timer
    checkTimer = millis();
  }

  bool httpIsConnected()
  {
      // check if "Linked" is received
      byte ret = checkbuffer("Linked", MAX_CONN_TIME);
      if (ret == 1) 
      {
        // success
        Serial.println("CONNECTED");
        connErrors = 0;
        return true;
      } 
      else if (ret == 2) 
      {
        // timeout
        wifiState = WIFI_HTTP_ERROR;
        connErrors++;
      }
      // not yet
      return false;
  }

  bool httpSend(HTTP_METHOD method, const char* path, bool keepAlive, const char* payload = 0, int payloadSize = 0)
  {
    char header[192];
    char *p = header;
    // generate HTTP header
    p += sprintf_P(p, PSTR("%s %s HTTP/1.1\r\nUser-Agent: ONE\r\nHost: %s\r\nConnection: %s\r\n"),
      method == HTTP_GET ? "GET" : "POST", path, SERVER_URL, keepAlive ? "keep-alive" : "close");
    if (method == HTTP_POST) 
    {
      p += sprintf_P(p, PSTR("Content-length: %u\r\n"), payloadSize);
    }
    p += sprintf_P(p, PSTR("\r\n"));
    // start TCP send
    sprintf_P(buffer, PSTR("AT+CIPSEND=%u\r\n"), (unsigned int)(p - header) + payloadSize);
    if (sendWifiCommand(buffer, 1000, ">")) 
    {
      // send HTTP header
      xbWrite(header);
      delay(50);
      // send POST payload if any
      if (payload) 
      {
        xbWrite(payload);
      }
      buffer[0] = 0;
      bytesRecv = 0;
      checkTimer = millis();
      return true;
    }
    connErrors++;
    return false;
  }

  bool httpIsSent()
  {
    // check if "SEND OK" is received
    byte ret = checkbuffer("SEND OK", MAX_CONN_TIME);
    if (ret == 1) 
    {
      // success
      connErrors = 0;
      return true;
    } 
    else if (ret == 2) 
    {
      // timeout
      wifiState = WIFI_HTTP_ERROR;
      connErrors++;
    }
    // not yet
    return false;
  }

  bool httpRead()
  {
      byte ret = checkbuffer("+IPD", MAX_CONN_TIME);
      if (ret == 1) 
      {
        // success
        connErrors = 0;
        return true;
      } 
      else if (ret == 2) 
      {
        // timeout
        wifiState = WIFI_HTTP_ERROR;
        connErrors++;
      }
      return false;
  }

  byte checkbuffer(const char* expected, unsigned int timeout = 2000)
  {
    // check if expected string is in reception buffer
    if (strstr(buffer, expected)) 
    {
      return 1;
    }
    // if not, receive a chunk of data from xBee module and look for expected string
    byte ret = xbReceive(buffer, sizeof(buffer), 0, expected) != 0;
    if (ret == 0) 
    {
      // timeout
      return (millis() - checkTimer < timeout) ? 0 : 2;
    } 
    else 
    {
      return ret;
    }
  }

  bool sendWifiCommand(const char* cmd, unsigned int timeout = 2000, const char* expected = "OK")
  {
    xbPurge();
    if (cmd) 
    {
      xbWrite(cmd);
      delay(50);
    }
    buffer[0] = 0;
    return xbReceive(buffer, sizeof(buffer), timeout, expected) != 0;
  }

  char buffer[192];
  byte bytesRecv;
  uint32_t checkTimer;
  byte wifiState;
  byte connErrors;
};