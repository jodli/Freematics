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

  void tcpDisconnect()
  {
    sendWifiCommand("AT+CIPCLOSE\r\n", 1000, "Unlink");
    Serial.println("DISCONNECTED");
  }

  void tcpConnect()
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

  bool tcpIsConnected()
  {
    // check if "Linked" is received
    byte ret = checkBuffer("Linked", MAX_CONN_TIME);
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

  bool tcpSend(const char* payload = 0, int payloadSize = 0)
  {
    if (!payload || payloadSize <= 0)
    {
      return true;
    }
    // start TCP send
    sprintf_P(buffer, PSTR("AT+CIPSEND=%u\r\n"), (unsigned int)payloadSize);
    if (sendWifiCommand(buffer, 1000, ">"))
    {
      // send payload
      xbWrite(payload);
      buffer[0] = 0;
      checkTimer = millis();
      return true;
    }
    connErrors++;
    return false;
  }

  bool tcpIsSent()
  {
    // check if "SEND OK" is received
    byte ret = checkBuffer("SEND OK", MAX_CONN_TIME);
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
  
  bool tcpReceive()
  {
      byte ret = checkBuffer("+IPD", MAX_CONN_TIME);
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

  byte checkBuffer(const char* expected, unsigned int timeout = 2000)
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