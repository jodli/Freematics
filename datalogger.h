/*************************************************************************
* Telematics Data Logger Class
* Distributed under BSD license
* Written by Stanley Huang <support@freematics.com.au>
* Visit http://freematics.com for more information
*************************************************************************/

// additional custom PID for data logger
#define PID_DATA_SIZE 0x80

#if ENABLE_DATA_LOG
static File sdfile;
#endif

typedef struct {
    uint8_t pid;
    char name[3];
} PID_NAME;

class CDataLogger 
{
public:
  CDataLogger() : m_lastDataTime(0), dataTime(0), dataSize(0)
  {
  #if ENABLE_DATA_CACHE
    cacheBytes = 0;
  #endif //ENABLE_DATA_CACHE
  }

  void initSender()
  {
  #if ENABLE_DATA_OUT
    SerialRF.begin(STREAM_BAUDRATE);
    SerialRF.println(ID_STR);
  #endif //ENABLE_DATA_OUT
  }

  byte genTimestamp(char* buf, bool absolute)
  {
    byte n = 0;
    if (absolute || dataTime >= m_lastDataTime + 60000)
    {
      // absolute timestamp
      n = sprintf(buf, "#%lu", dataTime);
    }
    else
    {
      // relative timestamp
      uint16_t elapsed = (unsigned int)(dataTime - m_lastDataTime);
      n = sprintf(buf, "%u", elapsed);
    }
    buf[n++] = ',';
    return n;
  }

  void record(const char* buf, byte len)
  {
  #if ENABLE_DATA_LOG
    char tmp[12];
    byte n = genTimestamp(tmp, dataSize == 0);
    dataSize += sdfile.write(tmp, n);
    dataSize += sdfile.write(buf, len);
    sdfile.println();
    dataSize += 3;
  #endif //ENABLE_DATA_LOG
    m_lastDataTime = dataTime;
  }

  void dispatch(const char* buf, byte len)
  {
  #if ENABLE_DATA_CACHE
    // reserve some space for timestamp, ending white space and zero terminator
    int l = cacheBytes + len + 12 - CACHE_SIZE;
    if (l >= 0)
    {
      // cache full
    #if CACHE_SHIFT
      // discard the oldest data
      for (l = CACHE_SIZE / 2; cache[l] && cache[l] != ' '; l++);
      if (cache[l])
      {
        cacheBytes -= l;
        memcpy(cache, cache + l + 1, cacheBytes);
      }
      else
      {
        cacheBytes = 0;
      }
    #else //CACHE_SHIFT
      return;
    #endif //CACHE_SHIFT
    }

    // add new data at the end
    byte n = genTimestamp(cache + cacheBytes, cacheBytes == 0);
    if (n == 0)
    {
      // same timestamp
      cache[cacheBytes - 1] = ';';
    }
    else
    {
      cacheBytes += n;
      cache[cacheBytes++] = ',';
    }

    if (cacheBytes + len < CACHE_SIZE - 1)
    {
      memcpy(cache + cacheBytes, buf, len);
      cacheBytes += len;
      cache[cacheBytes++] = ' ';
    }
    cache[cacheBytes] = 0;
  #else //ENABLE_DATA_CACHE
    //char tmp[12];
    //byte n = genTimestamp(tmp, dataTime >= m_lastDataTime + 100);
    //SerialRF.write(tmp, n);
  #endif //ENABLE_DATA_CACHE

  #if ENABLE_DATA_OUT
    SerialRF.write(buf, len);
    SerialRF.println();
  #endif //ENABLE_DATA_OUT
  }

  void logData(const char* buf, byte len)
  {
    dispatch(buf, len);
    record(buf, len);
  }

  void logData(uint16_t pid)
  {
    char buf[8];
    byte len = translatePIDName(pid, buf);
    dispatch(buf, len);
    record(buf, len);
  }

  void logData(uint16_t pid, int value)
  {
    char buf[16];
    byte n = translatePIDName(pid, buf);
    byte len = sprintf_P(buf + n, PSTR("%d"), value) + n;
    dispatch(buf, len);
    record(buf, len);
  }

  void logData(uint16_t pid, int32_t value)
  {
    char buf[20];
    byte n = translatePIDName(pid, buf);
    byte len = sprintf_P(buf + n, PSTR("%ld"), value) + n;
    dispatch(buf, len);
    record(buf, len);
  }

  void logData(uint16_t pid, uint32_t value)
  {
    char buf[20];
    byte n = translatePIDName(pid, buf);
    byte len = sprintf_P(buf + n, PSTR("%lu"), value) + n;
    dispatch(buf, len);
    record(buf, len);
  }

  void logData(uint16_t pid, int value1, int value2, int value3)
  {
    char buf[24];
    byte n = translatePIDName(pid, buf);
    byte len = sprintf_P(buf + n, PSTR("%d/%d/%d"), value1, value2, value3) + n;
    dispatch(buf, len);
    record(buf, len);
  }

  void logCoordinate(uint16_t pid, int32_t value)
  {
    char buf[24];
    byte len = translatePIDName(pid, buf);
    len += sprintf_P(buf + len, PSTR("%d.%06lu"), (int)(value / 1000000), abs(value) % 1000000);
    dispatch(buf, len);
    record(buf, len);
  }

#if ENABLE_DATA_LOG
  uint16_t openFile(uint16_t logFlags = 0, uint32_t dateTime = 0)
  {
    uint16_t fileIndex;
    char filename[24] = "/FRMATICS";

    dataSize = 0;
    if (SD.exists(filename)) {
        for (fileIndex = 1; fileIndex; fileIndex++) {
            sprintf_P(filename + 9, PSTR(FILE_NAME_FORMAT), fileIndex);
            if (!SD.exists(filename)) {
                break;
            }
        }
        if (fileIndex == 0)
            return 0;
    } else {
        SD.mkdir(filename);
        fileIndex = 1;
        sprintf_P(filename + 9, PSTR(FILE_NAME_FORMAT), 1);
    }

    sdfile = SD.open(filename, FILE_WRITE);
    if (!sdfile) {
        return 0;
    }
    m_lastDataTime = dateTime;
    return fileIndex;
  }

  void closeFile()
  {
      sdfile.close();
  }

  void flushFile()
  {
      sdfile.flush();
  }
#endif //ENABLE_DATA_LOG

  uint32_t dataTime;
  uint32_t dataSize;

#if ENABLE_DATA_CACHE
  void purgeCache()
  {
    cacheBytes = 0;
  }

  char cache[CACHE_SIZE];
  int cacheBytes;
#endif //ENABLE_DATA_CACHE

private:
  byte translatePIDName(uint16_t pid, char* text)
  {
      return sprintf_P(text, PSTR("%X="), pid);
  }

  uint32_t m_lastDataTime;
};