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
extern const uint8_t tank200png_start[] asm("_binary_static_tank200_png_start");
extern const uint8_t tank200png_end[] asm("_binary_static_tank200_png_end");

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

void setup() {
    Serial.begin(115200);
    Serial.println("movduino.ino    created on 2018/6/13 by wuyuepku");
    wiFiMulti.addAP("CDMA", "1877309730");  // this is my WIFI hotspot, you can add your own here

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(request->beginResponse_P(200, "text/html", indexhtml_start, indexhtml_end - indexhtml_start));  // in case the file is too large (10k level)
    });

    server.on("/statc/tank200.png", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(request->beginResponse_P(200, "text/html", tank200png_start, tank200png_end - tank200png_start));  // the image file is too large (33k)
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
