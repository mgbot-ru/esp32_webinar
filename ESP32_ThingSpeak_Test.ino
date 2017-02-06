#define BLYNK_PRINT Serial

#include "DHT.h"
#include <Wire.h>
#include <BH1750FVI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_BMP085_U.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

// Wi-Fi point
char ssid[] = "MGBot";
char pass[] = "Terminator812";
WiFiClient client;

// Blynk
char auth[] = "f3cf0144dbd14b19a10c63ec7a03f1d8";

// For ThingSpeak IoT
const String CHANNELID_0 = "206369";
const String WRITEAPIKEY_0 = "0WTBXBB0D5FZUPWQ";
//IPAddress thingspeak_server(184, 106, 153, 149);
//IPAddress thingspeak_server(52, 203, 165, 232);
char thingspeak_server[] = "thingspeak.com";
const int httpPort = 80;

// For ThingWorx IoT
char iot_server[] = "jrskillsiot.cloud.thingworx.com";
IPAddress iot_address(52, 203, 26, 63);
char appKey[] = "309f3951-2ce3-43e0-9a89-87543ace2e42";
char thingName[] = "esp32_test_thing";
char serviceName[] = "esp32_test_service";

// ThingWorx parameters
#define sensorCount 7
char* sensorNames[] = {"ds18b20_temp", "dht11_temp", "dht11_hum", "bh1750_light", "bmp180_pressure", "bmp180_temp", "mq2_air"};
float sensorValues[sensorCount];
// Номера датчиков
#define ds18b20_temp     0
#define dht11_temp       1
#define dht11_hum        2
#define bh1750_light     3
#define bmp180_pressure  4
#define bmp180_temp      5
#define mq2_air          6

#define THINGSPEAK_UPDATE_TIME 30000    // Update ThingSpeak data server
#define THINGWORX_UPDATE_TIME 30000     // Update ThingWorx data server
#define SENSORS_UPDATE_TIME 5000        // Update time for all sensors
#define PRINT_UPDATE_TIME 5000          // Print data to LCD
#define BLYNK_UPDATE_TIME 5000          // Update Blynk
#define AUTO_UPDATE_TIME 500            // Automatic cooler control

#define MAX_LCD_PAGES 3
int lcd_page = 0;

#define MAX_SMOKE 30

// DHT11 sensor
#define DHT11_PIN 33
DHT dht11(DHT11_PIN, DHT11, 15);

// DS18B20 sensor
#define ONE_WIRE_BUS 27
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

// Analog MQ-2 air sensor
#define MQ2_PIN 34

// BH1750 sensor
BH1750FVI LightSensor_1;

// BMP180 sensor
Adafruit_BMP085_Unified bmp180 = Adafruit_BMP085_Unified(10085);

// Relay
#define RELAY_PIN 32

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Control parameters
int relay_control = 0;
int btn_state = 0;
int auto_control = 0;

// Timer counters
unsigned long timer_thingspeak = 0;
unsigned long timer_thingworx = 0;
unsigned long timer_sensors = 0;
unsigned long timer_blynk = 0;
unsigned long timer_print = 0;
unsigned long timer_auto = 0;

#define TIMEOUT 1000 // 1 second timout

// Максимальное время ожидания ответа от сервера
#define IOT_TIMEOUT1 5000
#define IOT_TIMEOUT2 100

// Таймер ожидания прихода символов с сервера
long timer_iot_timeout = 0;

// Размер приемного буффера
#define BUFF_LENGTH 256

// Приемный буфер
char buff[BUFF_LENGTH] = "";

// Main setup
void setup()
{
  // Init serial port
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Init Wi-Fi
  /*
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
    delay(500);
    Serial.print(".");
    }
  */
  Blynk.begin(auth, ssid, pass);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Init actuators
  digitalWrite(RELAY_PIN, false);
  pinMode(RELAY_PIN, OUTPUT);

  // Init sensors
  pinMode(MQ2_PIN, ANALOG);
  dht11.begin();
  ds18b20.begin();
  if (!bmp180.begin()) Serial.println("Could not find a valid BMP085 sensor!");
  LightSensor_1.begin();
  LightSensor_1.setMode(Continuously_High_Resolution_Mode);

  // Init LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd_printstr("Hello!");
  lcd.setCursor(0, 1);
  lcd_printstr("I am ESP32");

  // First measurement and print data from sensors
  readDHT11();
  readDS18B20();
  readBH1750();
  readBMP180();
  readMQ2();
  printAllSensors();
}

// Main loop cycle
void loop()
{
  // Send data to ThingSpeak server
  if (millis() > timer_thingspeak + THINGSPEAK_UPDATE_TIME)
  {
    sendThingSpeakStream();
    timer_thingspeak = millis();
  }

  // Send data to ThingWorx server
  if (millis() > timer_thingworx + THINGWORX_UPDATE_TIME)
  {
    sendThingWorxStream();
    timer_thingworx = millis();
  }

  // Send data to Blynk
  if (millis() > timer_blynk + BLYNK_UPDATE_TIME)
  {
    sendBlynk();
    timer_blynk = millis();
  }

  // Read all sensors
  if (millis() > timer_sensors + SENSORS_UPDATE_TIME)
  {
    readDHT11();
    readDS18B20();
    readBH1750();
    readBMP180();
    readMQ2();
    printAllSensors();
    timer_sensors = millis();
  }

  // Autocontrol cooler
  if (millis() > timer_auto + AUTO_UPDATE_TIME)
  {
    autoControlCooler();
    timer_auto = millis();
  }

  // Print data to LCD
  if (millis() > timer_print + PRINT_UPDATE_TIME)
  {
    printDataLCD();
    timer_print = millis();
  }

  // Run blynk handler
  Blynk.run();
  delay(100);
}

// Send IoT packet to ThingSpeak
void sendThingSpeakStream()
{
  Serial.print("Connecting to ");
  Serial.print(thingspeak_server);
  Serial.println("...");
  if (client.connect(thingspeak_server, httpPort))
  {
    if (client.connected())
    {
      Serial.println("Sending data to ThingSpeak server...\n");
      String post_data = "field1=";
      post_data = post_data + String(sensorValues[ds18b20_temp], 1);
      post_data = post_data + "&field2=";
      post_data = post_data + String(sensorValues[dht11_temp], 1);
      post_data = post_data + "&field3=";
      post_data = post_data + String(sensorValues[bmp180_temp], 1);
      post_data = post_data + "&field4=";
      post_data = post_data + String(sensorValues[dht11_hum], 1);
      post_data = post_data + "&field5=";
      post_data = post_data + String(sensorValues[bmp180_pressure], 1);
      post_data = post_data + "&field6=";
      post_data = post_data + String(sensorValues[bh1750_light], 1);
      post_data = post_data + "&field7=";
      post_data = post_data + String(sensorValues[mq2_air], 1);
      Serial.println("Data to be send:");
      Serial.println(post_data);
      client.println("POST /update HTTP/1.1");
      client.println("Host: api.thingspeak.com");
      client.println("Connection: close");
      client.println("X-THINGSPEAKAPIKEY: " + WRITEAPIKEY_0);
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.print("Content-Length: ");
      int thisLength = post_data.length();
      client.println(thisLength);
      client.println();
      client.println(post_data);
      client.println();
      delay(1000);
      timer_thingspeak = millis();
      while ((client.available() == 0) && (millis() < timer_thingspeak + TIMEOUT));
      while (client.available() > 0)
      {
        char inData = client.read();
        Serial.print(inData);
      }
      Serial.println();
      client.stop();
    }
    Serial.println("Data sent OK!");
    Serial.println();
  }
}

// Подключение к серверу IoT ThingWorx
void sendThingWorxStream()
{
  // Подключение к серверу
  Serial.println("Connecting to IoT server...");
  if (client.connect(iot_address, 80))
  {
    // Проверка установления соединения
    if (client.connected())
    {
      // Отправка заголовка сетевого пакета
      Serial.println("Sending data to IoT server...\n");
      Serial.print("POST /Thingworx/Things/");
      client.print("POST /Thingworx/Things/");
      Serial.print(thingName);
      client.print(thingName);
      Serial.print("/Services/");
      client.print("/Services/");
      Serial.print(serviceName);
      client.print(serviceName);
      Serial.print("?appKey=");
      client.print("?appKey=");
      Serial.print(appKey);
      client.print(appKey);
      Serial.print("&method=post&x-thingworx-session=true");
      client.print("&method=post&x-thingworx-session=true");
      // Отправка данных с датчиков
      for (int idx = 0; idx < sensorCount; idx ++)
      {
        Serial.print("&");
        client.print("&");
        Serial.print(sensorNames[idx]);
        client.print(sensorNames[idx]);
        Serial.print("=");
        client.print("=");
        Serial.print(sensorValues[idx]);
        client.print(sensorValues[idx]);
      }
      // Закрываем пакет
      Serial.println(" HTTP/1.1");
      client.println(" HTTP/1.1");
      Serial.println("Accept: application/json");
      client.println("Accept: application/json");
      Serial.print("Host: ");
      client.print("Host: ");
      Serial.println(iot_server);
      client.println(iot_server);
      Serial.println("Content-Type: application/json");
      client.println("Content-Type: application/json");
      Serial.println();
      client.println();

      // Ждем ответа от сервера
      timer_iot_timeout = millis();
      while ((client.available() == 0) && (millis() < timer_iot_timeout + IOT_TIMEOUT1))
      {
        delay(10);
      }

      // Выводим ответ о сервера, и, если медленное соединение, ждем выход по таймауту
      int iii = 0;
      bool currentLineIsBlank = true;
      bool flagJSON = false;
      timer_iot_timeout = millis();
      while ((millis() < timer_iot_timeout + IOT_TIMEOUT2) && (client.connected()))
      {
        while (client.available() > 0)
        {
          char symb = client.read();
          Serial.print(symb);
          if (symb == '{')
          {
            flagJSON = true;
          }
          else if (symb == '}')
          {
            flagJSON = false;
          }
          if (flagJSON == true)
          {
            buff[iii] = symb;
            iii ++;
          }
          delay(10);
          timer_iot_timeout = millis();
        }
        delay(10);
      }
      buff[iii] = '}';
      buff[iii + 1] = '\0';
      Serial.println(buff);
      // Закрываем соединение
      client.stop();

      // Расшифровываем параметры
      StaticJsonBuffer<BUFF_LENGTH> jsonBuffer;
      JsonObject& json_array = jsonBuffer.parseObject(buff);
      relay_control = json_array["relay_control"];
      Serial.println("Relay control:   " + String(relay_control));
      Serial.println();
      // Делаем управление устройствами
      digitalWrite(RELAY_PIN, relay_control | btn_state | auto_control);
      Serial.println("Packet successfully sent!");
      Serial.println();
    }
  }
}

// Отправка данных в приложение Blynk
void sendBlynk()
{
  Serial.println("Sending data to Blynk...");
  Blynk.virtualWrite(V4, sensorValues[ds18b20_temp]); delay(50);
  Blynk.virtualWrite(V0, sensorValues[dht11_temp]); delay(50);
  Blynk.virtualWrite(V1, sensorValues[dht11_hum]); delay(50);
  Blynk.virtualWrite(V5, sensorValues[bh1750_light]); delay(50);
  Blynk.virtualWrite(V3, sensorValues[bmp180_pressure]); delay(50);
  Blynk.virtualWrite(V2, sensorValues[bmp180_temp]); delay(50);
  Blynk.virtualWrite(V6, sensorValues[mq2_air]); delay(50);
  Serial.println("Data successfully sent!");
}

// Прием данных с кнопки в приложении Blynk
BLYNK_WRITE(V7)
{
  btn_state = param.asInt();
  digitalWrite(RELAY_PIN, relay_control | btn_state | auto_control);
  Serial.print("Button state: ");
  Serial.println(btn_state);
}

// Автоматический контроль датчика дыма и вентилятора
void autoControlCooler()
{
  readMQ2();
  if (sensorValues[mq2_air] > MAX_SMOKE)
  {
    Serial.println("Cooler activated! ");
    auto_control = 1;
  }
  else
  {
    auto_control = 0;
  }
  digitalWrite(RELAY_PIN, relay_control | btn_state | auto_control);
}

// Вывод данных с датчиков на LCD экран
void printDataLCD()
{
  if (lcd_page == 0)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd_printstr("T = " + String(sensorValues[dht11_temp], 1) + " *C");
    lcd.setCursor(0, 1);
    lcd_printstr("H = " + String(sensorValues[dht11_hum], 1) + " %");
  }
  else if (lcd_page == 1)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd_printstr("P = " + String(sensorValues[bmp180_pressure], 1) + " mm");
    lcd.setCursor(0, 1);
    lcd_printstr("L = " + String(sensorValues[bh1750_light], 1) + " lx");
  }
  else if (lcd_page == 2)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd_printstr("A = " + String(sensorValues[mq2_air], 1) + " %");
  }
  lcd_page ++;
  if (lcd_page >= MAX_LCD_PAGES)
  {
    lcd_page = 0;
  }
}

// Print sensors data to terminal
void printAllSensors()
{
  for (int i = 0; i < sensorCount; i++)
  {
    Serial.print(sensorNames[i]);
    Serial.print(" = ");
    Serial.println(sensorValues[i]);
  }
  Serial.print("Relay state: ");
  Serial.println(relay_control);
  Serial.println("");
}

// Read DHT11 sensor
void readDHT11()
{
  sensorValues[dht11_hum] = dht11.readHumidity();
  sensorValues[dht11_temp] = dht11.readTemperature();
  if (isnan(sensorValues[dht11_hum]) || isnan(sensorValues[dht11_temp]))
  {
    Serial.println("Failed to read from DHT11 sensor!");
  }
}

// Read DS18B20 sensor
void readDS18B20()
{
  ds18b20.requestTemperatures();
  sensorValues[ds18b20_temp] = ds18b20.getTempCByIndex(0);
  if (isnan(sensorValues[ds18b20_temp]))
  {
    Serial.println("Failed to read from DS18B20 sensor!");
  }
}

// Read BH1750 sensor
void readBH1750()
{
  sensorValues[bh1750_light] = LightSensor_1.getAmbientLight();
}

// Read BMP180 sensor
void readBMP180()
{
  float t3 = 0;
  sensors_event_t p_event;
  bmp180.getEvent(&p_event);
  if (p_event.pressure)
  {
    sensorValues[bmp180_pressure] = p_event.pressure * 7.5006 / 10;
    bmp180.getTemperature(&t3);
  }
  sensorValues[bmp180_temp] = t3;
}

// Read analog MQ-2 sensor
void readMQ2()
{
  sensorValues[mq2_air] = analogRead(MQ2_PIN) / 4096.0 * 100.0;
}

// Print string to I2C LCD
void lcd_printstr(String str1)
{
  for (int i = 0; i < str1.length(); i++)
  {
    lcd.print(str1.charAt(i));
  }
}

