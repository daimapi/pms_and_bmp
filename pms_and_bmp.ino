// UTF-8
////////////////////////////////////////////////////////environment setting
#include <PMS.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include "WiFi.h"
#include "ThingSpeak.h"
#include "time.h"
#include <ESP32Time.h>
ESP32Time rtc;
PMS pms(Serial2);
PMS::DATA data;
Adafruit_BMP280 bmp;
WiFiClient client;

const char *ssid = "---";           // ssid:網路名稱
const char *password = "---"; // pasword：網路密碼
#define CHANNEL_ID 2544455
#define CHANNEL_WRITE_API_KEY "3ZEDL3WPV2JS3NNN"
const int time_array[] = {7200, 72000, 50400, 28800};
//const int time_array[] = {6240};
///////////////////////////////////////////////////////
void connectwifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.println(String("Connecting to ") + ssid);
  delay(100);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(10);
    Serial.print(".");
  }
  // how to sleep ???
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("\nIP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("WiFi status:");
    WiFi.printDiag(Serial); // 顯示WiFi連線狀態資訊：工作模式、Channel、SSID、Passphrase、BSSID
    Serial.println("WiFi RSSI:");
    Serial.println(WiFi.RSSI());
    Serial.println("");
  }
  else
    Serial.println("connection failed");

}

int sleeptime()
{
  struct tm now = rtc.getTimeStruct();
  int nowt = now.tm_hour * 3600 + now.tm_min * 60 + now.tm_sec;
  Serial.print("nowt : ");
  Serial.println(nowt);
  int tarray[sizeof(time_array)/sizeof(time_array[0])] = {};
    for (int i = 0; i < (sizeof(time_array)/sizeof(time_array[0])); i++)
    {
        tarray[i] = time_array[i];
    }
    
    for (int i = 0; i < (sizeof(tarray)/sizeof(tarray[0])); i++)
    {
        tarray[i] = tarray[i] - nowt;
        if (tarray[i] < 0)
        {
            tarray[i] = tarray[i] + 86400;
        }
    }
    int minVal = tarray[0];
    for (int i = 0; i < (sizeof(tarray)/sizeof(tarray[0])); i++)
    {
        minVal = min(tarray[i], minVal);
    }
  return minVal;
}

void PMSreq()
{
  // pms.wakeUp();

  // delay(30000);
  pms.requestRead();
  if (pms.readUntil(data))
  {
    Serial.print(F("HUM (%): "));
    Serial.println(data.AMB_HUM / 10);
    ThingSpeak.setField(4, data.AMB_HUM / 10);

    Serial.print(F("PM 1.0 (ug/m^3): "));
    Serial.println(data.PM_AE_UG_1_0);
    ThingSpeak.setField(1, data.PM_AE_UG_1_0);

    Serial.print(F("PM 2.5 (ug/m^3): "));
    Serial.println(data.PM_AE_UG_2_5);
    ThingSpeak.setField(2, data.PM_AE_UG_2_5);

    Serial.print(F("PM 10 (ug/m^3): "));
    Serial.println(data.PM_AE_UG_10_0);
    ThingSpeak.setField(3, data.PM_AE_UG_10_0);

    Serial.print(F("HCHO (mg/m^3): "));
    Serial.println(data.AMB_HCHO);
    ThingSpeak.setField(7, data.AMB_HCHO);
  }
  delay(1000);
  pms.sleep();
}

void BMPreq()
{
  if (bmp.takeForcedMeasurement())
  {
    Serial.print(F("Temperature = "));
    Serial.print(bmp.readTemperature());
    Serial.println(" *C");
    ThingSpeak.setField(5, round(bmp.readTemperature()));

    Serial.print(F("Pressure = "));
    Serial.print(bmp.readPressure() / 100);
    Serial.println(" hPa");
    ThingSpeak.setField(6, round(bmp.readPressure() / 100));
    // Serial.print(F("Approx altitude = "));
    // Serial.print(bmp.readAltitude(1013.25)); /* Adjusted to local forecast! */
    // Serial.println(" m");
  }
  else
  {
    Serial.println("Forced measurement failed!");
  }
}

void NTPsync()
{
  struct tm t;
  configTime(8 * 3600L, 0, "time.stdtime.gov.tw", "time.google.com");
  while (!getLocalTime(&t))
  {
    Serial.println("getLocalTime Error");
    delay(500);
  }
  rtc.setTimeStruct(t);
  // rtc.adjust(DateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec));
  Serial.print("NTPsynced : ");
  Serial.println(&t, "%A, %B %d %Y %H:%M:%S");
}

void setup()
{
  pinMode(2, OUTPUT);
  digitalWrite(2,LOW);
  Serial.begin(9600);
  Serial2.begin(9600); // for pms
  delay(1000);
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.print("hello world : ");
  Serial.println(wakeup_reason);
  Serial.println(esp_reset_reason());
  pms.passiveMode();

  unsigned status = bmp.begin(0x76);
  {
    Serial.println("bmp waking up");
    if (!status)
    {
      Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                       "try a different address!"));
      while (1)
        delay(10);
    }
    else
    {
      Serial.println("bmp init pass");
      bmp.setSampling(Adafruit_BMP280::MODE_FORCED,   /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X1,   /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X1,   /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_OFF,    /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_1); /* Standby time. */
    }
  }

  pms.wakeUp();
  Serial.println("pms waking up");
  delay(30000);
  Serial.println("pms init pass");

  connectwifi();
  ThingSpeak.begin(client);
  Serial.println("start process");
  BMPreq();
  PMSreq();
  ThingSpeak.setField(8, WiFi.RSSI());
  //ThingSpeak.writeFields(CHANNEL_ID, CHANNEL_WRITE_API_KEY);
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER)
  {
    ThingSpeak.writeFields(CHANNEL_ID, CHANNEL_WRITE_API_KEY);
    Serial.println("sent");
    if (rtc.getHour(true) == 2)
      NTPsync(); // sync every 02:00
  }
  else
  {
    NTPsync();
  }

  Serial.println("end process");
  int sleep_time = sleeptime();
  if (sleep_time == 0){
    sleep_time++;
  }
  Serial.print("byebye for : ");
  Serial.println(sleep_time);
  Serial.flush(); 
  esp_sleep_enable_timer_wakeup(sleep_time * 1000000ULL);
  esp_deep_sleep_start();
  //ESP.deepSleep(sleep_time * 1000000);
}

void loop()
{
}
