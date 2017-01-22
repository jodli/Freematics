#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <FreematicsONE.h>
#include "wifi.h"

#if ENABLE_DATA_LOG
#include <SD.h>
#endif
#include "datalogger.h"

// logger states
#define STATE_SD_READY 0x1
#define STATE_OBD_READY 0x2
#define STATE_MEMS_READY 0x8
#define STATE_WIFI_READY 0x10
#define STATE_FILE_READY 0x20
#define STATE_CONNECTED 0x40

static uint8_t lastFileSize = 0;

class CTeleLogger : public COBDWIFI, public CDataLogger, public CMPU6050
{
public:
  CTeleLogger() : state(0)
  {
  }

  void setup()
  {
    delay(500);
    // this will init SPI communication
    begin();
    Serial.print("Firmware Ver. ");
    Serial.println(version);

  #if USE_MEMS
    // start I2C communication
    Wire.begin();
    Serial.print("MEMS... ");
    if (memsInit())
    {
      state |= STATE_MEMS_READY;
      Serial.println("OK");
    }
    else
    {
      Serial.println("NO");
    }
  #endif //USE_MEMS

  #if USE_ESP8266
    // initialize ESP8266 xBee module (if present)
    Serial.print("ESP8266... ");
    xbBegin(XBEE_BAUDRATE);
    if (initWifi())
    {
      state |= STATE_WIFI_READY;
      Serial.println("OK");
    }
    else
    {
      Serial.println(buffer);
      standby();
    }
  #endif //USE_ESP8266

  #if USE_OBD
    // initialize OBD communication
    Serial.print("OBD... ");
    if (init())
    {
      state |= STATE_OBD_READY;
      Serial.println("OK");
    }
    else
    {
      Serial.println("NO");
      reconnect();
    }
  #endif //USE_OBD

  #if ENABLE_DATA_LOG
    Serial.print("SD... ");
    uint16_t volsize = initSD();
    if (volsize)
    {
      Serial.print(volsize);
      Serial.println("MB");
    }
    else
    {
      Serial.println("NO");
    }
  #endif

    Serial.println();
    delay(1000);
  }

#if ENABLE_DATA_LOG
  uint16_t initSD()
  {
    state &= ~STATE_SD_READY;
    pinMode(SS, OUTPUT);
    Sd2Card card;
    uint32_t volumesize = 0;

    if (card.init(SPI_HALF_SPEED, SD_CS_PIN))
    {
      SdVolume volume;
      if (volume.init(card))
      {
        volumesize = volume.blocksPerCluster();
        volumesize >>= 1; // 512 bytes per block
        volumesize *= volume.clusterCount();
        volumesize /= 1000;
      }
    }

    if (SD.begin(SD_CS_PIN))
    {
      state |= STATE_SD_READY;
      return volumesize;
    }
    else
    {
      return 0;
    }
  }

  void flushData()
  {
    // flush SD data every 1KB
    byte dataSizeKB = dataSize >> 10;
    if (dataSizeKB != lastFileSize)
    {
      flushFile();
      lastFileSize = dataSizeKB;
    #if MAX_LOG_FILE_SIZE
      if (dataSize >= 1024L * MAX_LOG_FILE_SIZE)
      {
        closeFile();
        state &= ~STATE_FILE_READY;
      }
    #endif
    }
  }
#endif

  void loop()
  {
    // the main loop
    uint32_t start = millis();

    if (state & STATE_CONNECTED)
    {
    #if USE_MEMS
      // process MEMS data if available
      if (state & STATE_MEMS_READY)
      {
          processMEMS();
      }
    #endif //USE_MEMS

    #if USE_OBD
      // process OBD data if connected
      if (state & STATE_OBD_READY)
      {
        processOBD();
      }
    #endif //USE_OBD

      // read and log voltage at OBD2
      int v = getVoltage() * 100;
      logData(PID_BATTERY_VOLTAGE, v);
    }

    do
    {
      if (millis() > nextConnTime)
      {
      #if USE_ESP8266
        // process HTTP state machine
        processTcp();
      #endif

      #if USE_OBD
        // continously read speed for calculating trip distance
        if (state & STATE_OBD_READY)
        {
          readSpeed();
        }
      #endif //USE_OBD

        if (deviceTemp >= COOLING_DOWN_TEMP)
        {
          // device too hot, slow down communication a bit
          coolDown();
          break;
        }
      }
    } while (millis() - start < MIN_LOOP_TIME);
  }

private:

  void coolDown()
  {
    Serial.print("Cool down (");
    Serial.print(deviceTemp);
    Serial.println(" C)");
    delay(5000);
  }

#if USE_ESP8266
  bool connectWifi()
  {
    // attempt to join AP with pre-defined credential
    for (byte n = 0; ;n++)
    {
      delay(100);
      Serial.print("WIFI (SSID:");
      Serial.print(WIFI_SSID);
      Serial.print(")... ");
      if (setupWifi())
      {
        return true;
      }
      else
      {
        Serial.println("NO");
        if (n >= MAX_ERRORS_RECONNECT)
        {
          // not in range of wifi
          return false;
        }
      }
    }
  }

  void processTcp()
  {
    // state machine for HTTP communications
    nextConnTime = millis() + 200;

    switch (wifiState)
    {
    case WIFI_READY:
      // ready for doing next HTTP request
      if (cacheBytes > 0)
      {
        // send HTTP POST request with cached data as payload
        
        char* payload = "payload";
        int payloadBytes = 8;
        if (tcpSend(payload, payloadBytes))
        {
          // success
          Serial.print("Sending ");
          Serial.print(cacheBytes);
          Serial.println(" bytes");
          Serial.println(cache);
          purgeCache();
          wifiState = WIFI_SENDING;
        }
        else
        {
          Serial.println("Request error");
          wifiState = WIFI_HTTP_ERROR;
        }
      }
      break;
    case WIFI_DISCONNECTED:
      // attempt to connect again
      xbPurge();
      tcpConnect();
      wifiState = WIFI_CONNECTING;
      connCount = 0;
      break;
    case WIFI_CONNECTING:
      // in the progress of connecting
      if (tcpIsConnected())
      {
        wifiState = WIFI_READY;
        state |= STATE_CONNECTED;
        break;
      }
      break;
    case WIFI_SENDING:
      // in the progress of data sending
      if (tcpIsSent() || strstr(buffer, "+IPD"))
      {
        Serial.println("Sent");
        connErrors = 0;
        wifiState = WIFI_RECEIVING;
        break;
      }
      break;
    case WIFI_RECEIVING:
      // in the progress of data receiving
      if (tcpReceive())
      {
        // success
        connCount++;
        Serial.print("Success #");
        Serial.println(connCount);
        Serial.println(buffer);
        if (connCount >= MAX_HTTP_CONNS)
        {
          // re-establish TCP connection
          tcpDisconnect();
          wifiState = WIFI_DISCONNECTED;
        }
        else
        {
          wifiState = WIFI_READY;
        }
      }
      break;
    case WIFI_HTTP_ERROR:
      // oops, we got an error
      Serial.println(buffer);
      // check if there are too many connection errors
      if (connErrors >= MAX_ERRORS_RECONNECT || strstr_P(buffer, PSTR("link is not")))
      {
        // reset WIFI
        tcpDisconnect();
        if (connErrors >= MAX_ERRORS_RESET)
        {
          state &= ~STATE_CONNECTED;
          Serial.println("Reset WIFI...");
          disconnectWifi();
          resetWifi();
          delay(1000);
          initWifi();
          setupWifi();
          connErrors = 0;
        }
        wifiState = WIFI_DISCONNECTED;
      }
      else
      {
        wifiState = WIFI_READY;
      }
      break;
    }
  }
#endif

#if USE_OBD
  void processOBD()
  {
    int speed = readSpeed();
    if (speed == -1)
    {
      return;
    }

    logData(0x100 | PID_SPEED, speed);
    logData(PID_TRIP_DISTANCE, distance);

    // poll more PIDs
    byte pids[]= {0, PID_RPM, PID_ENGINE_LOAD, PID_THROTTLE};
    const byte pidTier2[] = {PID_INTAKE_TEMP, PID_COOLANT_TEMP};

    static byte index2 = 0;
    int values[sizeof(pids)] = {0};

    // read multiple OBD-II PIDs, tier2 PIDs are less frequently read
    pids[0] = pidTier2[index2 = (index2 + 1) % sizeof(pidTier2)];
    byte results = readPID(pids, sizeof(pids), values);
    if (results == sizeof(pids))
    {
      for (byte n = 0; n < sizeof(pids); n++)
      {
        logData(0x100 | pids[n], values[n]);
      }
    }
  }

  int readSpeed()
  {
    int value;
    if (readPID(PID_SPEED, value))
    {
      dataTime = millis();
      distance += (value + lastSpeed) * (dataTime - lastSpeedTime) / 3600 / 2;
      lastSpeedTime = dataTime;
      lastSpeed = value;
      return value;
    }
    else
    {
      return -1;
    }
  }

  void reconnect()
  {
    // try to re-connect to OBD
    if (!init())
    {
      delay(1000);
      if (!init())
      {
        standby();
      }
    }
  }
#endif //USE_OBD

#if USE_MEMS
  void processMEMS()
  {
    // log the loaded MEMS data
    if (accCount)
    {
      logData(PID_ACC,
      accSum[0] / accCount / ACC_DATA_RATIO,
      accSum[1] / accCount / ACC_DATA_RATIO,
      accSum[2] / accCount / ACC_DATA_RATIO);
    }
  }
#endif //USE_MEMS

  void standby()
  {
    state &= ~(STATE_OBD_READY | STATE_CONNECTED);
    Serial.print("Standby");
    // put OBD chips into low power mode
    enterLowPowerMode();
    // sleep for several seconds
    for (byte n = 0; n < 30; n++)
    {
      Serial.print('.');
      readMEMS();
      sleepms(250);
    }
    calibrateMEMS();

    // check if there are files to send

    // connect to wifi if in range
    if (connectWifi())
    {

    }

    // connect to tcp socket

    // send file

    // delete file on sd card

    // go to low power wifi
    if (state & STATE_WIFI_READY)
    {
      tcpDisconnect();
      disconnectWifi(); // disconnect from AP
      delay(500);
      resetWifi();
    }
    state &= ~(STATE_WIFI_READY);

    for (;;)
    {
      accSum[0] = 0;
      accSum[1] = 0;
      accSum[2] = 0;
      for (accCount = 0; accCount < 10; )
      {
        readMEMS();
        sleepms(30);
      }

      // calculate relative movement
      unsigned long motion = 0;
      for (byte i = 0; i < 3; i++)
      {
        long n = accSum[i] / accCount - accCal[i];
        motion += n * n;
      }

      // check movement
      if (motion > START_MOTION_THRESHOLD)
      {
        Serial.print("MOTION... ");
        Serial.println(motion);
        // try OBD reading
        leaveLowPowerMode();
        if (init())
        {
          // OBD is accessible
          break;
        }
        enterLowPowerMode();
        // calibrate MEMS again in case the device posture changed
        calibrateMEMS();
      }
    }

    // now we are able to get OBD data again
    // reset device
    void(* resetFunc) (void) = 0; //declare reset function at address 0
    resetFunc();
  }

  void calibrateMEMS()
  {
      // get accelerometer calibration reference data
      accCal[0] = accSum[0] / accCount;
      accCal[1] = accSum[1] / accCount;
      accCal[2] = accSum[2] / accCount;
  }

  void readMEMS()
  {
      // load accelerometer and temperature data
      int acc[3] = {0};
      int temp; // device temperature (in 0.1 celcius degree)
      memsRead(acc, 0, 0, &temp);

      if (accCount >= 250)
      {
        accSum[0] >>= 1;
        accSum[1] >>= 1;
        accSum[2] >>= 1;
        accCount >>= 1;
      }

      accSum[0] += acc[0];
      accSum[1] += acc[1];
      accSum[2] += acc[2];
      accCount++;

      deviceTemp = temp / 10;
  }

  byte state;
};

static CTeleLogger logger;

void setup()
{
  // initialize hardware serial (for USB and BLE)
  logger.initSender();
  // perform initializations
  logger.setup();
}

void loop()
{
  logger.loop();
}
