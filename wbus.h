#ifndef wbus_h
#define wbus_h

#include "wbus_const.h"
#include <SoftwareSerial.h>
  
class wbus
{
  private:
    SoftwareSerial  *_debugPort;
    unsigned long   _lastUpdate;
    uint8_t         _state;
    uint8_t         _temperature; 
    float           _voltage;
    boolean         _flame;
    char            _name[16];
    uint8_t         _faultCodes[16];
    uint8_t         _faultCount;
    unsigned long   _heatStartTimer;
    uint8_t         _heatInterval;

    uint8_t         _checkSum(uint8_t *cmd, uint8_t start, uint8_t len);
    uint8_t         _addFaultCode(uint8_t faultCode);
    int8_t          _sendCommand(uint8_t *cmdBuf, uint8_t cmdLen);
        
  public:  
    uint8_t         logLevel;
    uint8_t         cmdRefreshSec;
    uint8_t         cmdTimeout;
    uint8_t         readDelay;
    uint8_t         maxRetryCount;
    
    String          state();
    boolean         flame();
    uint8_t         temperature();
    float           voltage();
    char            *name();    
    char            *faultCode(uint8_t number);
    uint8_t         faultCount();
    uint16_t        heatCountdown();
    
    wbus(SoftwareSerial *port); 
    int8_t turnOn();
    int8_t turnOff();
    int8_t getDevInfo();
    int8_t getSensorsInfo();
    int8_t checkCmd(uint8_t state);
    int8_t readFaultCodes();
    int8_t clearFaultCodes();
    void loop();
};

#endif

