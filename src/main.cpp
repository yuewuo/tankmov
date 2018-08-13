#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPAsyncWebServer.h>

WiFiMulti wiFiMulti;  // enables multiple WIFI connection try
AsyncWebServer server(80);  // listen for connection on 80 port
bool lastTimeIsConnected = false;  // to print IP information each time you connected

const char* PARAM_MESSAGE = "message";
extern const uint8_t indexhtml_start[] asm("_binary_static_index_html_start");
extern const uint8_t indexhtml_end[] asm("_binary_static_index_html_end");

void setup() {
    Serial.begin(115200);
    Serial.println("movduino.ino    created on 2018/6/13 by wuyuepku");
    wiFiMulti.addAP("CDMA", "1877309730");  // this is my WIFI hotspot, you can add your own here

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        // request->send(200, "text/plain", "Hello, world");
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/plain", indexhtml_start, indexhtml_end - indexhtml_start);  // in case the file is too large (10k level)
        request->send(response);
    });

    server.on("/post", HTTP_POST, [](AsyncWebServerRequest *request){
        String message;
        if (request->hasParam(PARAM_MESSAGE, true)) {
            message = request->getParam(PARAM_MESSAGE, true)->value();
        } else {
            message = "No message sent";
        }
        request->send(200, "text/plain", "Hello, POST: " + message);
    });

    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Not found");
    });

}

void loop() {
    if (wiFiMulti.run() != WL_CONNECTED) {
        Serial.println("try connect to WIFI...");
        lastTimeIsConnected = false;
        delay(500);
    } else {  // WIFI connection established
        if (!lastTimeIsConnected) {
            Serial.print("now IP is: ");
            Serial.println(WiFi.localIP());  // print IP to user
            lastTimeIsConnected = true;
            server.begin();
        }
    }
}
