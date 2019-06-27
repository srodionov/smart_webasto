#include "Arduino.h"
#include "wbus.h"

wbus::wbus(SoftwareSerial *port)
{
  _debugPort = port;

  Serial.end();

  // initialize serial communication at 2400 bits per second, 8 bit data, even parity and 1 stop bit
  Serial.begin(2400, SERIAL_8E1);

  //250ms for timeouts
  Serial.setTimeout(250);

  logLevel = 0; // 0-nothing; 1-info; 2-warn; 3-debug;
  _faultCount = 0;
  _lastUpdate = 0;
  _name[0] = 0;
  _state = WBUS_CMD_OFF;
  _temperature = 0;
  _voltage = 0;
  _flame = false;
  _heatStartTimer = 0;
  _heatInterval = 30;

  cmdRefreshSec = 15;
  cmdTimeout = 3;       // max seconds for webasto command response
  readDelay = 30;       // milli sec
  maxRetryCount = 2;    //
}

String wbus::state() {
  if (_state == WBUS_CMD_ON)
    return "ON";
  else if (_state == WBUS_CMD_OFF)
    return "OFF";
}

boolean wbus::flame() {
  return _flame;
}

byte wbus::temperature() {
  return _temperature;
}

float wbus::voltage() {
  return _voltage;
}

char *wbus::name() {
  return _name;
}

uint16_t wbus::heatCountdown() {  
  if (_state == WBUS_CMD_ON)
    return _heatInterval*60 - (uint16_t)((millis() - _heatStartTimer)/1000);
  else
    return 0;
}

char *wbus::faultCode(uint8_t number) {
  char faultCodeHEX[2];

  sprintf(faultCodeHEX, "%02X", _faultCodes[number]);
  return faultCodeHEX;
}

int8_t wbus::clearFaultCodes() {
  uint8_t cmd[2];

  if (logLevel >= 1) _debugPort->println("wbus::clearFaultCodes");

  _faultCount = 0;
  cmd[0] = WBUS_CMD_ERR;
  cmd[1] = ERR_DEL;
  return _sendCommand(cmd, 2);
}

int8_t wbus::readFaultCodes() {
  uint8_t cmd[2];

  if (logLevel >= 1) _debugPort->println("wbus::readFaultCodes");

  _faultCount = 0;
  cmd[0] = WBUS_CMD_ERR;
  cmd[1] = ERR_LIST;
  return _sendCommand(cmd, 2);
}

uint8_t wbus::faultCount() {
  return _faultCount;
}

uint8_t wbus::_addFaultCode(uint8_t faultCode)
{
  if (logLevel >= 1) _debugPort->println("wbus::_addFaultCode");

  _faultCodes[_faultCount] = faultCode;
  _faultCount++;

  return _faultCount;
}

int8_t wbus::turnOn() {
  uint8_t cmd[2];
  int result = 0;

  if (logLevel >= 1) _debugPort->println("wbus::turnOn");

  if (_heatStartTimer == 0) _heatStartTimer = millis();
  cmd[0] = WBUS_CMD_ON;
  cmd[1] = _heatInterval;
  return _sendCommand(cmd, 2);
}

int8_t wbus::turnOff() {
  uint8_t cmd[1];

  if (logLevel >= 1) _debugPort->println("wbus::turnOff");

  _heatStartTimer = 0;
  _state = WBUS_CMD_OFF;

  cmd[0] = WBUS_CMD_OFF;
  return _sendCommand(cmd, 1);
}

int8_t wbus::getSensorsInfo() {
  uint8_t cmd[2];

  if (logLevel >= 1) _debugPort->println("wbus::getSensorsInfo");

  cmd[0] = WBUS_CMD_QUERY;
  cmd[1] = QUERY_SENSORS;
  return _sendCommand(cmd, 2);
}

int8_t wbus::getDevInfo() {
  uint8_t cmd[2];

  if (logLevel >= 1) _debugPort->println("wbus::getDevInfo");

  cmd[0] = WBUS_CMD_IDENT;
  cmd[1] = IDENT_DEV_NAME;
  return _sendCommand(cmd, 2);
}

int8_t wbus::checkCmd(uint8_t state) {
  uint8_t cmd[3];

  if (logLevel >= 1) _debugPort->println("wbus::checkCmd");

  cmd[0] = WBUS_CMD_CHK;
  cmd[1] = state;
  cmd[2] = 0;
  return _sendCommand(cmd, 3);

}

// XOR checksum
uint8_t wbus::_checkSum(uint8_t *cmd, uint8_t start, uint8_t len)
{
  uint8_t result = 0;

  for (int i = start; i < len; i++) {
    result = result ^ cmd[i];
  }
  return result;
}

/*
   Send a client W-Bus request and read answer from Heater.
*/
int8_t wbus::_sendCommand(uint8_t *cmdBuf, uint8_t cmdLen)
{
  uint8_t inHdr[5], dataLen, outData[32], iCount = 0, retryCount = 1, rxByte, chkSum;
  long lastPacket = 0;
  char buf[4];
  int8_t result = -1;

  if (logLevel >= 1) {
    _debugPort->println("wbus::_sendCommand");

    for (int i = 0; i < cmdLen; i++) {
      _debugPort->print("Param ");
      _debugPort->print(i);
      _debugPort->print(": ");
      _debugPort->println(cmdBuf[i], HEX);
    }
  }

  //address
  inHdr[0] = (WBUS_CADDR << 4) | WBUS_HADDR;
  //length
  inHdr[1] = cmdLen + 1;
  // command
  inHdr[2] = cmdBuf[0];

  if (cmdBuf[0] == WBUS_CMD_ON) {
  } else if (cmdBuf[0] == WBUS_CMD_OFF) {
  } else if (cmdBuf[0] == WBUS_CMD_IDENT) {
  } else if (cmdBuf[0] == WBUS_CMD_QUERY) {
  } else if (cmdBuf[0] == WBUS_CMD_ERR) {
  } else if (cmdBuf[0] == WBUS_CMD_CHK) {
  } else {
    if (logLevel >= 2) {_debugPort->println("Unknown or unsupported command");}
    return result;
  }

  iCount = 0;
  while (iCount < cmdLen) {
    inHdr[2 + iCount] = cmdBuf[iCount];
    iCount++;
  }
  inHdr[2 + iCount] = _checkSum(inHdr, 0, cmdLen + 2);

  // clean input buffer
  if (logLevel >= 2) { _debugPort->println("Clean input buffer");}

  while (Serial.available()) {
    rxByte = Serial.read();

    if (logLevel >= 3) {_debugPort->print("Rx Byte: "); _debugPort->println(rxByte, HEX);}
  }

  retryCount = 1;
  do {
    if (logLevel >= 2) {_debugPort->print("Sending Data Packet. Attempt: "); _debugPort->println(retryCount);}

    Serial.write(inHdr, cmdLen + 3);
    retryCount++;

    lastPacket = millis(); //start timeout
    //looking for echoed address
    do {
      delay(readDelay);
      rxByte = Serial.read();

      if (logLevel >= 3) {_debugPort->print("Rx Byte: "); _debugPort->println(rxByte, HEX);}
    } while ((inHdr[0] != rxByte) && ((millis() - lastPacket) < cmdTimeout*1000));

    // cheking whole echoed message
    iCount = 0;
    while (inHdr[iCount] == rxByte) {
      rxByte = Serial.read();
      iCount++;

      if (logLevel >= 3) {_debugPort->print("Rx Byte: "); _debugPort->println(rxByte, HEX);}
    }

    //check answer
    if (iCount == cmdLen + 3) {
      if (logLevel >= 2) {_debugPort->println("Webasto answer");}

      //looking address byte
      iCount = 0;
      while ((rxByte != ((WBUS_HADDR << 4) | WBUS_CADDR)) && iCount <= 100 ) {
        delay(readDelay);
        rxByte = Serial.read();
        iCount++;

        if (logLevel >= 3) {_debugPort->print("Rx Byte: ");_debugPort->println(rxByte, HEX);}
      }

      dataLen = 0;
      chkSum = rxByte;
      if (rxByte == ((WBUS_HADDR << 4) | WBUS_CADDR)) {
        if (logLevel >= 2) {_debugPort->println("Header found");}

        rxByte = Serial.read(); // len
        if (logLevel >= 3) {_debugPort->print("Rx Byte: ");_debugPort->println(rxByte, HEX);}

        dataLen = rxByte;
        chkSum = chkSum ^ rxByte;

        iCount = 0;
        while (iCount <= dataLen - 1) {
          rxByte = Serial.read();
          if (logLevel >= 3) {_debugPort->print("Rx Byte: "); _debugPort->println(rxByte, HEX);}

          if (iCount == 0) // command
            if ((cmdBuf[0] | 0x80) == rxByte) {
              if (logLevel >= 2) {_debugPort->println("Command is valid");}
            } else if (rxByte == 0x7F) { // error command received
              if (logLevel >= 2) {_debugPort->println("Error Command received");}
              cmdBuf[1] = cmdBuf[0];  // initial cmd
              cmdBuf[0] = 0x7F;   // set error command
            } else {
              if (logLevel >= 2) {_debugPort->println("Command is not valid");}
              result = -1;
              break;
            }

          if ((iCount == 1) && (cmdLen != 1)) { // param, if it was specified
            // for WBUS_CMD_CHK and WBUS_CMD_ON param could be changed, won't verify and store param as output Data
            if ((cmdBuf[0] == WBUS_CMD_ON) || (cmdBuf[0] == WBUS_CMD_CHK)) {
              outData[0] = rxByte;
              if (logLevel >= 2) {_debugPort->println("Param overrided");}
            } else {
              if (cmdBuf[1] == rxByte) {
                if (logLevel >= 2) {_debugPort->println("Param is valid");}
              }
              else {
                if (logLevel >= 2) {_debugPort->println("Param is not valid");}
                result = -2;
                break;
              }
            }
          }

          // data
          if ((iCount > 1) && (iCount < dataLen - 1)){
            outData[iCount - cmdLen] = rxByte;
          }
          
          if (iCount == dataLen - 1)
            if (chkSum == rxByte) { // checksum
              if (logLevel >= 2) {_debugPort->println("Checksum is correct");}
              result = 0;
            } else {
              if (logLevel >= 2) {_debugPort->println("Checksum is not correct");}
              result = -3;
              break;
            }

          chkSum = chkSum ^ rxByte;
          iCount++;
        }
      }
    }
  }
  while ((retryCount <= maxRetryCount) && (result != 0));

  // parse outData
  if (logLevel >= 2) {_debugPort->println("Parsing Data");}

  if (result == 0) {
    if (cmdBuf[0] == WBUS_CMD_ON) {
      _state = cmdBuf[0];
      if (logLevel >= 2) {_debugPort->print("Countdown min: "); _debugPort->println(outData[0]);}
    } else if (cmdBuf[0] == WBUS_CMD_OFF) {
      _state = cmdBuf[0];
    } else if (cmdBuf[0] == WBUS_CMD_QUERY) {
      if (cmdBuf[1] == QUERY_SENSORS) {
        _temperature = outData[0] - 50;

        sprintf(buf, "%02X%02X", outData[1], outData[2]);
        _voltage = (((unsigned int)strtol(buf, NULL, 16)) / 1000);

        if (outData[3] == 1) _flame = true; else _flame = false;
      }
    } else if (cmdBuf[0] == WBUS_CMD_IDENT) {
      if (cmdBuf[1] == IDENT_DEV_NAME) {
        iCount = 0;
        while (iCount < dataLen - (cmdLen + 1)) {
          _name[iCount] = outData[iCount];
          iCount++;
        }
      }
    } else if (cmdBuf[0] == WBUS_CMD_ERR) {
      if (cmdBuf[1] == ERR_LIST) {
        iCount = 1;
        while (iCount < dataLen - (cmdLen + 1)) {
          _addFaultCode(outData[iCount]);
          iCount = iCount + 3;
        }
      }
    } else if (cmdBuf[0] == 0x7F) {
      result = outData[0];
    } else if (cmdBuf[0] == WBUS_CMD_CHK) {
      result = outData[0]; //0-ok; 1-not
    }
  }
  return result;
}

void wbus::loop() {
  unsigned long _now = millis();
  uint8_t cmd[2];
 
  if (((_now - _lastUpdate) > cmdRefreshSec * 1000) || (_now < _lastUpdate) || (_lastUpdate == 0)) {
    if (logLevel >= 1) {_debugPort->println("wbus::loop()");}

    if (_state == WBUS_CMD_ON) {
      if (logLevel >= 2) { _debugPort->print("Seconds since start: "); _debugPort->println((int)((_now - _heatStartTimer)/1000)); }

      if ((int)((_now - _heatStartTimer)/1000) < _heatInterval*60) turnOn(); else turnOff();

      //  60 sec delay after on
      if ((int)((_now - _heatStartTimer)/1000) > 60)
        //checking state
        if (checkCmd(_state) != 0) turnOff();

      getSensorsInfo();
    }

    if (_name[0] == 0) getDevInfo();

    _lastUpdate = millis();
  }

}



