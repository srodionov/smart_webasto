/*
#include "FS.h"
#include <ArduinoJson.h>

const size_t bufferSize = JSON_ARRAY_SIZE(8) + JSON_OBJECT_SIZE(3);
DynamicJsonBuffer jsonBuffer(bufferSize);

JsonObject& inJson = jsonBuffer.parseObject(payload);
if (!inJson.success()) result = -1;

String cmd = inJson["cmd"]; cmd.toUpperCase();
String param = inJson["param"]; param.toUpperCase();

jsonBuffer.clear();
JsonObject& outJson = jsonBuffer.createObject();
outJson["cmd"] = cmd;

outJson["result"] = result;
outJson.printTo((char*)buffer, outJson.measureLength() + 1);
*/


  

