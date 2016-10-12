/*
    Used library:
    NTP: https://github.com/PaulStoffregen/Time
*/

#include <ESP8266WiFi.h>
#include <string.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h> 
#include <Adafruit_ST7735.h>
#include <DHT.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// Khai báo các chân nối với DHT22, cảm biến chuyển động
// còi buzzer và nút gạt
#define DHTTYPE  DHT22
#define DHTPIN       D3
#define PIR_PIN D1
#define BUZZER_PIN D4
#define BTN_PIN D2

// Khai báo các chân điều khiển LCD
#define TFTCS       D0
#define TFTDC       D8
#define TFTRST      D4

Adafruit_ST7735   tft = Adafruit_ST7735(TFTCS, TFTDC, TFTRST);

// Cấu hình Wifi, sửa lại theo đúng mạng Wifi của bạn
const char* WIFI_SSID = "Sandiego";
const char* WIFI_PWD = "0988807067";

// Khai báo web server hỗ trợ cấu hình qua giao diện web
ESP8266WebServer server(80);
MDNSResponder mdns;
// Giao diện web
const char INDEX_HTML[] =
"<!DOCTYPE html>"
"<html lang='en' >"
"<head>"
"    <meta name = 'viewport' content = 'width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0'>"
"    <title>Ivy Sensor Alarm Config</title>"
"    <script type='text/javascript' src='https://cdnjs.cloudflare.com/ajax/libs/jquery/3.1.1/jquery.slim.min.js' integrity='sha256-/SIrNqv8h6QGKDuNoLGA4iret+kyesCkHGzVUUV0shc=' crossorigin='anonymous'></script>"
"    <!-- Latest compiled and minified CSS -->"
"    <link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css' integrity='sha384-BVYiiSIFeK1dGmJRAkycuHAHRg32OmUcww7on3RYdg4Va+PmSTsz/K68vbdEjh4u' crossorigin='anonymous'>"
"    <link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap-theme.min.css' integrity='sha384-rHyoN1iRsVXV4nD0JutlnGaslCJuC7uwjduW9SVrLvRYooPp2bWYgmgJQIXwl/Sp' crossorigin='anonymous'>"
"    <link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/bootstrap-datetimepicker/4.17.43/css/bootstrap-datetimepicker-standalone.min.css' integrity='sha256-+CTjwODD2mYru0lguUnWuJ0c6zYdassaASkIFVtD5mY=' crossorigin='anonymous' />"
"    <script src='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js' integrity='sha384-Tc5IQib027qvyjSMfHjOMaLkfuWVxZxUPnCJA7l2mCWNIpG9mGCD8wGNIcPD7Txa' crossorigin='anonymous'></script>"
"    <script type='text/javascript' src='https://cdnjs.cloudflare.com/ajax/libs/moment.js/2.15.1/moment.min.js' integrity='sha256-4PIvl58L9q7iwjT654TQJM+C/acEyoG738iL8B8nhXg=' crossorigin='anonymous'></script>"
"    <script type='text/javascript' src='https://cdnjs.cloudflare.com/ajax/libs/bootstrap-datetimepicker/4.17.43/js/bootstrap-datetimepicker.min.js' integrity='sha256-I8vGZkA2jL0PptxyJBvewDVqNXcgIhcgeqi+GD/aw34=' crossorigin='anonymous'></script>"
"    <script type='text/javascript'>"
"        $(function () {"
"            $('#timePicker').datetimepicker({"
"                format: 'LT'"
"            });"
"        });"
"    </script>"
"</head>"
"<body>"
"    <div class='container col-xs-4 col-xs-offset-4'>"
"        <div class='page-header'>"
"          <h1>Ivy Sensor Alarm Config</h1>"
"        </div>"
"        <form>"
"          <div class='form-group'>"
"            <label for='tempMin'>Temperature Min</label>"
"            <input type='text' class='form-control' name='tempMin' id='tempMin'>"
"          </div>"
"          <div class='form-group'>"
"            <label for='tempMax'>Temperature Max</label>"
"            <input type='text' class='form-control' name='tempMax' id='tempMax'>"
"          </div>"
"          <div class='form-group'>"
"            <label for='humidMin'>Humidity Min</label>"
"            <input type='text' class='form-control' name='humidMin' id='humidMin'>"
"          </div>"
"          <div class='form-group'>"
"            <label for='humidMax'>Humidity Max</label>"
"            <input type='text' class='form-control' name='humidMax' id='humidMax'>"
"          </div>"
"          <div class='form-group'>"
"            <div class='input-group date' id='timePicker'>"
"                <input type='text' class='form-control' name='time' />"
"                <span class='input-group-addon'>"
"                    <span class='glyphicon glyphicon-time'></span>"
"                </span>"
"            </div>"
"         </div>"
"           <div class='checkbox'>"
"             <label>"
"               <input type='checkbox' name='repeat' id='repeat'> Repeat"
"             </label>"
"           </div>"
"          <button type='submit' class='btn btn-primary'>Submit</button>"
"        </form>"
"    </div>"
"</body>"
"</html>";

// Tần suất cập nhật dữ liệu về MQTT server là 10 phút 1 lần
const int UPDATE_INTERVAL = 5000; //1 * 60 * 1000;
unsigned long lastSentToServer = 0; // lưu thời gian lần cuối gửi về server

// Cấu hình thư viện NTP để lấy giờ hiện tại từ Internet
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = 7;     // GMT của Việt Nam, GMT+7
time_t prevDisplay = 0; // when the digital clock was displayed

// Cấu hình cho giao thức MQTT
const char* clientId = "IvySensor11";
//const char* mqttServer = "broker.hivemq.com";
//const char* mqttServer = "192.168.1.110";
const char* mqttServer = "m20.cloudmqtt.com";
const int mqttPort = 15060;
// Username và password để kết nối đến MQTT server nếu server có
// bật chế độ xác thực trên MQTT server
// Nếu không dùng thì cứ để vậy
//const char* mqttUsername = "<MQTT_BROKER_USERNAME>";
//const char* mqttPassword = "<MQTT_BROKER_PASSWORD>";
const char* mqttUsername = "jbfptrgk";
const char* mqttPassword = "hRZXXsuJBg4R";

// Tên MQTT topic để gửi thông tin về nhiệt độ
const char* tempTopic = "/easytech.vn/LivingRoom/Temperature";
// Tên MQTT topic để gửi thông tin về độ ẩm
const char* humidityTopic = "/easytech.vn/LivingRoom/Humidity";
// Tên MQTT topic để gửi thông tin về ánh sáng
const char* brightnessTopic = "/easytech.vn/LivingRoom/Light";
// Tên MQTT topic để gửi thông tin về chuyển động khi phát hiện
const char* motionTopic = "/easytech.vn/LivingRoom/Motion";

boolean           blinkDot = false;
boolean           noData = true;
uint8_t           lastHour = 255;
uint8_t           lastMinute = 255;
uint8_t           lastHumidity = 255;
uint8_t           lastTemperature = 255;

// Các chuỗi lưu dữ liệu thu thập được để hiển thị trên màn hình Oled 
// và gửi về server qua giao thức MQTT
char temperatureMsg[10]; // nhiệt độ
char humidityMsg[10]; // độ ẩm (%)
char brightnessMsg[10]; // ánh sáng (%)
char motionMsg[5]; // chuyển động (1 hoặc 0)
float temperature, humidity, brightness;
boolean lastMotionStatus = false;

// Khởi tạo cảm biến độ ẩm nhiệt độ
DHT dht(DHTPIN, DHTTYPE, 15);

// Khởi tạo thư viện để kết nối wifi và MQTT server
WiFiClient wclient;
PubSubClient client(wclient);

// Khai báo các icon để hiển thị trên màn hình LCD

static int16_t PROGMEM icon_sleep_1[] = {
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0861, 0xEF7D, 0xEF7D, 0x0861, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0010 (16) pixels
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7BEF, 0xFFDF, 0xFFDF, 0x7BEF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0020 (32) pixels
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0861, 0xEF5D, 0xFFDF, 0xFFDF, 0xEF5D, 0x0861, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0030 (48) pixels
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7BCF, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0x7BCF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0040 (64) pixels
0x0000, 0x2965, 0x39E7, 0x7BCF, 0x8C51, 0xE73C, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0xE73C, 0x8C51, 0x7BCF, 0x39E7, 0x2965, 0x0000,   // 0x0050 (80) pixels
0xE71C, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xE71C,   // 0x0060 (96) pixels
0xE71C, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xE71C,   // 0x0070 (112) pixels
0x2965, 0xDEFB, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xDEFB, 0x2965,   // 0x0080 (128) pixels
0x0000, 0x2945, 0xDEDB, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xDEDB, 0x2945, 0x0000,   // 0x0090 (144) pixels
0x0000, 0x0000, 0x2945, 0xDEDB, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xDEDB, 0x2945, 0x0000, 0x0000,   // 0x00A0 (160) pixels
0x0000, 0x0000, 0x0000, 0xBDD7, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xBDD7, 0x0000, 0x0000, 0x0000,   // 0x00B0 (176) pixels
0x0000, 0x0000, 0x0000, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0x0000, 0x0000, 0x0000,   // 0x00C0 (192) pixels
0x0000, 0x0000, 0x2945, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0x2945, 0x0000, 0x0000,   // 0x00D0 (208) pixels
0x0000, 0x0000, 0x4228, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0x8C51, 0x8C51, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0x4228, 0x0000, 0x0000,   // 0x00E0 (224) pixels
0x0000, 0x0000, 0x5AEB, 0xDEDB, 0xDEDB, 0x94B2, 0x18C3, 0x0000, 0x0000, 0x18C3, 0x94B2, 0xDEDB, 0xDEDB, 0x5AEB, 0x0000, 0x0000,   // 0x00F0 (240) pixels
0x0000, 0x0000, 0x0861, 0x5AEB, 0x2945, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2945, 0x5AEB, 0x0861, 0x0000, 0x0000,   // 0x0100 (256) pixels
};

static int16_t PROGMEM icon_sleep_2[] = {
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0861, 0xEF7D, 0xEF7D, 0x0861, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0010 (16) pixels
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7BEF, 0xFFDF, 0xFFDF, 0x7BEF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0020 (32) pixels
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0861, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0x0861, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0030 (48) pixels
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7BCF, 0xF7BE, 0x7BCF, 0x7BCF, 0xF7BE, 0x7BCF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0040 (64) pixels
0x0000, 0x2965, 0x39E7, 0x7BCF, 0x8C51, 0xE73C, 0xE73C, 0x0861, 0x0861, 0xE73C, 0xE73C, 0x8C51, 0x7BCF, 0x39E7, 0x2965, 0x0000,   // 0x0050 (80) pixels
0xE71C, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0x7BCF, 0x0000, 0x0000, 0x7BCF, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xE71C,   // 0x0060 (96) pixels
0xE71C, 0xEF7D, 0xC618, 0x5ACB, 0x39C7, 0x0861, 0x0000, 0x0000, 0x0000, 0x0000, 0x0861, 0x39C7, 0x5ACB, 0xC618, 0xEF7D, 0xE71C,   // 0x0070 (112) pixels
0x2965, 0xDEFB, 0xEF7D, 0x5ACB, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x5ACB, 0xEF7D, 0xDEFB, 0x2965,   // 0x0080 (128) pixels
0x0000, 0x2945, 0xDEDB, 0xEF5D, 0x52AA, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x52AA, 0xEF5D, 0xDEDB, 0x2945, 0x0000,   // 0x0090 (144) pixels
0x0000, 0x0000, 0x2945, 0xDEDB, 0xE73C, 0x18E3, 0x0000, 0x0000, 0x0000, 0x0000, 0x18E3, 0xE73C, 0xDEDB, 0x2945, 0x0000, 0x0000,   // 0x00A0 (160) pixels
0x0000, 0x0000, 0x0000, 0xBDD7, 0xE73C, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xE73C, 0xBDD7, 0x0000, 0x0000, 0x0000,   // 0x00B0 (176) pixels
0x0000, 0x0000, 0x0000, 0xE71C, 0xAD55, 0x0000, 0x0000, 0x738E, 0x738E, 0x0000, 0x0000, 0xAD55, 0xE71C, 0x0000, 0x0000, 0x0000,   // 0x00C0 (192) pixels
0x0000, 0x0000, 0x2945, 0xE71C, 0x8C71, 0x528A, 0xC638, 0xE71C, 0xE71C, 0xC638, 0x528A, 0x8C71, 0xE71C, 0x2945, 0x0000, 0x0000,   // 0x00D0 (208) pixels
0x0000, 0x0000, 0x4228, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0x8C51, 0x8C51, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0x4228, 0x0000, 0x0000,   // 0x00E0 (224) pixels
0x0000, 0x0000, 0x5AEB, 0xDEDB, 0xDEDB, 0x94B2, 0x18C3, 0x0000, 0x0000, 0x18C3, 0x94B2, 0xDEDB, 0xDEDB, 0x5AEB, 0x0000, 0x0000,   // 0x00F0 (240) pixels
0x0000, 0x0000, 0x0861, 0x5AEB, 0x2945, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2945, 0x5AEB, 0x0861, 0x0000, 0x0000,   // 0x0100 (256) pixels
};

static int16_t PROGMEM icon_humidity[] = {
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2965, 0xDEFB, 0x2965, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0010 (16) pixels
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2965, 0xEF5D, 0xFFDF, 0xEF5D, 0x2965, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0020 (32) pixels
0x0000, 0x0000, 0x0000, 0x0000, 0x2965, 0xEF5D, 0xFFDF, 0xFFDF, 0xFFDF, 0xEF5D, 0x2965, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0030 (48) pixels
0x0000, 0x0000, 0x0000, 0x0861, 0xE73C, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0xE73C, 0x0861, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0040 (64) pixels
0x0000, 0x0000, 0x0861, 0xC638, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0xC638, 0x0861, 0x0000, 0x0000, 0x0000,   // 0x0050 (80) pixels
0x0000, 0x0000, 0xC638, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xC638, 0x0000, 0x0000, 0x0000,   // 0x0060 (96) pixels
0x0000, 0x73AE, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0x73AE, 0x0000, 0x0000,   // 0x0070 (112) pixels
0x0000, 0xCE79, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xCE79, 0x0000, 0x0000,   // 0x0080 (128) pixels
0x0000, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0x0000, 0x0000,   // 0x0090 (144) pixels
0x0000, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0x0000, 0x0000,   // 0x00A0 (160) pixels
0x0000, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0x0000, 0x0000,   // 0x00B0 (176) pixels
0x0000, 0xC638, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xC638, 0x0000, 0x0000,   // 0x00C0 (192) pixels
0x0000, 0x738E, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0x738E, 0x0000, 0x0000,   // 0x00D0 (208) pixels
0x0000, 0x0000, 0xB5B6, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xB5B6, 0x0000, 0x0000, 0x0000,   // 0x00E0 (224) pixels
0x0000, 0x0000, 0x0861, 0xB596, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xB596, 0x0861, 0x0000, 0x0000, 0x0000,   // 0x00F0 (240) pixels
0x0000, 0x0000, 0x0000, 0x0000, 0x6B6D, 0xBDF7, 0xDEDB, 0xDEDB, 0xDEDB, 0xBDF7, 0x6B6D, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0100 (256) pixels
};

static int16_t PROGMEM icon_temperature[] = {
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8C71, 0x8C71, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0010 (16) pixels
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0020 (32) pixels
0x0000, 0x0000, 0x8C71, 0x9CD3, 0x18E3, 0x0000, 0x2965, 0xBDD7, 0xBDD7, 0x18E3, 0x0000, 0x18E3, 0x9CD3, 0x8C71, 0x0000, 0x0000,   // 0x0030 (48) pixels
0x0000, 0x0000, 0x9CD3, 0xF7BE, 0xDEDB, 0x0861, 0x5AEB, 0x7BCF, 0x7BCF, 0x5AEB, 0x0861, 0xDEDB, 0xF7BE, 0x9CD3, 0x0000, 0x0000,   // 0x0040 (64) pixels
0x0000, 0x0000, 0x18E3, 0xD6BA, 0x39E7, 0xD6BA, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0xBA, 0x39E7, 0xD6BA, 0x18E3, 0x0000, 0x0000,   // 0x0050 (80) pixels
0x0000, 0x0000, 0x0000, 0x0861, 0xD6BA, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xD6BA, 0x0861, 0x0000, 0x0000, 0x0000,   // 0x0060 (96) pixels
0x0000, 0x0000, 0x18E3, 0x5ACB, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0x5ACB, 0x2965, 0x0000, 0x0000,   // 0x0070 (112) pixels
0x8430, 0xEF7D, 0xB596, 0x8430, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0x73AE, 0xB596, 0xEF7D, 0x8430,   // 0x0080 (128) pixels
0x8430, 0xEF5D, 0xB596, 0x8430, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0x73AE, 0xB596, 0xEF5D, 0x8430,   // 0x0090 (144) pixels
0x0000, 0x0000, 0x18E3, 0x52AA, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0x52AA, 0x18E3, 0x0000, 0x0000,   // 0x00A0 (160) pixels
0x0000, 0x0000, 0x0000, 0x0861, 0xCE59, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xCE59, 0x0861, 0x0000, 0x0000, 0x0000,   // 0x00B0 (176) pixels
0x0000, 0x0000, 0x18E3, 0xC638, 0x39C7, 0xC638, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xC638, 0x39C7, 0xC638, 0x18E3, 0x0000, 0x0000,   // 0x00C0 (192) pixels
0x0000, 0x0000, 0x8C71, 0xE71C, 0xC638, 0x0861, 0x52AA, 0x738E, 0x738E, 0x52AA, 0x0861, 0xC638, 0xE71C, 0x8C71, 0x0000, 0x0000,   // 0x00D0 (208) pixels
0x0000, 0x0000, 0x7BEF, 0x8C51, 0x18C3, 0x0000, 0x18C3, 0xA534, 0xA534, 0x18C3, 0x0000, 0x18C3, 0x8C51, 0x7BEF, 0x0000, 0x0000,   // 0x00E0 (224) pixels
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xDEFB, 0xDEFB, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x00F0 (240) pixels
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7BCF, 0x7BCF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0100 (256) pixels
};

static int16_t PROGMEM icon_alarm[] = {
  0x8410, 0x8410, 0x8410, 0x83ef, 0x83ef, 0x7451, 0x6cb2, 0x6cd3, 0x6cb2, 0x7471, 0x83ef, 0x83ef, 0x8410, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x83ef, 0x7c10, 0x7451, 0x8b8e, 0xb249, 0xc1a6, 0xb249, 0x8b8e, 0x7451, 0x7c10, 0x83ef, 0x8410, 0x8410, 
  0x8410, 0x83ef, 0x7c10, 0x7451, 0xb249, 0xf820, 0xf800, 0xf800, 0xf800, 0xf800, 0xb228, 0x7451, 0x7c10, 0x83ef, 0x8410, 
  0x8410, 0x83ef, 0x7451, 0xaaaa, 0xf800, 0xf800, 0xf924, 0xff7d, 0xfa08, 0xf800, 0xf800, 0xb269, 0x7451, 0x83ef, 0x8410, 
  0x8410, 0x7c10, 0x7c10, 0xe882, 0xf800, 0xf000, 0xf904, 0xffff, 0xf9e7, 0xf000, 0xf800, 0xf041, 0x83ef, 0x7c10, 0x8410, 
  0x8410, 0x7c30, 0x934d, 0xf800, 0xf800, 0xf000, 0xf841, 0xfe9a, 0xf904, 0xf000, 0xf820, 0xf800, 0x9b2c, 0x7430, 0x8410, 
  0x8410, 0x7430, 0x9b2c, 0xf800, 0xf820, 0xf820, 0xf800, 0xfb8e, 0xf861, 0xf800, 0xf820, 0xf800, 0x9b0c, 0x7451, 0x8410, 
  0x8410, 0x7c10, 0x83cf, 0xf041, 0xf800, 0xf800, 0xf882, 0xfcb2, 0xf8e3, 0xf800, 0xf800, 0xf020, 0x8bae, 0x7c10, 0x8410, 
  0x8410, 0x83ef, 0x7451, 0xba08, 0xf800, 0xf800, 0xf8e3, 0xfedb, 0xf945, 0xf800, 0xf800, 0xc1c7, 0x7451, 0x7bef, 0x8410, 
  0x8410, 0x83ef, 0x7c10, 0x7c30, 0xc986, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xd165, 0x7c30, 0x7c10, 0x83ef, 0x8410, 
  0x8410, 0x8410, 0x83ef, 0x7c10, 0x7451, 0xa2eb, 0xc986, 0xd904, 0xc965, 0xa2cb, 0x7451, 0x7c10, 0x83ef, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x8410, 0x83ef, 0x7c10, 0x7451, 0x7451, 0x7451, 0x7451, 0x7451, 0x7c10, 0x83ef, 0x8410, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x83ef, 0x7c10, 0x7c10, 0x7c10, 0x83ef, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410
};

static int16_t PROGMEM icon_clock[] = {
0x0000, 0x0000, 0x0000, 0x0861, 0x8C71, 0xDEFB, 0xFFFF, 0xFFFF, 0xFFFF, 0xDEFB, 0x8C71, 0x0861, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0010 (16) pixels
0x0000, 0x0000, 0x4A69, 0xEF5D, 0xFFDF, 0xFFDF, 0xFFDF, 0xBDF7, 0xFFDF, 0xFFDF, 0xFFDF, 0xEF5D, 0x4A69, 0x0000, 0x0000, 0x0000,   // 0x0020 (32) pixels
0x0000, 0x4A69, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF, 0x4A69, 0x0000, 0x0000,   // 0x0030 (48) pixels
0x0861, 0xE73C, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0x5AEB, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0xE73C, 0x0861, 0x0000,   // 0x0040 (64) pixels
0x8C51, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0x0000, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0xF7BE, 0x8C51, 0x0000,   // 0x0050 (80) pixels
0xD69A, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0x0000, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xD69A, 0x0000,   // 0x0060 (96) pixels
0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0x0000, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0x0000,   // 0x0070 (112) pixels
0xEF7D, 0xB596, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0x2945, 0xDEFB, 0xEF7D, 0xEF7D, 0xEF7D, 0xEF7D, 0xB596, 0xEF7D, 0x0000,   // 0x0080 (128) pixels
0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xDEDB, 0x52AA, 0xDEDB, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0x0000,   // 0x0090 (144) pixels
0xCE59, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xDEDB, 0x52AA, 0xDEDB, 0xE73C, 0xE73C, 0xE73C, 0xCE59, 0x0000,   // 0x00A0 (160) pixels
0x8410, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xE73C, 0xD6BA, 0x52AA, 0xE73C, 0xE73C, 0xE73C, 0x8410, 0x0000,   // 0x00B0 (176) pixels
0x0861, 0xD6BA, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xD6BA, 0x0861, 0x0000,   // 0x00C0 (192) pixels
0x0000, 0x4228, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0xDEFB, 0x4228, 0x0000, 0x0000,   // 0x00D0 (208) pixels
0x0000, 0x0000, 0x4228, 0xCE79, 0xDEFB, 0xDEFB, 0xDEFB, 0xA534, 0xDEFB, 0xDEFB, 0xDEFB, 0xCE79, 0x4228, 0x0000, 0x0000, 0x0000,   // 0x00E0 (224) pixels
0x0000, 0x0000, 0x0000, 0x0861, 0x7BCF, 0xBDF7, 0xDEDB, 0xDEDB, 0xDEDB, 0xBDF7, 0x7BCF, 0x0861, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x00F0 (240) pixels
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0100 (256) pixels
};

void bitmap(int x, int y, int16_t *bitmap, int16_t w, int16_t h) {

  int16_t col, row;
  int16_t offset = 0;
  tft.setAddrWindow(x, y, x+w-1, y+h-1);

  for (row = 0; row < h; row++) 
  {
    for (col = 0; col < w; col++) {
        tft.pushColor(pgm_read_word(bitmap+offset));
        offset++;
      }
    }
}

void setup(void)
{
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(PIR_PIN, INPUT);
  pinMode(BTN_PIN, INPUT);

  // Khởi tạo thư viện cho cảm biến DHT
  Wire.begin();
  dht.begin();

  // Khởi tạo thư viện điều khiển màn hình LCD
  //tft.initR(INITR_REDTAB);
  tft.initR(INITR_BLACKTAB);   // ST7735S chip, black tab
  tft.setRotation(1);
  clearScreen();
  tft.setTextSize(1);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.setCursor(10, 50);
  
  tft.print("Dang ket noi Wifi");

  // Do cảm biến chuyển động PIR bị nhiễu khi sử dụng gần Esp8266 nên ta cần
  // phải giảm mức tín hiệu của esp8266 để tránh nhiễu cho PIR
  //WiFi.setPhyMode(WIFI_PHY_MODE_11G); 
  WiFi.setOutputPower(7);

  // Kết nối Wifi và chờ đến khi kết nối thành công
  WiFi.begin(WIFI_SSID, WIFI_PWD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    tft.print('.');
  }

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());

  tft.print("done!");
  tft.setCursor(10, 80);
  tft.print("IP: ");
  
  tft.print(WiFi.localIP());
  delay(1000);

  // Kết nối tới MQTT server
  client.setServer(mqttServer, mqttPort);
  // Đăng ký hàm sẽ xử lý khi có dữ liệu từ MQTT server gửi về
  client.setCallback(onMQTTMessageReceived);

  clearScreen();
  tft.setCursor(10, 50);  
  tft.print("Dang cap nhat gio tu Internet...");
 
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

  tft.print("done!");
  delay(1000);

  drawIdleScreen();

  launchWeb(0);
}

int stringToInt(String string) {
  int value = 0;
  for (int i=0; i < string.length() ; i++)
  {
    char c = string[i];
    if (c >= '0' && c <= '9')
    { 
      value = (10 * value) + (c - '0');
    }
  }
  return value;
}

uint32_t stringToTime(String string) {
  uint32_t pctime = 0;
  for (int i=0; i < string.length() ; i++) {
    char c = string[i];
    if (c >= '0' && c <= '9') { 
      pctime = (10 * pctime) + (c - '0');
    }
  }
  return pctime;
}

bool printTwoDigitsByZero(int digits, uint8_t *last) {
  if (*last == digits)
  {
    return false;
  }

  if (digits < 10)
  {
    tft.print("0");
  }

  tft.print(digits);

  *last = digits;

  return true;
}

bool printTwoDigitsBySpace(int digits, uint8_t *last) {
  if (*last == digits)
  {
    return false;
  }

  if (digits < 10)
  {
    tft.print(" ");
  }

  tft.print(digits);

  *last = digits;

  return true;
}

bool printFourDigitsByZero(int digits, uint16_t *last) {
  if (*last == digits)
  {
    return false;
  }

  if (digits < 10) 
  {
    tft.print("   ");
  } else
  if (digits < 100) 
  {
    tft.print("  ");
  } else
  if (digits < 1000) 
  {
    tft.print(" ");
  }

  tft.print(digits);

  *last = digits;

  return true;
}

void clearScreen() {
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);

  lastHour = 255;
  lastMinute = 255;
  lastHumidity = 255;
  lastTemperature = 255;
}

void drawIdleScreen() {
  clearScreen();

  bitmap(19, 87, icon_humidity, 16, 16);
  bitmap(18, 107, icon_temperature, 16, 16);
}

void updateSecDot(unsigned long drawTime, int fontsize, int x, int y, uint16_t color) {
  if (drawTime < 1000) {
    unsigned long diffTime = 1000 - drawTime;
    delay(diffTime);
  }

  tft.setTextSize(fontsize);
  tft.setCursor(x, y);
  tft.setTextColor(color, ST7735_BLACK);

  if (blinkDot) {
     tft.print(':');
  } else {
    tft.print(' ');
  }

  tft.setTextSize(1);
  tft.setCursor(0, 120);

  /*
  if (blinkDot) {
    bitmap(115, 95, icon_alarm, 16, 16);
  } else {
    tft.fillRect(115, 95, 16, 16, ST7735_BLACK);
  }
  */

  blinkDot = !blinkDot;
}

void updateIdleScreen() {
  unsigned long startDrawTime = millis();

  /*********************************
   * Display current date and time
   *********************************/

   if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();

      // current time
      tft.setTextSize(4);
      tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
    
      tft.setCursor(20, 5);
      printTwoDigitsByZero(hour(), &lastHour);
      tft.setCursor(68, 5);
      tft.print(":");
      tft.setCursor(92, 5);
      printTwoDigitsByZero(minute(), &lastMinute);
    
      int sx = 21;
      int sy = 40;
      int sec = second()*2;
      int black = (60-sec)*2;
      tft.fillRect(sx, sy, sec, 3, ST7735_YELLOW);
      tft.fillRect(sx+sec, sy, black, 3, ST7735_BLACK);

      // current date
      tft.setTextSize(2);
      tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
      tft.setCursor(19, 50);
      tft.print(day());
      tft.print("/");
      tft.print(month());
      tft.print("/");
      tft.print(year());

      // current week day
      tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
      tft.setCursor(65, 70);
      tft.setTextSize(1);
    
      switch (weekday())
      {
        case 1: tft.print("      SUN"); break;
        case 2: tft.print("      MON"); break;
        case 3: tft.print("      TUE"); break;
        case 4: tft.print("      WED"); break;
        case 5: tft.print("      THU"); break;
        case 6: tft.print("      FRI"); break;
        case 7: tft.print("      SAT"); break;
      }
    }
  }  

  /*******************************************
   * Display current temperature and humidity
   ******************************************/
   
  humidity = dht.readHumidity();
  dtostrf(humidity, 4, 1, humidityMsg);
  Serial.println(humidityMsg);
  temperature = dht.readTemperature();
  dtostrf(temperature, 4, 1, temperatureMsg);
  Serial.println(temperatureMsg);
  brightness = analogRead(A0)*100/1024;
  dtostrf(brightness, 4, 1, brightnessMsg);

  if (!isnan(temperature) && !isnan(humidity))
  {

    tft.setTextSize(2);

    tft.setCursor(40, 87);
    tft.setTextColor(ST7735_CYAN, ST7735_BLACK);

    tft.print(humidityMsg);
    tft.print("%");

    tft.setCursor(40, 108);
    tft.setTextColor(ST7735_MAGENTA, ST7735_BLACK);

    tft.print(temperatureMsg);
    tft.print("'C");
  }

  updateSecDot(millis() - (startDrawTime), 4, 68, 5, ST7735_CYAN);
}

void loop() {
  digitalWrite(BUZZER_PIN, HIGH); delay(300);
  digitalWrite(BUZZER_PIN, LOW); delay(300);
  digitalWrite(BUZZER_PIN, HIGH); delay(300);
  digitalWrite(BUZZER_PIN, LOW); delay(300);
  delay(1000);
  //return;
  
  if (!client.connected()) {
    if (client.connect(clientId, mqttUsername, mqttPassword)) {
      Serial.println("MQTT server connected");
      client.loop();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
    }
  } else {
    client.loop();  
  }

  boolean currentMotionStatus = digitalRead(PIR_PIN);
  
  if (currentMotionStatus != lastMotionStatus) {
    lastMotionStatus = currentMotionStatus;
    if (currentMotionStatus) {
      Serial.println("Motion detected");
    }
    client.publish(motionTopic, "1");
  }

  updateIdleScreen();
  server.handleClient();
  unsigned long currentMillis = millis();

  if (lastSentToServer == 0 || currentMillis - lastSentToServer >= UPDATE_INTERVAL) {
    // Update sensor information to MQTT server
    client.publish(humidityTopic, humidityMsg);
    delay(200);
    client.publish(tempTopic, temperatureMsg);
    delay(200);
    client.publish(brightnessTopic, brightnessMsg);
    Serial.print("Do sang (%) hien tai: "); Serial.println(brightness);

    // save the last time sent data to server
    lastSentToServer = currentMillis;
  }

  Serial.println(digitalRead(BTN_PIN));
}

void onMQTTMessageReceived(char* topic, byte* payload, unsigned int length) {
  Serial.print("Nhan duoc du lieu tu MQTT server [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime() {
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

/*---------------------- Web server setup ----------------------*/

void launchWeb(int webtype) {
  if (mdns.begin("ivysensor1", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/ledon", handleLEDon);
  server.on("/ledoff", handleLEDoff);
  server.onNotFound(handleNotFound);

  /*
  if (webtype == 0) {
    server.on("/", []() {
      IPAddress ip = WiFi.localIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      server.send(200, "application/json", "{\"IP\":\"" + ipStr + "\"}");
    });
    server.on("/cleareeprom", []() {
      String content;
      content = "<!DOCTYPE HTML>\r\n<html>";
      content += "<p>Clearing the EEPROM</p></html>";
      server.send(200, "text/html", content);
      Serial.println("clearing eeprom");
      //for (int i = 0; i < 96; ++i) { EEPROM.write(i, 0); }
      //EEPROM.commit();
    });
  }
  */
  server.begin();
  Serial.println("Server started"); 
  Serial.print("Connect to http://ivysensor1.local or http://");
  Serial.println(WiFi.localIP());
}

void handleRoot()
{
  if (server.hasArg("tempMin")) {
    handleSubmit();
  }
  else {
    server.send(200, "text/html", INDEX_HTML);
  }
}

void returnFail(String msg)
{
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(500, "text/plain", msg + "\r\n");
}

void handleSubmit()
{
  String temp;

  if (!server.hasArg("tempMin")) return returnFail("BAD ARGS");
  Serial.print("Temp Min: "); Serial.println(server.arg("tempMin"));
  Serial.print("Temp Max: "); Serial.println(server.arg("tempMax"));
  Serial.print("Humidity Min: "); Serial.println(server.arg("humidMin"));
  Serial.print("Humidity Max: "); Serial.println(server.arg("humidMax"));
  Serial.print("Time: "); Serial.println(server.arg("time"));
  Serial.print("Repeat: "); Serial.println(server.arg("repeat"));
  //char buffer[2000];
  //sprintf(buffer, INDEX_HTML, "checked", "");
  //server.send(200, "text/html", buffer);
  //server.send(200, "text/html", INDEX_HTML);
  server.send(200, "text/html", INDEX_HTML);
  return;
  /*
  LEDvalue = server.arg("LED");
  if (LEDvalue == "1") {
    Serial.println("Led on");
    char buffer[700];
    sprintf(buffer, INDEX_HTML, "checked", "");
    server.send(200, "text/html", buffer);
  }
  else if (LEDvalue == "0") {
    Serial.println("Led off");
    char buffer[700];
    sprintf(buffer, INDEX_HTML, "", "checked");
    server.send(200, "text/html", buffer);
  }
  else {
    returnFail("Bad LED value");
  }
  */
}

void returnOK()
{
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "OK\r\n");
}

/*
 * Imperative to turn the LED on using a non-browser http client.
 * For example, using wget.
 * $ wget http://esp8266webform/ledon
 */
void handleLEDon()
{
  Serial.println("Led on");
  returnOK();
}

/*
 * Imperative to turn the LED off using a non-browser http client.
 * For example, using wget.
 * $ wget http://esp8266webform/ledoff
 */
void handleLEDoff()
{
  Serial.println("Led off");
  returnOK();
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

