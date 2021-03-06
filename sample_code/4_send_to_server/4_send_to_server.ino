#include <ESP8266WiFi.h>
#include <string.h>
#include <Wire.h>
#include <DHT.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <Adafruit_GFX.h> 
#include <Adafruit_ST7735.h>
#include <PubSubClient.h>

// Khai báo các chân nối với cảm biến
#define DHTTYPE  DHT22
#define DHTPIN   D3
#define PIR_PIN D1

// Khai báo các chân điều khiển LCD SPI
#define TFTCS       D0
#define TFTDC       D8
#define TFTRST      D4

const int UPDATE_INTERVAL = 5000; // Tần suất cập nhật dữ liệu
unsigned long lastSensorUpdateTime = 0; // Lưu thời gian lần cuối cùng cập nhật dữ liệu sensor

float temperature, humidity, lightStatus;
char temperatureMsg[10]; // nhiệt độ
char humidityMsg[10]; // độ ẩm (%)
char lightStatusMsg[10]; // ánh sáng (%)
char motionMsg[5]; // chuyển động (1 hoặc 0)

boolean lastMotionStatus = false;

// Khởi tạo cảm biến độ ẩm nhiệt độ
DHT dht(DHTPIN, DHTTYPE, 15);

// Cấu hình Wifi, sửa lại theo đúng mạng Wifi của bạn
const char* WIFI_SSID = "your_wifi_network";
const char* WIFI_PWD = "your_wifi_password";

// Cấu hình thư viện NTP để lấy giờ hiện tại từ Internet
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = 7; // GMT của Việt Nam, GMT+7
time_t lastClockUpdateTime = 0; // Lưu thời gian lần cuối cập nhật đồng hồ

// Khai báo thiết lập màn hình LCD
Adafruit_ST7735 tft = Adafruit_ST7735(TFTCS, TFTDC, TFTRST);

// Khai báo các icon để hiển thị trên màn hình LCD
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
  0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x83ef, 0x7c10, 0x83ef, 0x83cf, 0x83ef, 0x7c10, 0x7c10, 0x83ef, 0x83cf, 0x83ef, 0x7c10, 0x83ef, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x8410, 0x8410, 0x83ef, 0x7c10, 0x83ef, 0x83ef, 0x7c10, 0x7c30, 0x7c10, 0x83cf, 0x83cf, 0x7c10, 0x7c30, 0x7c30, 0x83ef, 0x83ef, 0x7c10, 0x83ef, 0x8410, 0x8410, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x8410, 0x83ef, 0x7c10, 0x83ef, 0x83ef, 0x7c30, 0x934d, 0xc1c7, 0xd8e3, 0xe8a2, 0xe8a2, 0xe0e3, 0xc1a6, 0x9b2c, 0x7c30, 0x7c10, 0x83ef, 0x7c10, 0x83ef, 0x8410, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x8410, 0x7c10, 0x83ef, 0x7c10, 0x7bef, 0xc1c7, 0xf820, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xc986, 0x83cf, 0x7c10, 0x83ef, 0x7c10, 0x8410, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x83ef, 0x83ef, 0x7c10, 0x7bef, 0xd145, 0xf800, 0xf020, 0xf041, 0xf020, 0xfb8e, 0xfc51, 0xf082, 0xf020, 0xf020, 0xf800, 0xd904, 0x83cf, 0x7c10, 0x83ef, 0x83ef, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x7c10, 0x83ef, 0x7430, 0xc1e7, 0xf800, 0xf041, 0xf820, 0xf000, 0xf8e3, 0xffff, 0xffff, 0xfa28, 0xf000, 0xf841, 0xf041, 0xf800, 0xc9a6, 0x7c30, 0x83ef, 0x7c10, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x83ef, 0x7c10, 0x936d, 0xf020, 0xf820, 0xf820, 0xf820, 0xf820, 0xf861, 0xfefb, 0xffdf, 0xf986, 0xf800, 0xf820, 0xf820, 0xf020, 0xf800, 0x9b2c, 0x7c30, 0x83ef, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x83ef, 0x7430, 0xb228, 0xf800, 0xf041, 0xf820, 0xf820, 0xf841, 0xf800, 0xfe59, 0xffbe, 0xf882, 0xf800, 0xf820, 0xf820, 0xf041, 0xf800, 0xc1e7, 0x7430, 0x83ef, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x83ef, 0x7c30, 0xc965, 0xf800, 0xf020, 0xf820, 0xf820, 0xf841, 0xf800, 0xfd34, 0xfedb, 0xf800, 0xf820, 0xf820, 0xf820, 0xf020, 0xf800, 0xd124, 0x7c10, 0x83ef, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x83ef, 0x7c30, 0xd145, 0xf800, 0xf020, 0xf820, 0xf820, 0xf841, 0xf000, 0xfc71, 0xfdf7, 0xf000, 0xf841, 0xf820, 0xf820, 0xf020, 0xf800, 0xd904, 0x7c10, 0x83ef, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x83ef, 0x7c30, 0xc9a6, 0xf800, 0xf020, 0xf820, 0xf820, 0xf841, 0xf800, 0xfa49, 0xfb4d, 0xf800, 0xf841, 0xf820, 0xf820, 0xf020, 0xf800, 0xd145, 0x7c30, 0x83ef, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x83ef, 0x7430, 0xaa8a, 0xf800, 0xf041, 0xf820, 0xf820, 0xf820, 0xf800, 0xfaeb, 0xfb4d, 0xf820, 0xf820, 0xf820, 0xf820, 0xf041, 0xf800, 0xba28, 0x7430, 0x83ef, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x83ef, 0x7c10, 0x83cf, 0xe861, 0xf820, 0xf820, 0xf820, 0xf800, 0xf882, 0xffff, 0xffff, 0xf904, 0xf000, 0xf820, 0xf820, 0xf820, 0xf041, 0x8b8e, 0x7c10, 0x83ef, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x7c10, 0x83ef, 0x7430, 0xaa8a, 0xf800, 0xf020, 0xf020, 0xf820, 0xf820, 0xfc71, 0xfcd3, 0xf841, 0xf820, 0xf820, 0xf041, 0xf800, 0xb249, 0x7430, 0x83ef, 0x7c10, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x83ef, 0x7c10, 0x83ef, 0x7c30, 0xba08, 0xf800, 0xf800, 0xf041, 0xf020, 0xf000, 0xf000, 0xf020, 0xf041, 0xf800, 0xf800, 0xc1c7, 0x7c10, 0x83ef, 0x7bef, 0x83ef, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x8410, 0x83ef, 0x83ef, 0x83ef, 0x7c30, 0xaaaa, 0xe0a2, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xe882, 0xaa69, 0x7c10, 0x83ef, 0x83ef, 0x7c10, 0x8410, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x8410, 0x8410, 0x7c10, 0x83ef, 0x83ef, 0x7c30, 0x83ef, 0xa2cb, 0xc1c7, 0xd145, 0xd145, 0xc1c7, 0xaaaa, 0x83cf, 0x7c30, 0x83ef, 0x83ef, 0x7c10, 0x83ef, 0x8410, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x83ef, 0x7c10, 0x83ef, 0x83ef, 0x7c30, 0x7c30, 0x7c30, 0x7c30, 0x7c30, 0x7c30, 0x7c10, 0x83ef, 0x7c10, 0x7bef, 0x83ef, 0x8410, 0x8410, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x83ef, 0x7c10, 0x83ef, 0x83ef, 0x83ef, 0x83ef, 0x83ef, 0x83ef, 0x83ef, 0x83ef, 0x7c10, 0x83ef, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 
  0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410
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

static int16_t PROGMEM icon_motion[] = {
  0x8410, 0x8410, 0x840f, 0x7bf0, 0x840f, 0x83ef, 0x7bb1, 0x7bf0, 0x84ab, 0x8566, 0x85a4, 0x85a4, 0x85a4, 0x85a4, 0x8566, 0x84ab, 0x7bf0, 0x7bb1, 0x83ef, 0x840f, 0x7bf0, 0x840f, 0x8410, 0x8410, 
  0x8410, 0x840f, 0x7bf0, 0x840f, 0x7bf0, 0x7bf0, 0x848b, 0x8566, 0x85a5, 0x85a4, 0x85a4, 0x85c4, 0x85c4, 0x85a4, 0x85a4, 0x85a5, 0x8566, 0x848b, 0x7bf0, 0x7bf0, 0x840f, 0x7bf0, 0x840f, 0x8410, 
  0x840f, 0x7bf0, 0x840f, 0x7bf0, 0x7c2e, 0x8566, 0x85c3, 0x85c4, 0x85c4, 0x85c4, 0x85c4, 0x85c4, 0x85c4, 0x85c4, 0x85c4, 0x85c4, 0x85c4, 0x85c3, 0x8566, 0x7c2e, 0x7bf0, 0x840f, 0x7bf0, 0x840f, 
  0x7bf0, 0x840f, 0x7bd0, 0x846c, 0x85a5, 0x85c4, 0x85a5, 0x85a4, 0x85a4, 0x85c4, 0x85a4, 0x85c4, 0x85c5, 0x85c5, 0x85a4, 0x85a4, 0x85a4, 0x85a5, 0x85c4, 0x85a5, 0x846c, 0x7bd0, 0x840f, 0x7bf0, 
  0x840f, 0x7bf0, 0x842e, 0x85a5, 0x85c4, 0x85a5, 0x85c4, 0x85a4, 0x85c4, 0x85c4, 0x85c4, 0x85c4, 0x7da3, 0x7da3, 0x85c5, 0x85a4, 0x85a4, 0x85c4, 0x85a5, 0x85c4, 0x85a5, 0x842e, 0x7bf0, 0x840f, 
  0x7bf0, 0x7bef, 0x8586, 0x85c4, 0x85a5, 0x85c4, 0x85a4, 0x85c4, 0x85c4, 0x85a4, 0x85c4, 0x85a4, 0x9e2a, 0x9608, 0x85a4, 0x85c5, 0x85c4, 0x85a4, 0x85c4, 0x85a5, 0x85c4, 0x8586, 0x7c0f, 0x7c0f, 
  0x7bb1, 0x84e9, 0x85e3, 0x85a5, 0x85c4, 0x85a4, 0x85c4, 0x85c4, 0x85c4, 0x85c5, 0x7d83, 0xae6d, 0xffff, 0xf7dd, 0x8de7, 0x85a4, 0x85c4, 0x85c4, 0x85a4, 0x85c4, 0x85a5, 0x85e3, 0x84e9, 0x7bd1, 
  0x7c0f, 0x8586, 0x85c4, 0x85a4, 0x85a4, 0x85a4, 0x85c5, 0x85c4, 0x85a4, 0x85c5, 0x7da3, 0xa64c, 0xfffe, 0xf7dd, 0x95e7, 0x85a4, 0x85a4, 0x85c5, 0x85c4, 0x85a4, 0x85a4, 0x85c4, 0x8586, 0x7bf0, 
  0x8508, 0x85c4, 0x85a4, 0x85a4, 0x85a4, 0x85c4, 0x7da3, 0x85c4, 0x85c4, 0x85a4, 0x7d83, 0xae6d, 0xd715, 0x9608, 0x85a4, 0x85c4, 0x85c5, 0x7da4, 0x85a4, 0x85c4, 0x85a4, 0x85a4, 0x85c4, 0x84ab, 
  0x8585, 0x85a4, 0x85c4, 0x85c4, 0x85c5, 0x7da4, 0xa64c, 0x8dc5, 0x85a4, 0x7da3, 0xc6d2, 0xffff, 0xffff, 0x9e2a, 0x7da3, 0x85c5, 0x7da3, 0xa64c, 0x8dc5, 0x85a4, 0x85c4, 0x85a4, 0x85c4, 0x8566, 
  0x85c4, 0x85a4, 0x85c4, 0x85a5, 0x7da3, 0x9609, 0xc6d2, 0xb68f, 0x7da3, 0xc6d1, 0xffff, 0xffde, 0xffff, 0xef9a, 0x7da3, 0x7d82, 0xae4c, 0xbed1, 0xae6d, 0x7d83, 0x85c5, 0x85c4, 0x85a4, 0x85c4, 
  0x85a4, 0x85c4, 0x85c4, 0x85c5, 0x7d83, 0xae6e, 0xbeb1, 0xb68e, 0x7d83, 0xef9b, 0xcf14, 0xffff, 0xe779, 0xe779, 0xe77a, 0x8de6, 0x9e2a, 0xc6d1, 0xbeb0, 0x85c5, 0x85a4, 0x85a4, 0x85c4, 0x85a4, 
  0x85a4, 0x85c4, 0x85c4, 0x85c5, 0x7da3, 0xae8e, 0xc6d2, 0x9e09, 0x8de7, 0xefbc, 0xcef4, 0xffff, 0xcf14, 0x7d83, 0xd736, 0xbed1, 0x85a4, 0xc6d2, 0xbeb0, 0x8dc5, 0x85a4, 0x85a4, 0x85c4, 0x85a4, 
  0x85c4, 0x85a4, 0x85c4, 0x85a5, 0x7d83, 0xae6d, 0xbeb1, 0xb6b0, 0x85c5, 0xa62b, 0xdf37, 0xffff, 0xe77a, 0x85a4, 0x7d83, 0x7d83, 0xae4c, 0xbeb1, 0xbeb0, 0x85a4, 0x85a4, 0x85c4, 0x85a4, 0x85c4, 
  0x85a5, 0x85c4, 0x85a4, 0x85c4, 0x85a4, 0x8de7, 0xc6d2, 0xa64c, 0x85a4, 0x7da3, 0xf7bd, 0xe779, 0xffff, 0xd737, 0x85a4, 0x85a4, 0x9e29, 0xbed1, 0xa64b, 0x7d83, 0x85c5, 0x85a4, 0x85c4, 0x8566, 
  0x84ca, 0x85a4, 0x85c4, 0x85a4, 0x85c5, 0x85a4, 0x9608, 0x85c5, 0x7d82, 0xa64b, 0xffff, 0x9e2a, 0xae6d, 0xffff, 0xa64b, 0x7da3, 0x85a4, 0x9609, 0x85a4, 0x85a4, 0x85a4, 0x85a4, 0x85c4, 0x846c, 
  0x7c0f, 0x8566, 0x85c4, 0x85a4, 0x85a4, 0x85c5, 0x85a4, 0x85a4, 0x85a4, 0xef9b, 0xef9b, 0x85a4, 0x8dc5, 0xffff, 0xbeb1, 0x7d82, 0x85c5, 0x85a4, 0x85c4, 0x85c4, 0x85a4, 0x85c4, 0x8566, 0x7bf0, 
  0x7bd1, 0x84ab, 0x85e3, 0x85a5, 0x85c4, 0x85a4, 0x85c5, 0x7d82, 0xbed1, 0xffff, 0x9e2a, 0x7da3, 0x85a4, 0xf7bd, 0xdf58, 0x7d82, 0x85c5, 0x85c5, 0x85c4, 0x85c4, 0x85a5, 0x85e3, 0x84ab, 0x7bd0, 
  0x7c0f, 0x7bf0, 0x8547, 0x85c4, 0x85a5, 0x85c4, 0x85c5, 0x7da3, 0xa64c, 0xbeb0, 0x7da3, 0x85c5, 0x7da3, 0xae6e, 0xae6e, 0x7da3, 0x85a4, 0x85a4, 0x85c4, 0x85a5, 0x85c4, 0x8547, 0x7bf0, 0x7c0f, 
  0x840f, 0x7bf0, 0x7c2e, 0x8585, 0x85c4, 0x85a5, 0x85c4, 0x85c5, 0x7da3, 0x7d83, 0x85c5, 0x85a4, 0x85c5, 0x7da3, 0x7da3, 0x85c5, 0x85c4, 0x85c4, 0x85a5, 0x85c4, 0x8585, 0x7c2e, 0x7bf0, 0x840f, 
  0x7bf0, 0x840f, 0x7bf0, 0x7c2e, 0x8585, 0x85e3, 0x85a5, 0x85a4, 0x85c5, 0x85c5, 0x85a4, 0x85c4, 0x85a4, 0x85c5, 0x85c5, 0x85a4, 0x85a5, 0x85a5, 0x85e3, 0x8585, 0x7c2e, 0x7bf0, 0x840f, 0x7bf0, 
  0x840f, 0x7bf0, 0x840f, 0x7bf0, 0x7c2e, 0x8527, 0x85c4, 0x85c4, 0x85c4, 0x85a4, 0x85a4, 0x85c4, 0x85c4, 0x85a4, 0x85a4, 0x85c4, 0x85c4, 0x85c4, 0x8527, 0x7c2e, 0x7bf0, 0x840f, 0x7bf0, 0x840f, 
  0x8410, 0x840f, 0x7bf0, 0x840f, 0x7bf0, 0x7bd0, 0x848c, 0x8547, 0x85a4, 0x85c4, 0x85c4, 0x85a4, 0x85a4, 0x85c4, 0x85c4, 0x85a4, 0x8547, 0x848c, 0x7bd0, 0x7bf0, 0x840f, 0x7bf0, 0x840f, 0x8410, 
  0x8410, 0x8410, 0x840f, 0x7bf0, 0x840f, 0x840f, 0x7bd0, 0x7bd1, 0x842e, 0x84ab, 0x8566, 0x85c4, 0x85c4, 0x8566, 0x84ab, 0x842e, 0x7bd1, 0x7bd0, 0x840f, 0x840f, 0x7bf0, 0x840f, 0x8410, 0x8410
};

// Cấu hình cho giao thức MQTT
const char* clientId = "IvySensor1";
const char* mqttServer = "192.168.1.110";
const int mqttPort = 1883;
// Username và password để kết nối đến MQTT server nếu server có
// bật chế độ xác thực trên MQTT server
// Nếu không dùng thì cứ để vậy
const char* mqttUsername = "<MQTT_BROKER_USERNAME>";
const char* mqttPassword = "<MQTT_BROKER_PASSWORD>";

// MQTT topic để gửi thông tin về nhiệt độ
const char* tempTopic = "/easytech.vn/LivingRoom/Temperature";
// MQTT topic để gửi thông tin về độ ẩm
const char* humidityTopic = "/easytech.vn/LivingRoom/Humidity";
// MQTT topic để gửi thông tin về ánh sáng
const char* lightStatusTopic = "/easytech.vn/LivingRoom/Light";
// MQTT topic để gửi thông tin về chuyển động khi phát hiện
const char* motionTopic = "/easytech.vn/LivingRoom/Motion";

// Khởi tạo thư viện để kết nối wifi và MQTT server
WiFiClient wclient;
PubSubClient client(wclient);

void setup(void)
{
  Serial.begin(115200);

  // Khởi tạo thư viện cho cảm biến DHT
  Wire.begin();
  dht.begin();

  // Set chế độ của chân nối với cảm biến chuyển động là Input
  pinMode(PIR_PIN, INPUT);

  // Do cảm biến chuyển động PIR bị nhiễu khi sử dụng gần Esp8266 nên ta cần
  // phải giảm mức tín hiệu của esp8266 để tránh nhiễu cho PIR
  //WiFi.setPhyMode(WIFI_PHY_MODE_11G); 
  WiFi.setOutputPower(7);

  // Khởi tạo thư viện điều khiển màn hình LCD
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  clearScreen();
  tft.setTextSize(1);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.setCursor(10, 50);
  
  tft.print("Dang ket noi Wifi");

  // Kết nối Wifi và chờ đến khi kết nối thành công
  WiFi.begin(WIFI_SSID, WIFI_PWD);

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    tft.print('.');
    count++;

    if (count > 20) {
      ESP.restart();
    }
  }

  tft.print("done!");
  tft.setCursor(10, 80);
  tft.print("IP: ");
  
  tft.print(WiFi.localIP());
  delay(1000);

  clearScreen();
  tft.setCursor(10, 50);  
  tft.print("Dang cap nhat gio tu Internet...");

  // Kết nối tới NTP server dùng UDP để cập nhật thời gian
  Serial.print("Dang cap nhat gio tu Internet...");
  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
  Serial.println("Done");

  tft.print("Done!");

  clearScreen();
  bitmap(19, 87, icon_humidity, 16, 16);
  bitmap(18, 107, icon_temperature, 16, 16);

  // Kết nối tới MQTT server
  client.setServer(mqttServer, mqttPort);
}


void loop() {
  if (!client.connected()) {
    if (client.connect(clientId, mqttUsername, mqttPassword)) {
      Serial.println("Ket noi den MQTT server thanh cong");
      client.loop();
    } else {
      Serial.print("Ket noi den MQTT server that bai. Error code: ");
      Serial.println(client.state());
    }
  } else {
    client.loop();  
  }
  
  updateSensorData();
  
  updateTime();
}

void updateSensorData() {
  // Đối với nhiệt độ, độ ẩm và ánh sáng, ta chỉ update sau một khoảng
  // thời gian nhất định được khai báo trong biến UPDATE_INTERVAL
  unsigned long currentMillis = millis();
  if (lastSensorUpdateTime == 0 || currentMillis - lastSensorUpdateTime >= UPDATE_INTERVAL) {
    Serial.println(); Serial.println();    
    Serial.println("-------------- Cap nhat du lieu cam bien ----------------");
    humidity = dht.readHumidity();
    dtostrf(humidity, 4, 1, humidityMsg);
    Serial.print("Do am la: "); Serial.print(humidityMsg); Serial.println("%");
  
    temperature = dht.readTemperature();
    dtostrf(temperature, 4, 1, temperatureMsg);
    Serial.print("Nhiet do la: "); Serial.print(temperatureMsg); Serial.println("'C");
    
    lightStatus = analogRead(A0)*100/1024;
    dtostrf(lightStatus, 4, 1, lightStatusMsg);
    Serial.print("Do sang la: "); Serial.print(lightStatusMsg); Serial.println("%");

    // Hiển thị dữ liệu ra màn hình LCD

    if (!isnan(temperature) && !isnan(humidity)) {
      tft.setTextSize(2);
  
      tft.setCursor(40, 87);
      tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
  
      tft.print(humidityMsg); tft.print("%");
  
      tft.setCursor(40, 108);
      tft.setTextColor(ST7735_MAGENTA, ST7735_BLACK);
  
      tft.print(temperatureMsg); tft.print("'C");
    }

    // Gửi dữ liệu về server
    client.publish(humidityTopic, humidityMsg);
    delay(100);
    client.publish(tempTopic, temperatureMsg);
    delay(100);
    client.publish(lightStatusTopic, lightStatusMsg);

    lastSensorUpdateTime = currentMillis;
  }

  // Đối với cảm biến chuyển động thì luôn đọc để phát hiện
  // chuyển động tức thời
  boolean currentMotionStatus = digitalRead(PIR_PIN);
  
  if (currentMotionStatus != lastMotionStatus) {
    lastMotionStatus = currentMotionStatus;
    if (currentMotionStatus) {
      Serial.println("Phat hien chuyen dong");
      bitmap2(130, 100, icon_motion, 24, 24);
      client.publish(motionTopic, "1");
    } else {
      Serial.println("Khong con chuyen dong");
      tft.fillRect(130, 100, 24, 24, ST7735_BLACK);
    }    
  }  
}

/*-------- Cập nhật thời gian dùng NTP ----------*/

void updateTime() {
  if (timeStatus() != timeNotSet) {
    if (now() != lastClockUpdateTime) { // Chỉ update nếu thời gian đã thay đổi
      lastClockUpdateTime = now();
      int currentHour = hour();
      int currentMinute = minute();
      int currentSec = second();

      int currentDay = day();
      int currentMonth = month();
      int currentYear = year();
      int weekDay = weekday();
      
      Serial.print("Gio hien tai la ");
      Serial.print(currentHour); Serial.print(":");
      Serial.print(currentMinute); Serial.print(":");
      Serial.print(currentSec); Serial.print(" ");

      Serial.print(currentDay); Serial.print("/");
      Serial.print(currentMonth); Serial.print("/");
      Serial.print(currentYear); Serial.print(" ");

      switch (weekDay)
      {
        case 1: Serial.print(" SUN"); break;
        case 2: Serial.print(" MON"); break;
        case 3: Serial.print(" TUE"); break;
        case 4: Serial.print(" WED"); break;
        case 5: Serial.print(" THU"); break;
        case 6: Serial.print(" FRI"); break;
        case 7: Serial.print(" SAT"); break;
      }

      Serial.println(); Serial.println();

      // Hiển thị ra màn hình LCD
      // current time
      tft.setTextSize(4);
      tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
    
      tft.setCursor(20, 5);
      printTwoDigitsByZero(currentHour);
      tft.setCursor(68, 5);
      tft.print(":");
      tft.setCursor(92, 5);
      printTwoDigitsByZero(currentMinute);
    
      int sx = 21;
      int sy = 40;
      int sec = currentSec*2;
      int black = (60-sec)*2;
      tft.fillRect(sx, sy, sec, 3, ST7735_YELLOW);
      tft.fillRect(sx+sec, sy, black, 3, ST7735_BLACK);

      // current date
      tft.setTextSize(2);
      tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
      tft.setCursor(19, 50);
      tft.print(currentDay);
      tft.print("/");
      tft.print(currentMonth);
      tft.print("/");
      tft.print(currentYear);

      // current week day
      tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
      tft.setCursor(65, 70);
      tft.setTextSize(1);
    
      switch (weekDay)
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
}

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

/*-------- Hiển thị thông tin ra màn hình LCD ----------*/
void clearScreen() {
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
}

void bitmap(int x, int y, int16_t *bitmap, int16_t w, int16_t h) {
  int16_t col, row;
  int16_t offset = 0;
  tft.setAddrWindow(x, y, x+w-1, y+h-1);

  for (row = 0; row < h; row++) {
    for (col = 0; col < w; col++) {
      tft.pushColor(pgm_read_word(bitmap+offset));
      offset++;
    }
  }
}

void bitmap2(int x, int y, int16_t *bitmap, int16_t w, int16_t h) {
  int16_t col, row;
  int16_t offset = 0;
  tft.setAddrWindow(x, y, x+w-1, y+h-1);
  for (row=0; row<h; row++) { // For each scanline...
    for (col=0; col<w; col++) { // For each pixel...
      //To read from Flash Memory, pgm_read_XXX is required.
      //Since image is stored as uint16_t, pgm_read_word is used as it uses 16bit address
      tft.drawPixel(x+col, y+row, pgm_read_word(bitmap + offset));
      offset++;
    } // end pixel
  }
}

bool printTwoDigitsByZero(int digits) {
  if (digits < 10) {
    tft.print("0");
  }

  tft.print(digits);

  return true;
}

