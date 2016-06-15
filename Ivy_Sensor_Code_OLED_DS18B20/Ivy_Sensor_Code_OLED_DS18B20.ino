/**The MIT License (MIT)

Copyright (c) 2015 by Daniel Eichhorn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at http://blog.squix.ch
*/

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <JsonListener.h>
#include "SSD1306.h"
#include "SSD1306Ui.h"
#include "Wire.h"
#include "WundergroundClient.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "TimeClient.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <NTPClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

/********************************
 * Bắt Đầu Phần Khai Báo Cấu Hình
 ********************************/

// Cấu hình Wifi, sửa lại theo đúng mạng Wifi của bạn
const char* WIFI_SSID = "Sandiego";
const char* WIFI_PWD = "0988807067";

// Khai báo tần suất cập nhật dữ liệu là 10 phút 1 lần
const int UPDATE_INTERVAL_SECS = 10 * 60;

// Trên module Ivy sensor, esp8266 nối với màn hình Oled I2C
// với 2 chân D3 (SDC) và D4 (SDA)
// Địa chỉ 0x3c là mặc định của nhà sản xuất Oled
const int I2C_DISPLAY_ADDRESS = 0x3c;
const int SDA_PIN = D4;
const int SDC_PIN = D3;

// chân Data của DS18B20 được nối với D2
#define TEMP_SENSOR_PIN D2

// Cấu hình cảm biến chuyển động, input được nối với chân D1
#define PIR_PIN D1

// Cấu hình thư viện NTP để lấy giờ hiện tại từ Internet
// Đây là số giây lệch với múi giờ GMT của Việt Nam, GMT+7 ~ 7*60*60=25200
const float UTC_OFFSET = 25200;

// Cấu hình thư viện Wunderground dùng để lấy thông tin thời tiết từ Internet
// Để có API KEY, các bạn đăng ký user ở trang https://www.wunderground.com/
// Sau đó vào menu More > Weather API For Developers > Key Settings > Key ID
const boolean IS_METRIC = true;
const String WUNDERGRROUND_API_KEY = "16438ae0363f1cea";
const String WUNDERGRROUND_LANGUAGE = "VU"; // chọn ngôn ngữ trả về là tiếng Việt
const String WUNDERGROUND_COUNTRY = "CA";
const String WUNDERGROUND_CITY = "Ho_Chi_Minh"; // Chọn thành phố

// Cấu hình cho giao thức MQTT
const char* clientId = "IvySensor1aaaa";
//const char* mqttServer = "192.168.1.110";
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
// Username và password để kết nối đến MQTT server nếu có 
// bật chế độ xác thực trên MQTT server
// Nếu không dùng thì cứ để vậy
const char* mqttUsername = "<MQTT_BROKER_USERNAME>";
const char* mqttPassword = "<MQTT_BROKER_PASSWORD>";

// Tên MQTT topic để gửi thông tin về nhiệt độ
const char* tempTopic = "/eHome/Bedroom1/Temperature";
// Tên MQTT topic để gửi thông tin về độ ẩm
const char* humidityTopic = "/eHome/Bedroom1/Humidity";
// Tên MQTT topic để gửi thông tin về ánh sáng
const char* brightnessTopic = "/eHome/Bedroom1/Brightness";
// Tên MQTT topic để gửi thông tin về chuyển động khi phát hiện
const char* motionTopic = "/eHome/Bedroom1/Motion";


// Khởi tạo màn hình Oled
SSD1306   display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
SSD1306Ui ui     ( &display );

/**********************************
 * Kết thúc phần khai báo cấu hình
 **********************************/

// Khởi tạo thư viện NTP để lấy giờ hiện tại từ Internet
NTPClient timeClient(UTC_OFFSET); 

// Khởi tạo thư viện Weather Underground để lấy thông tin thời tiết
// IS_METRIC được set thành false để sử dụng đơn vị Km, độ C... thay vì
// đơn vị Mile, độ F
WundergroundClient wunderground(IS_METRIC);

// các hàm hiển thị thông tin trên màn hình Oled
// Mỗi hàm dùng để thể hiện 1 slide
bool drawFrame1(SSD1306 *display, SSD1306UiState* state, int x, int y);
bool drawFrame2(SSD1306 *display, SSD1306UiState* state, int x, int y);
bool drawFrame3(SSD1306 *display, SSD1306UiState* state, int x, int y);
bool drawFrame4(SSD1306 *display, SSD1306UiState* state, int x, int y);
void setReadyForWeatherUpdate();
void drawForecast(SSD1306 *display, int x, int y, int dayIndex);

// Khai báo mảng các hàm bên trên để hiển thị trên màn hình Oled
// Mỗi hàm hiển thị 1 slide trượt từ phải qua trái
bool (*frames[])(SSD1306 *display, SSD1306UiState* state, int x, int y) = { drawFrame1, drawFrame2, drawFrame3, drawFrame4 };
int numberOfFrames = 4;

// Cờ này sẽ được bật lên khi đến lúc phải cập nhật dữ liệu
bool readyForWeatherUpdate = false;

String lastUpdate = "--";

// Các chuỗi lưu dữ liệu thu thập được để hiển thị trên màn hình Oled 
// và gửi về server qua giao thức MQTT
char temperatureMsg[10]; // nhiệt độ
char humidityMsg[10]; // độ ẩm (%)
char brightnessMsg[10]; // ánh sáng (%)
char motionMsg[5]; // chuyển động (1 hoặc 0)
float temperature, humidity, brightness;
boolean motionOn = false;

// Khởi tạo cảm biến độ ẩm nhiệt độ
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature DS18B20(&oneWire);

// Khởi tạo thư viện để kết nối wifi và MQTT server
WiFiClient wclient;
PubSubClient client(wclient);
Ticker ticker;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // initialize dispaly
  display.init();
  display.clear();
  display.display();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  WiFi.begin(WIFI_SSID, WIFI_PWD);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Connecting to WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();

    counter++;
  }

  // Kết nối tới MQTT server
  client.setServer(mqttServer, mqttPort);
  // Đăng ký hàm sẽ xử lý khi có dữ liệu từ MQTT server gửi về
  client.setCallback(onMQTTMessageReceived);

  // Chạy thư viện đọc cảm biến nhiệt độ
  DS18B20.begin();

  ui.setTargetFPS(30);

  ui.setActiveSymbole(activeSymbole);
  ui.setInactiveSymbole(inactiveSymbole);

  // Có 4 options là TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Khai báo frame đầu tiên hiển thị ở đâu
  ui.setIndicatorDirection(LEFT_RIGHT);

  // Khai báo chế độ chuyển đổi giữa khác khung hình
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  // Khai báo các hàm hiển thị từng frame dùng các hàm bên trên
  ui.setFrames(frames, numberOfFrames);

  // Khởi tạo màn hình sau khi khai báo các settings xong
  ui.init();

  Serial.println("");

  // cập nhật dữ liệu lần đầu
  updateData(&display);

  // Yêu cầu cập nhật dữ liệu theo tần suất khai báo bên trên
  ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);

  // Khởi tạo chức năng nạp code qua mạng wifi
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);  
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("IvySensor1");
  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    display.clear();
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    FirmwareUpdateDone(&display);
    Serial.println("\nEnd");
    // delay(500);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    FirmwareUpdate(&display, progress, total);   
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}

void loop() {

  if (readyForWeatherUpdate && ui.getUiState().frameState == FIXED) {
    updateData(&display);
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
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
  
    boolean motionInput = digitalRead(PIR_PIN);
  
    if (motionInput != motionOn) {
      motionOn = motionInput;
      if (motionOn) {
        Serial.print("Motion: "); Serial.println(motionOn);
      }
      sprintf(motionMsg, "%d", motionOn);
      client.publish(motionTopic, motionMsg);
    }
  }

}

void updateData(SSD1306 *display) {
  drawProgress(display, 10, "Cập nhật giờ...");
  timeClient.begin();
  drawProgress(display, 30, "Cập nhật thời tiết...");
  wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(display, 50, "Cập nhật dự báo thời tiết...");
  wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  
  drawProgress(display, 80, "Cập nhật dữ liệu cảm biến...");
  do {
    DS18B20.requestTemperatures(); 
    temperature = DS18B20.getTempCByIndex(0);
    delay(100);
  } while (temperature == 85.0 || temperature == (-127.0));
  Serial.print("Nhiệt độ đọc được là: "); Serial.println(temperature);

  dtostrf(temperature, 4, 1, temperatureMsg);
  client.publish(tempTopic, temperatureMsg);
  delay(200);

  brightness = analogRead(A0)*100/1024;
  dtostrf(brightness, 4, 1, brightnessMsg);
  client.publish(brightnessTopic, brightnessMsg);
  Serial.print("Độ sáng (%) hiện tại là: "); Serial.println(brightness);

  lastUpdate = timeClient.getFormattedTime();
  Serial.println(lastUpdate);
  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Hoàn thành cập nhật dữ liệu!");
  delay(1000);
}

void drawProgress(SSD1306 *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawRect(10, 28, 108, 12);
  display->fillRect(12, 30, 104 * percentage / 100 , 9);
  display->display();
}

// Hiển thị ngày giờ hiện tại
bool drawFrame1(SSD1306 *display, SSD1306UiState* state, int x, int y) {  
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_16);
  String date = wunderground.getDate();
  int textWidth = display->getStringWidth(date);
  display->drawString(64 + x, 5 + y, date);
  display->setFont(ArialMT_Plain_24);
  String time = timeClient.getFormattedTime();
  textWidth = display->getStringWidth(time);
  display->drawString(64 + x, 30 + y, time);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

// Hiển thị thời tiết hiện tại từ Internet
bool drawFrame2(SSD1306 *display, SSD1306UiState* state, int x, int y) {
  display->setFont(ArialMT_Plain_10);
  display->drawString(60 + x, 10 + y, wunderground.getWeatherText());

  display->setFont(ArialMT_Plain_24);
  String temp = wunderground.getCurrentTemp() + "°C";
  display->drawString(60 + x, 20 + y, temp);
  int tempWidth = display->getStringWidth(temp);

  display->setFont(Meteocons_0_42);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  display->drawString(32 + x - weatherIconWidth / 2, 10 + y, weatherIcon);
}

// Hiển thị dự báo thời tiết cho 3 ngày
bool drawFrame3(SSD1306 *display, SSD1306UiState* state, int x, int y) {
  drawForecast(display, x, y, 0);
  drawForecast(display, x + 44, y, 2);
  drawForecast(display, x + 88, y, 4);
}

// Hiển thị dữ liệu từ cảm biến
bool drawFrame4(SSD1306 *display, SSD1306UiState* state, int x, int y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 0 + y, "Trong Phòng");
  display->setFont(ArialMT_Plain_24);  
  display->drawString(64 + x, 10 + y, String(temperatureMsg) + "°C");
  display->drawString(64 + x, 35 + y, String(brightnessMsg) + "%");
}

void drawForecast(SSD1306 *display, int x, int y, int dayIndex) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
  day.toUpperCase();
  display->drawString(x + 20, y, day);

  display->setFont(Meteocons_0_21);
  display->drawString(x + 20, y + 15, wunderground.getForecastIcon(dayIndex));

  display->setFont(ArialMT_Plain_16);
  display->drawString(x + 20, y + 37, wunderground.getForecastLowTemp(dayIndex) + "/" + wunderground.getForecastHighTemp(dayIndex));
}

void setReadyForWeatherUpdate() {
  Serial.println("Đến giờ cập nhật dữ liệu");
  readyForWeatherUpdate = true;
}

void FirmwareUpdate(SSD1306 *display,unsigned int progress, unsigned int total ) {
  drawProgress(display, (progress / (total / 100)), "OTA Firmware update");
}

void FirmwareUpdateDone(SSD1306 *display) {
  drawProgress(display, 100, "Done. Rebooting...");
}

void onMQTTMessageReceived(char* topic, byte* payload, unsigned int length) {
  Serial.print("Nhận được dữ liệu từ MQTT server [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}
