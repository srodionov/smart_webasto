//#include <ESPAsyncTCP.h>
//#include <ESPAsyncWebServer.h>
//#include <Hash.h>
//#include <FS.h>
//#include <AsyncJson.h>
//#include <ArduinoJson.h>

/*
void _onHome(AsyncWebServerRequest *request) {

    if (!webAuthenticate(request)) {
        return request->requestAuthentication(getSetting("hostname").c_str());
    }

    if (request->header("If-Modified-Since").equals(_last_modified)) {
        request->send(304);
    } else {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", webui_image, webui_image_len);

      response->addHeader("Content-Encoding", "gzip");
      response->addHeader("Last-Modified", _last_modified);
      response->addHeader("X-XSS-Protection", "1; mode=block");
      response->addHeader("X-Content-Type-Options", "nosniff");
      response->addHeader("X-Frame-Options", "deny");
      request->send(response);
    }
}
*/

// -----------------------------------------------------------------------------
/*
bool webAuthenticate(AsyncWebServerRequest *request) {
  String password = getSetting("adminPass", ADMIN_PASS);
  char httpPassword[password.length() + 1];
  password.toCharArray(httpPassword, password.length() + 1);
  return request->authenticate(WEB_USERNAME, httpPassword);
}

*/

// -----------------------------------------------------------------------------
/*
AsyncWebServer * webServer() {
    return _server;
}

void webLog(AsyncWebServerRequest *request) {
    DEBUG_MSG_P(PSTR("[WEBSERVER] Request: %s %s\n"), request->methodToString(), request->url().c_str());
}

void webSetup() {

    // Cache the Last-Modifier header value
    snprintf_P(_last_modified, sizeof(_last_modified), PSTR("%s %s GMT"), __DATE__, __TIME__);

    // Create server
    unsigned int port = 80;
    _server = new AsyncWebServer(port);

    // Rewrites
    _server->rewrite("/", "/index.html");

    // Serve home (basic authentication protection)
*/

//    _server->on("/index.html", HTTP_GET, _onHome);
//    _server->on("/reset", HTTP_GET, _onReset);
//    _server->on("/config", HTTP_GET, _onGetConfig);
//    _server->on("/config", HTTP_POST | HTTP_PUT, _onPostConfig, _onPostConfigData);
//    _server->on("/upgrade", HTTP_POST, _onUpgrade, _onUpgradeData);
//    _server->on("/discover", HTTP_GET, _onDiscover);
/*    
  server.on("/", handleRoot);
  server.on("/wifisave", handleWifiSave);
  server.onNotFound ( handleNotFound );


    // 404
    _server->onNotFound([](AsyncWebServerRequest *request){
        request->send(404);
    });

    // Run server
    _server->begin();

}
*/
