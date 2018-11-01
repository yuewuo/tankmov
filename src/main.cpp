#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPAsyncWebServer.h>
#include "AsyncJson.h"
#include "ArduinoJson.h"
#include "Ticker.h"

// since this function seems not work on https://github.com/SmartArduino/XPT/blob/master/GM25-370DataSheetSR04-T2.pdf
// which only generate two pulse, but not AB phase coding
#define ENABLE_HALL_SPEED_MEANSUREMENT

// set how to detect speed, using pin change interrupt or using 10kHz timer
#if true
    #define USING_INTERRUPT_DETECT_SPEED
#else
    #define USING_10KHzTIMR_DETECT_SPEED
    hw_timer_t * timer = NULL;
    #define PRESCALER 80  // prescale to 1MHz
    #define TIMER_CHANNEL 2  // since timer 0 and 1 will be used to generate PWM, use another one
    #define TIMER_FREQUENCY 1000
#endif

// timer to send speed data
#if true
    #define USING_1HzTIMR_SEND_SPEED
    // #define SHOW_READABLE_SPEED
    hw_timer_t * timer1Hz = NULL;
    #define PRESCALER1Hz 80
    #define TIMER1Hz_CHANNEL 3
    #define TIMER1Hz_FREQUENCY 2
#endif

// whether to print the 4 pin bits read in "motor_INT" function, this is just print data in interrupt function, which is unreliable implementation
// #define ENABLE_MOTOR_INT_DATA_OUTPUT

#define PWM_FREQUENCY 20000  // 20kHz
#define PWM_RESOLUTION 10  // bit, the resolution of PWM duty
const int MAX_PWM_DUTY = (1<<PWM_RESOLUTION) - 1;  // the maxmium duty

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
extern const uint8_t cssbootstrap_slider_start[] asm("_binary_static_css_bootstrap_slider_min_css_start");
extern const uint8_t cssbootstrap_slider_end[] asm("_binary_static_css_bootstrap_slider_min_css_end");
extern const uint8_t jsbootstrap_slider_start[] asm("_binary_static_js_bootstrap_slider_min_js_start");
extern const uint8_t jsbootstrap_slider_end[] asm("_binary_static_js_bootstrap_slider_min_js_end");
extern const uint8_t jsjquery_start[] asm("_binary_static_js_jquery_min_js_start");
extern const uint8_t jsjquery_end[] asm("_binary_static_js_jquery_min_js_end");

#define M1A 36//12  // magnetic coding A
#define M1B 39//14
#define M1P 27  // motor P
#define M1N 26

#define M2A 34//33
#define M2B 35//32
#define M2P 33//35
#define M2N 32//34


#ifdef ENABLE_HALL_SPEED_MEANSUREMENT
HardwareSerial RawSerial(1);  // serial to print raw data, without any log information
#define RawRx 16
#define RawTx 17

// comment to cancel print this info 
// #define PRINT_FIFO_INFO_INTERVAL 1

#define unlikely(x) __builtin_expect(!!(x), 0)

struct FIFO {  // this is designed to be thread safe, one thread for read and another for write
    volatile char *rptr;
    volatile char *wptr;
    char *head;
    char *tail;
    bool write(char data) {  // return 0 if success, otherwise error
        volatile char *w = wptr + 1;
        if (unlikely(w == tail)) w = head;
        if (unlikely(w == rptr)) return 1;  // fifo is full!
        *wptr = data;  // must save data first
        wptr = w;      // then change the pointer
        return 0;
    }
    inline bool has() {
        return rptr != wptr;
    }
    char read() {  // assume there has data, make sure you have check that!
        char a = *rptr;
        volatile char *r = rptr + 1;
        if (unlikely(r == tail)) r = head;
        rptr = r;  // then copy the pointer to it, do not change the rptr directly to avoid confliction with "write"
        return a;
    }
    int len() { // do not call this often, just for debug
        int length = tail - head;
        return (wptr-rptr + (length)) % length;
    }
};
struct FIFO fifo;  // no need to volatile
#define INTBUF_SIZE 4096
char intbuf[INTBUF_SIZE];
#endif

volatile uint8_t lastM1, lastM2;
volatile int32_t mov1, mov2;

#ifdef ENABLE_HALL_SPEED_MEANSUREMENT
void IRAM_ATTR motor_INT() {  // handling magnetic coding part
    uint8_t thisM1 = (((uint8_t)digitalRead(M1A)) << 1) + digitalRead(M1B);
    uint8_t thisM2 = (((uint8_t)digitalRead(M2A)) << 1) + digitalRead(M2B);
    uint8_t xor1 = thisM1 ^ lastM1, xor2 = thisM2 ^ lastM2;
    int8_t ch1 = 0, ch2 = 0;
    switch(xor1) {
        case 0x00:  // not changed
            break;
        case 0x03:  // WARNING: there must be bug or sampling rate is not enough
            // Serial.println("error 1");
            break;
        default:
            ch1 = ((thisM1 >> 1) ^ (lastM1 & 0x01)) ? -1 : 1;
            break;
    }
    switch(xor2) {
        case 0x00:  // not changed
            break;
        case 0x03:  // WARNING: there must be bug or sampling rate is not enough
            // Serial.println("error 2");
            break;
        default:
            ch2 = ((thisM2 >> 1) ^ (lastM2 & 0x01)) ? 1 : -1;
            break;
    }
    lastM1 = thisM1;
    lastM2 = thisM2;
    mov1 += ch1;
    mov2 += ch2;
    #ifdef ENABLE_MOTOR_INT_DATA_OUTPUT
        // Serial.print((char)(0x30 | (thisM1<<2) | thisM2));  // referred to ASCII table, this would be 0~9 : ; < = > ?, M1A-M1B-M2A-M2B order
        fifo.write((0x30 | (thisM1<<2) | thisM2));
    #endif
}
#endif

#ifdef USING_1HzTIMR_SEND_SPEED
int mv1=0, mv2=0, mva=0;
void IRAM_ATTR send1Hz_INT() {
    char msg[32];
    char* cp = msg;
    mv1 = mov1;
    mv2 = mov2;
    mov1 = 0;  // already volatile, so no conflict
    mov2 = 0;
    mva = (mv1 + mv2) / 2;
    sprintf(msg, "%d - %d %d\n", mva, mv1, mv2);
    #ifdef SHOW_READABLE_SPEED
        while (*cp) fifo.write(*(cp++));
    #else
        fifo.write(0xFF & mva);
    #endif
    Serial.print(msg);
}
#endif

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
volatile int pwm0 = 0;
volatile int pwm1 = 0;
void setNowState(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    // root["heap"] = ESP.getFreeHeap();
    // root["ssid"] = WiFi.SSID();
    root["mode"] = mode == PWM ? "PWM" : 
        (mode == PID ? "PID" :
        "UNKNOWN");
    root["millis"] = millis();
    root["pwm0"] = pwm0;
    root["pwm1"] = pwm1;
    root["maxpwm"] = MAX_PWM_DUTY;
    root.printTo(*response);
    request->send(response);
}

inline void _updatePWMSubFunc(int pwm, int ch1, int ch2) {
    if (pwm == 0) {
        ledcWrite(ch1, 0);
        ledcWrite(ch2, 0);
    } else if (pwm > 0) {
        ledcWrite(ch2, 0);
        ledcWrite(ch1, pwm);
    } else {
        ledcWrite(ch1, 0);
        ledcWrite(ch2, -pwm);
    }
}

void updatePWM() {
    _updatePWMSubFunc(pwm0, 0, 1);  // you can adjust the order of para 2 and 3 to reach the wanted direction
    _updatePWMSubFunc(pwm1, 2, 3);
    // Log(pwm[0]);
    // Log(pwm[1]);
}

#ifdef ENABLE_HALL_SPEED_MEANSUREMENT
void printFifoInfo() {
    Serial.print("Now FIFO Length: ");
    Serial.print(fifo.len());
    Serial.print('/');
    Serial.println(INTBUF_SIZE);
}
#endif

Ticker fifoticker;
char speedstr[32];

void setup() {

    #ifdef ENABLE_HALL_SPEED_MEANSUREMENT
    // initialize fifo
    fifo.head = intbuf;
    fifo.tail = intbuf + INTBUF_SIZE;
    fifo.rptr = fifo.head;
    fifo.wptr = fifo.head;
    #ifdef PRINT_FIFO_INFO_INTERVAL
        fifoticker.attach(PRINT_FIFO_INFO_INTERVAL, printFifoInfo);
    #endif
    #endif

    mode = PWM;  // initialize mode

    Serial.begin(115200);
    Serial.println("movduino.ino    created on 2018/6/13 by wuyuepku");
    wiFiMulti.addAP("CDMA", "1877309730");  // this is my WIFI hotspot, you can add your own here
    wiFiMulti.addAP("SOAR_513", "soar@ceca");

    #ifdef ENABLE_HALL_SPEED_MEANSUREMENT
    RawSerial.begin(115200, SERIAL_8N1, RawRx, RawTx);
    #endif

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
    server.on("/static/css/bootstrap-slider.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(request->beginResponse_P(200, "text/css", cssbootstrap_slider_start, cssbootstrap_slider_end - cssbootstrap_slider_start - 1));  // the image file is too large (33k)
    });
    server.on("/static/js/bootstrap-slider.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(request->beginResponse_P(200, "application/x-javascript", jsbootstrap_slider_start, jsbootstrap_slider_end - jsbootstrap_slider_start - 1));  // the image file is too large (33k)
    });
    server.on("/static/js/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(request->beginResponse_P(200, "application/x-javascript", jsjquery_start, jsjquery_end - jsjquery_start - 1));  // the image file is too large (33k)
    });

    server.on("/sync", HTTP_GET, [](AsyncWebServerRequest *request) {
        setNowState(request);
    });

    server.on("/speed", HTTP_GET, [](AsyncWebServerRequest *request) {
        sprintf(speedstr, "%d %d %d", mva, mv1, mv2);
        request->send(200, "text/plain", speedstr);
    });

    server.on("/setMode", HTTP_POST, [](AsyncWebServerRequest *request){
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

    server.on("/setPWM", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("pwm0", true) && request->hasParam("pwm1", true)) {
            if (mode == PWM) {
                int tpwm0 = atoi(request->getParam("pwm0", true)->value().c_str());
                int tpwm1 = atoi(request->getParam("pwm1", true)->value().c_str());
                if (tpwm0 > MAX_PWM_DUTY) tpwm0 = MAX_PWM_DUTY;
                if (tpwm0 < -MAX_PWM_DUTY) tpwm0 = -MAX_PWM_DUTY;
                if (tpwm1 > MAX_PWM_DUTY) tpwm1 = MAX_PWM_DUTY;
                if (tpwm1 < -MAX_PWM_DUTY) tpwm1 = -MAX_PWM_DUTY;  // limit the bound
                pwm0 = tpwm0;
                pwm1 = tpwm1;
                updatePWM();
            } else {
                Err("cannot set PWM when mode is not \"PWM\"");
            }
        } else {
            Err("setPWM called but no pwm0 and pwm1 provided");
        }
        setNowState(request);
    });

    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Not found");
    });

    #ifdef ENABLE_HALL_SPEED_MEANSUREMENT
    pinMode(M1A, INPUT);
    pinMode(M1B, INPUT);
    pinMode(M2A, INPUT);
    pinMode(M2B, INPUT);
    #endif
    pinMode(M1P, OUTPUT);
    pinMode(M1N, OUTPUT);
    pinMode(M2P, OUTPUT);
    pinMode(M2N, OUTPUT);

    #if defined USING_INTERRUPT_DETECT_SPEED && defined ENABLE_HALL_SPEED_MEANSUREMENT
        attachInterrupt(M1A, motor_INT, CHANGE);  // all link to one interrupt function
        attachInterrupt(M1B, motor_INT, CHANGE);
        attachInterrupt(M2A, motor_INT, CHANGE);
        attachInterrupt(M2B, motor_INT, CHANGE);
    #elif defined USING_10KHzTIMR_DETECT_SPEED && defined ENABLE_HALL_SPEED_MEANSUREMENT
        timer = timerBegin(TIMER_CHANNEL, PRESCALER, true);
        timerAttachInterrupt(timer, motor_INT, true);
        timerAlarmWrite(timer, 80000000 / PRESCALER / TIMER_FREQUENCY, true);  // true is to repeat
        timerAlarmEnable(timer);  // enable interrupt
    #endif

    #ifdef USING_1HzTIMR_SEND_SPEED
        timer1Hz = timerBegin(TIMER1Hz_CHANNEL, PRESCALER1Hz, true);
        timerAttachInterrupt(timer1Hz, send1Hz_INT, true);
        timerAlarmWrite(timer1Hz, 80000000 / PRESCALER1Hz / TIMER1Hz_FREQUENCY, true);  // true is to repeat
        timerAlarmEnable(timer1Hz);  // enable interrupt
    #endif

    // use LEDc to generate PWM
    ledcAttachPin(M1P, 0);
    ledcAttachPin(M1N, 1);
    ledcAttachPin(M2P, 2);
    ledcAttachPin(M2N, 3);
    // channels 0-15, resolution 1-16 bits, freq limits depend on resolution
    // ledcSetup(uint8_t channel, uint32_t freq, uint8_t resolution_bits);
    ledcSetup(0, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcSetup(1, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcSetup(2, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcSetup(3, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcWrite(0, 0);  // close all the PWM output
    ledcWrite(1, 0);
    ledcWrite(2, 0);
    ledcWrite(3, 0);

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
    
    #ifdef ENABLE_HALL_SPEED_MEANSUREMENT
    if (fifo.has()) {
        RawSerial.print(fifo.read());
        // Serial.print(fifo.read());
    }
    #endif
}
