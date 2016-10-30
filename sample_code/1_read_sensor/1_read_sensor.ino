#include <ESP8266WiFi.h>
#include <string.h>
#include <Wire.h>
#include <DHT.h>

// Khai báo các chân nối với cảm biến
#define DHTTYPE  DHT22
#define DHTPIN   D3
#define PIR_PIN D1

const int UPDATE_INTERVAL = 5000; // Tần suất cập nhật dữ liệu
unsigned long lastUpdateTime = 0; // Lưu thời gian lần cuối cùng cập nhật dữ liệu sensor

float temperature, humidity, brightness;
char temperatureMsg[10]; // nhiệt độ
char humidityMsg[10]; // độ ẩm (%)
char brightnessMsg[10]; // ánh sáng (%)
char motionMsg[5]; // chuyển động (1 hoặc 0)

boolean lastMotionStatus = false;

// Khởi tạo cảm biến độ ẩm nhiệt độ
DHT dht(DHTPIN, DHTTYPE, 15);

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
}


void loop() {

  updateSensorData();
}

void updateSensorData() {
  // Đối với nhiệt độ, độ ẩm và ánh sáng, ta chỉ update sau một khoảng
  // thời gian nhất định được khai báo trong biến UPDATE_INTERVAL
  unsigned long currentMillis = millis();
  if (lastUpdateTime == 0 || currentMillis - lastUpdateTime >= UPDATE_INTERVAL) {
    Serial.println(); Serial.println();    
    Serial.println("-------------- Cap nhat du lieu cam bien ----------------");
    humidity = dht.readHumidity();
    dtostrf(humidity, 4, 1, humidityMsg);
    Serial.print("Do am la: "); Serial.print(humidityMsg); Serial.println("%");
  
    temperature = dht.readTemperature();
    dtostrf(temperature, 4, 1, temperatureMsg);
    Serial.print("Nhiet do la: "); Serial.print(temperatureMsg); Serial.println("'C");
    
    brightness = analogRead(A0)*100/1024;
    dtostrf(brightness, 4, 1, brightnessMsg);
    Serial.print("Do sang la: "); Serial.print(brightnessMsg); Serial.println("%");
    
    lastUpdateTime = currentMillis;
  }

  // Đối với cảm biến chuyển động thì luôn đọc để phát hiện
  // chuyển động tức thời
  boolean currentMotionStatus = digitalRead(PIR_PIN);
  
  if (currentMotionStatus != lastMotionStatus) {
    lastMotionStatus = currentMotionStatus;
    if (currentMotionStatus) {
      Serial.println("Phat hien chuyen dong");
    } else {
      Serial.println("Khong con chuyen dong");
    }    
  }
  
}


