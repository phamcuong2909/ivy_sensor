#include <ESP8266WiFi.h>
#include <string.h>
#include <Wire.h>
#include <DHT.h>
#include <TimeLib.h>
#include <WiFiUdp.h>


// Khai báo các chân nối với cảm biến
#define DHTTYPE  DHT22
#define DHTPIN   D3
#define PIR_PIN D1

const int UPDATE_INTERVAL = 5000; // Tần suất cập nhật dữ liệu
unsigned long lastSensorUpdateTime = 0; // Lưu thời gian lần cuối cùng cập nhật dữ liệu sensor

float temperature, humidity, brightness;
char temperatureMsg[10]; // nhiệt độ
char humidityMsg[10]; // độ ẩm (%)
char brightnessMsg[10]; // ánh sáng (%)
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

  // Kết nối Wifi và chờ đến khi kết nối thành công
  WiFi.begin(WIFI_SSID, WIFI_PWD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  // Kết nối tới NTP server dùng UDP để cập nhật thời gian
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
}


void loop() {

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
    
    brightness = analogRead(A0)*100/1024;
    dtostrf(brightness, 4, 1, brightnessMsg);
    Serial.print("Do sang la: "); Serial.print(brightnessMsg); Serial.println("%");
    
    // save the last time sent data to server
    lastSensorUpdateTime = currentMillis;
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

/*-------- Đoạn code cập nhật thời gian dùng NTP ----------*/

void updateTime() {
  if (timeStatus() != timeNotSet) {
    if (now() != lastClockUpdateTime) { // Chỉ update nếu thời gian đã thay đổi
      lastClockUpdateTime = now();
      Serial.print("Gio hien tai la ");
      Serial.print(hour()); Serial.print(":");
      Serial.print(minute()); Serial.print(":");
      Serial.print(second()); Serial.print(" ");

      Serial.print(day()); Serial.print("/");
      Serial.print(month()); Serial.print("/");
      Serial.print(year()); Serial.print(" ");

      switch (weekday())
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

