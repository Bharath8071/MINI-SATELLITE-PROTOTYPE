// use baudrate 9600 
#include "config_temp.h"

#include "esp_adc_cal.h"
#include <HardwareSerial.h>
#include <HTTPClient.h>

#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>             
#include <NTPClient.h>
#include "ThingSpeak.h"
#include "DHT.h"
#include <Adafruit_Sensor.h>  
#include <Adafruit_BMP280.h>  
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>

int sensorEventCount = 0;

float latitude = 0.0;
float longitude = 0.0;

#define select_pin 4
HardwareSerial sim868(2);  // Use UART2 (TX: GPIO17, RX: GPIO16)

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

String folderToDelete = "/sat_data.json";  
bool SDcard_bool = true;

#define BMP280_I2C_ADDRESS  0x76
Adafruit_BMP280 bmp280;

DHT dht(13, DHT22);

WiFiClient client;
const char* ssid = WIFI_SSID ;  
const char* password = WIFI_PASSWORD ; 

unsigned long myChannelNumber = THINGSPEAK_API_NO;
const char * myWriteAPIKey = THINGSPEAK_API_KEY ;

void connectWiFi() {
    Serial.print("Connecting to Network...");
    WiFi.begin(ssid, password);
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println(WiFi.status() == WL_CONNECTED ? "\nConnected to ." : "\nFailed to connect.");
}

void BMP_setup() {
    while (!bmp280.begin(BMP280_I2C_ADDRESS)) {
        Serial.println("Could not find a valid BMP280 sensor!");
    }
    Serial.println("Found BMP280 sensor!");
}

void SDcard_setup(){
  while (!SD.begin(5)) {  
        Serial.println("SD Card initialization failed!");
        // while(1);
    }    
    Serial.println("SD card found!!");
}

void SendTOThingspeak(float lm35_temp,float dht_humidity, float dht_temp,float bmp_value,float aux_bmp){
        ThingSpeak.setField(1, lm35_temp);
        ThingSpeak.setField(2, dht_humidity);
        ThingSpeak.setField(3, dht_temp);
        ThingSpeak.setField(4, bmp_value);
        ThingSpeak.setField(5, aux_bmp);
        ThingSpeak.setField(6, latitude);
        ThingSpeak.setField(7, longitude);
}

void sim_setup() {
    Serial.println("started...");

    pinMode(select_pin, OUTPUT);
    digitalWrite(select_pin, LOW);
    delay(2000);
    digitalWrite(select_pin, HIGH);
    delay(3000);
    Serial.println("Checking SIM Module...");

    sim868.println("AT"); 
    delay(1000);
    
    if (sim868.available()) {  
        String response = sim868.readString();
        Serial.println("SIM Module Response:");
        Serial.println(response + "-START");

        if (response.indexOf("OK") != -1) {
            Serial.println("SIM Module is working!");
        } 
        
    } else {
        Serial.println("No response from SIM Module. Check connections.");
    }
}

String CCLK() {
  timeClient.update();

  // Convert UTC to IST by adding 5 hours 30 minutes
  time_t epochTime = timeClient.getEpochTime() + (5 * 3600) + (30 * 60);
  struct tm *timeinfo = gmtime(&epochTime);

  // Format into CCLK string
  char cclkCommand[50];
  sprintf(cclkCommand, "AT+CCLK=\"%02d/%02d/%02d,%02d:%02d:%02d+05\"",
          (timeinfo->tm_year + 1900) % 100,  // last 2 digits of year
          timeinfo->tm_mon + 1,              // month (0-11)
          timeinfo->tm_mday,
          timeinfo->tm_hour,
          timeinfo->tm_min,
          timeinfo->tm_sec);

  // Print result
  Serial.println("Formatted AT+CCLK string:");
  Serial.println(cclkCommand);
  Serial.println("-----------------------------");
  delay(2000); 
  return cclkCommand;
}

void CCLK_SYNC(String str_cclk){

    sendATCommand("AT+CLTS=1"); 
    sendATCommand("AT&W");
    sendATCommand("AT+CCLK?");
    sendATCommand(str_cclk);
    delay(2000);
    sendATCommand("AT+CCLK?");

}

String sendATCommand(String command) {
  sim868.println(command);
    if (sim868.available()) {
      String response = sim868.readString();
      Serial.println("SIM Module Response:");
      Serial.println(response + "-new output");
      return response;
  }
}

void setup() {
    Serial.begin(9600);
    sim868.begin(9600, SERIAL_8N1, 16, 17);  // SIM868 (RX=16, TX=17)

    WiFi.mode(WIFI_STA);
    connectWiFi();

    ThingSpeak.begin(client);

    Serial.println("BMP280 sensor testing");
    BMP_setup();
    delay(1000);
    Serial.println("SDcard testing");
    SDcard_setup();
    delay(1000);
    dht.begin();

    sim_setup();

    timeClient.begin();
    String cclk_str = CCLK();
    delay(1000);
    cclk_str = CCLK();
    CCLK_SYNC(cclk_str);

    sim868.println("AT+CENG=3");
    delay(500);

    JSONtoBeaconDB();
 
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }

    float lm35_temp = LM35_reader();
    delay(1000);
    float dht_temp = dht.readTemperature() , dht_humidity = dht.readHumidity();
    DHT_reader(dht_temp, dht_humidity);
    delay(1000);
    float bmp_value = BMP_reader();
    delay(1000);
    float aux_bmp = BMP_aux();
    delay(1000);

    if (WiFi.status() == WL_CONNECTED) {  

        if (SDcard_bool){
          SendJsonDataToThingSpeak(folderToDelete.c_str());
          SDcard_bool = false;
        }
        SendTOThingspeak(lm35_temp, dht_humidity, dht_temp, bmp_value, aux_bmp);
        int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
        Serial.println(x == 200 ? "Channel update successful:" + String(x) : "Problem updating channel. HTTP error code " + String(x));  
    } 
    else {
        SDcard_bool = true;
        WriteJsonData(folderToDelete.c_str(), lm35_temp, dht_humidity, dht_temp, bmp_value, aux_bmp);
    }

    sensorEventCount++;
    if (sensorEventCount >= 10) {
      getGSMTowerJSON();
      sensorEventCount = 0;
  }
    
    delay(5000);
}

void WriteJsonData(const char * jsonFile, float lm35_temp, float dht_humidity, float dht_temp, float bmp_value, float aux_bmp) {
    File file = SD.open(jsonFile, FILE_READ);
    StaticJsonDocument<1024> doc;
    JsonArray array;

    if (file) {
        DeserializationError error = deserializeJson(doc, file);
        file.close();
        if (!error) {
            array = doc.as<JsonArray>();
        } else {
            Serial.println("Error reading JSON. Starting new array.");
            doc.clear();  // Start fresh
            array = doc.to<JsonArray>();
        }
    } else {
        
        array = doc.to<JsonArray>();
    }

    JsonObject newEntry = array.createNestedObject();  // <-- This is the line you asked about
    newEntry["timestamp"] = sendATCommand("AT+CCLK?");
    newEntry["LM35_temp"] = lm35_temp;
    newEntry["DHT_Humidity"] = dht_humidity;
    newEntry["DHT_Temp"] = dht_temp;
    newEntry["BMP_Value"] = bmp_value;
    newEntry["AUX_Value"] = aux_bmp;

    // Save updated JSON array back to the file
    file = SD.open(jsonFile, FILE_WRITE);
    if (file) {
        serializeJson(doc, file);
        file.close();
        Serial.println("Data written to JSON.");
    } else {
        Serial.println("Failed to open file for writing.");
    }
}


void SendJsonDataToThingSpeak(const char * jsonFile) {
    File file = SD.open(jsonFile);
    if (!file) {
        Serial.println("No data to send.");
        return;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
        Serial.println("Error parsing JSON.");
        return;
    }

    for (JsonObject obj : doc.as<JsonArray>()) {
        long timestamp = obj["timestamp"];
        float lm35_temp = obj["LM35_temp"];
        float dht_humidity = obj["DHT_Humidity"];
        float dht_temp = obj["DHT_Temp"];
        float bmp_value = obj["BMP_Value"];
        float aux_bmp = obj["AUX_Value"];

        Serial.printf("Sending to ThingSpeak: Time: %lu, LM35: %.2f, Humidity: %.2f, Temp: %.2f, BMP: %.2f, AUX: %f \n",
                      timestamp, lm35_temp, dht_humidity, dht_temp, bmp_value, aux_bmp);

        SendTOThingspeak(lm35_temp, dht_humidity, dht_temp, bmp_value, aux_bmp);
        int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
        Serial.println(x == 200 ? "Channel update successful:" + String(x) : "Problem updating channel. HTTP error code " + String(x));  
    }

    
    SD.remove(jsonFile);
    Serial.println("JSON file deleted after successful upload.");
}

float LM35_reader() 
{
  int LM35_Raw_Sensor1 = 0;
  float LM35_TempC_Sensor1 = 0.0;
  float LM35_TempF_Sensor1 = 0.0;
  float Voltage = 0.0;

  // Read LM35_Sensor1 ADC Pin
  LM35_Raw_Sensor1 = analogRead(12);  
  // Calibrate ADC & Get Voltage (in mV)
  Voltage = readADC_Cal(LM35_Raw_Sensor1); 
  // TempC = Voltage(mV) / 10
  LM35_TempC_Sensor1 = Voltage / 10;
  LM35_TempF_Sensor1 = (LM35_TempC_Sensor1 * 1.8) + 32;
 
  // Print The Readings
  Serial.println();
  Serial.print("Temperature = ");
  Serial.print(LM35_TempF_Sensor1);
  Serial.println(" °F");
  
  return LM35_TempF_Sensor1;
}
 
uint32_t readADC_Cal(int ADC_Raw)
{
  esp_adc_cal_characteristics_t adc_chars;
  
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  return(esp_adc_cal_raw_to_voltage(ADC_Raw, &adc_chars));
}

void DHT_reader(float temperature,float humidity) {
    
    Serial.println ();
    Serial.print("DHT Humidity: ");
    Serial.print(humidity);
    Serial.print("%  Temperature: ");
    Serial.print(temperature);
    Serial.println("°C");
    Serial.println();

}

float BMP_reader() {
    float bmp_value = bmp280.readPressure();
    Serial.print("BMP280 pressure: ");
    Serial.print(bmp_value);
    Serial.println("Pa");
    Serial.println();
    return bmp_value;
}

float BMP_aux() {
    float aux_bmp = bmp280.readAltitude();
    Serial.print("AUX: ");
    Serial.println(aux_bmp);
    Serial.println();
    return aux_bmp;
}

void JSONtoBeaconDB() {

  String cellJson = getGSMTowerJSON();
  // Combine both parts
  String finalJson = "{";
  if (cellJson.length() > 0) {
    finalJson += "\"cellTowers\":[" + cellJson + "]";
  }
  finalJson += "}";

  Serial.println("\n📦 Final JSON to BeaconDB:");
  Serial.println(finalJson);

  // Send to BeaconDB
  HTTPClient http;
  http.begin("https://api.beacondb.net/v1/geolocate");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP32-GSM-GeoClient/1.0");

  int httpResponseCode = http.POST(finalJson);

  String response = "";
  if (httpResponseCode > 0) {
    response = http.getString();
    Serial.println("\n📬 BeaconDB Response:");
    Serial.println(response);
  } else {
    Serial.print("❌ HTTP Error: ");
    Serial.println(httpResponseCode);
  }

  http.end();

  int latIndex = response.indexOf("\"lat\":");
  int lngIndex = response.indexOf("\"lng\":");

  if (latIndex != -1 && lngIndex != -1) {
    latitude = response.substring(latIndex + 6, response.indexOf(",", latIndex)).toFloat();
    longitude = response.substring(lngIndex + 6, response.indexOf("\n", lngIndex)).toFloat();
  }

  Serial.print("📍 Latitude: "); Serial.println(latitude, 6);
  Serial.print("📍 Longitude: "); Serial.println(longitude, 6);

}


String getGSMTowerJSON() {
  sim868.println("AT+CENG?");
  delay(1000);

  String raw = "", line = "";
  long timeout = millis() + 3000;
  while (millis() < timeout) {
    while (sim868.available()) {
      raw += (char)sim868.read();
    }
  }

  Serial.println("\n📡 Raw GSM Data:");
  Serial.println(raw);

  String result = "";
  int index = 1;

  while (true) {
    String search = "+CENG: " + String(index) + ",\"";
    int start = raw.indexOf(search);
    if (start == -1) break;

    int end = raw.indexOf("\n", start);
    line = raw.substring(start + search.length(), end);
    if (line.indexOf("ffff") != -1 || line.startsWith(",,")) {
      index++;
      continue;
    }

    // Extract fields
    int mcc = line.substring(0, line.indexOf(',')).toInt();
    line = line.substring(line.indexOf(',') + 1);
    int mnc = line.substring(0, line.indexOf(',')).toInt();
    line = line.substring(line.indexOf(',') + 1);
    String lacHex = line.substring(0, line.indexOf(','));
    line = line.substring(line.indexOf(',') + 1);
    String cidHex = line.substring(0, line.indexOf(','));

    int lac = strtol(lacHex.c_str(), NULL, 16);
    int cid = strtol(cidHex.c_str(), NULL, 16);

    if (result.length() > 0) result += ",";
    result += "{";
    result += "\"mobileCountryCode\":" + String(mcc) + ",";
    result += "\"mobileNetworkCode\":" + String(mnc) + ",";
    result += "\"locationAreaCode\":" + String(lac) + ",";
    result += "\"cellId\":" + String(cid);
    result += "}";
    index++;
  }
  return result;
}