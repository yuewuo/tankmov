#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>

WiFiMulti wiFiMulti;  // enables multiple WIFI connection try
// AsyncWebServer server(80);  // listen for connection on 80 port
bool lastTimeIsConnected = false;  // to print IP information each time you connected

void setup() {
    Serial.begin(115200);
    Serial.println("movduino.ino    created on 2018/6/13 by wuyuepku");
    wiFiMulti.addAP("CDMA", "1877309730");  // this is my WIFI hotspot, you can add your own here
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
        }
    }
}
