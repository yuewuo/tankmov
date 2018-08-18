#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPAsyncWebServer.h>
#include "AsyncJson.h"
#include "ArduinoJson.h"

WiFiMulti wiFiMulti;  // enables multiple WIFI connection try
AsyncWebServer server(80);  // listen for connection on 80 port
bool lastTimeIsConnected = false;  // to print IP information each time you connected

extern const uint8_t indexhtml_start[] asm("_binary_static_index_html_start");
extern const uint8_t indexhtml_end[] asm("_binary_static_index_html_end");
extern const uint8_t tank200png_start[] asm("_binary_static_tank200_png_start");
extern const uint8_t tank200png_end[] asm("_binary_static_tank200_png_end");
extern const uint8_t cssbootstrap_start[] asm("_binary_static_css_bootstrap_min_css_start");
extern const uint8_t cssbootstrap_end[] asm("_binary_static_css_bootstrap_min_css_end");
extern const uint8_t jsbootstrap_start[] asm("_binary_static_js_bootstrap_min_js_start");
extern const uint8_t jsbootstrap_end[] asm("_binary_static_js_bootstrap_min_js_end");
extern const uint8_t jsjquery_start[] asm("_binary_static_js_jquery_min_js_start");
extern const uint8_t jsjquery_end[] asm("_binary_static_js_jquery_min_js_end");

#define M1A 12  // magnetic coding A
#define M1B 14
#define M1P 27  // motor P
#define M1N 26

#define M2A 33
#define M2B 32
#define M2P 35
#define M2N 34

volatile uint8_t lastM1, lastM2;
volatile int32_t mov1, mov2;

void motor_INT() {  // handling magnetic coding part
    uint8_t thisM1 = (((uint8_t)digitalRead(M1A)) << 1) + digitalRead(M1B);
    uint8_t thisM2 = (((uint8_t)digitalRead(M2A)) << 1) + digitalRead(M2B);
    uint8_t xor1 = thisM1 ^ lastM1, xor2 = thisM2 ^ lastM2;
    lastM1 = thisM1;
    lastM2 = thisM2;
    uint8_t ch1 = 0, ch2 = 0;
    switch(xor1) {
        case 0x00:  // not changed
            break;
        case 0x01:
            ch1 = (((thisM1 >> 1) & thisM1) & 0x01) ? -1 : 1;
            break;
        case 0x02:
            ch1 = (((thisM1 >> 1) & thisM1) & 0x01) ? 1 : -1;
            break;
        default:  // WARNING: there must be bug or sampling rate is not enough
            Serial.println("error 1");
            break;
    }
    switch(xor2) {
        case 0x00:  // not changed
            break;
        case 0x01:
            ch2 = (((thisM2 >> 1) & thisM2) & 0x01) ? -1 : 1;
            break;
        case 0x02:
            ch2 = (((thisM2 >> 1) & thisM2) & 0x01) ? 1 : -1;
            break;
        default:  // WARNING: there must be bug or sampling rate is not enough
            Serial.println("error 2");
            break;
    }
    mov1 += ch1;
    mov2 += ch2;
}

#define Err(xxx) do {\
    Serial.print("error: ");\
    Serial.println(xxx);\
} while (0)
#define Log(xxx) do {\
    Serial.print("log: ");\
    Serial.println(xxx);\
} while (0)

// static machine
enum {PWM, PID} mode;
void setNowState(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root["heap"] = ESP.getFreeHeap();
    root["ssid"] = WiFi.SSID();
    root["mode"] = mode == PWM ? "PWM" : 
        (mode == PID ? "PID" :
        "UNKNOWN");
    root["millis"] = millis();
    root.printTo(*response);
    request->send(response);
}

void setup() {
    Serial.begin(115200);
    Serial.println("movduino.ino    created on 2018/6/13 by wuyuepku");
    wiFiMulti.addAP("CDMA", "1877309730");  // this is my WIFI hotspot, you can add your own here

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(request->beginResponse_P(200, "text/html", indexhtml_start, indexhtml_end - indexhtml_start));  // in case the file is too large (10k level)
    });

    // add static resources
    server.on("/static/tank200.png", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(request->beginResponse_P(200, "image/png", tank200png_start, tank200png_end - tank200png_start - 1));  // the image file is too large (33k)
    });
    server.on("/static/css/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(request->beginResponse_P(200, "text/css", cssbootstrap_start, cssbootstrap_end - cssbootstrap_start - 1));  // the image file is too large (33k)
    });
    server.on("/static/js/bootstrap.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(request->beginResponse_P(200, "application/x-javascript", jsbootstrap_start, jsbootstrap_end - jsbootstrap_start - 1));  // the image file is too large (33k)
    });
    server.on("/static/js/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(request->beginResponse_P(200, "application/x-javascript", jsjquery_start, jsjquery_end - jsjquery_start - 1));  // the image file is too large (33k)
    });

    server.on("/sync", HTTP_GET, [](AsyncWebServerRequest *request) {
        setNowState(request);
    });

    server.on("/setMode", HTTP_POST, [](AsyncWebServerRequest *request){
        String message;
        if (request->hasParam("mode", true)) {
            const String& newMode = request->getParam("mode", true)->value();
            if (newMode == "PWM") mode = PWM;
            else if (newMode == "PID") mode = PID;
            else Err("unrecognized mode");
        } else {
            Err("setMode called but no mode provided");
        }
        setNowState(request);
    });

    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Not found");
    });

    pinMode(M1A, INPUT);
    pinMode(M1B, INPUT);
    pinMode(M1P, OUTPUT);
    pinMode(M1N, OUTPUT);
    pinMode(M2A, INPUT);
    pinMode(M2B, INPUT);
    pinMode(M2P, OUTPUT);
    pinMode(M2N, OUTPUT);

    attachInterrupt(M1A, motor_INT, CHANGE);  // all link to one interrupt function
    attachInterrupt(M1B, motor_INT, CHANGE);
    attachInterrupt(M2A, motor_INT, CHANGE);
    attachInterrupt(M2B, motor_INT, CHANGE);

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
