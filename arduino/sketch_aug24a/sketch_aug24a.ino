#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <time.h>

// ===== Network / Server =====
const char* SSID   = "BEGA_MASHA";
const char* PASS   = "!*25Be06ga15Ma04sha*!";
const char* SERVER = "https://beove.ru/api/post_temp.php";

// Статический IP (под вашу сеть)
IPAddress ip(192,168,1,101);
IPAddress gw(192,168,1,1);
IPAddress mask(255,255,255,0);
IPAddress dns1(192,168,1,1);
IPAddress dns2(8,8,8,8);

// ===== Payload =====
const int SENSOR_ID = 1;  // отправляем числом

// ===== Timings =====
const uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;  // 15 s
const uint32_t RETRY_CONNECT_TIMEOUT_MS = 12000; // повтор
const uint32_t NTP_SYNC_TIMEOUT_MS     = 3000;   // 3 s
const uint32_t POST_TIMEOUT_MS         = 5000;   // 5 s
const uint32_t SLEEP_SEC               = 300;    // 5 минут

// ===== DS18B20 =====
const uint8_t ONE_WIRE_PIN  = D2;   // GPIO4
const uint8_t DS_RESOLUTION = 9;

// ===== Time (UTC) — можно отключить, если не нужно =====
const char* NTP1 = "pool.ntp.org";
const char* NTP2 = "time.google.com";
const long  GMT_OFFSET = 0;
const int   DST_OFFSET = 0;

// ===== Globals =====
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

// Встроенный LED на ESP8266 часто инверсный: LOW = ON, HIGH = OFF
inline void ledOn()  { digitalWrite(LED_BUILTIN, LOW); }
inline void ledOff() { digitalWrite(LED_BUILTIN, HIGH); }

void blinkBuiltin(uint8_t times, uint16_t on_ms = 150, uint16_t off_ms = 150) {
  for (uint8_t i = 0; i < times; i++) {
    ledOn();
    delay(on_ms);
    ledOff();
    if (i + 1 < times) delay(off_ms);
  }
}

void blinkSuccessLong() {
  ledOn();
  delay(500);   // длинная вспышка
  ledOff();
}

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);

  // Статический IP до begin()
  WiFi.config(ip, gw, dns1, mask, dns2);

  // Первая попытка
  WiFi.begin(SSID, PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED) return true;

  // Повторная
  WiFi.disconnect(true);
  delay(200);
  WiFi.begin(SSID, PASS);
  start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < RETRY_CONNECT_TIMEOUT_MS) {
    delay(100);
  }
  return WiFi.status() == WL_CONNECTED;
}

time_t nowTs() { time_t n=0; time(&n); return n; }
bool hasValidTime() { return nowTs() > 1700000000; }

bool syncTimeNTP(uint32_t timeoutMs) {
  configTime(GMT_OFFSET, DST_OFFSET, NTP1, NTP2);
  uint32_t start = millis();
  while (!hasValidTime() && (millis() - start) < timeoutMs) {
    delay(100);
  }
  return hasValidTime();
}

bool postTemperature(float tempC, int& httpCodeOut, String& bodyOut) {
  httpCodeOut = -1000; bodyOut = "";
  if (WiFi.status() != WL_CONNECTED) return false;

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();

  HTTPClient http;
  http.setTimeout(POST_TIMEOUT_MS);
  if (!http.begin(*client, SERVER)) return false;

  http.addHeader("Content-Type", "application/json");
  String body = String("{\"sensor_id\":") + SENSOR_ID + ",\"tvalue\":" + String(tempC, 2) + "}";
  httpCodeOut = http.POST(body);
  bodyOut = http.getString();
  http.end();
  return (httpCodeOut == HTTP_CODE_OK);
}

void goDeepSleepSec(uint32_t sec) {
  ESP.deepSleep((uint64_t)sec * 1000000ULL);
  delay(50);
}

void setup() {
  Serial.begin(115200);
  delay(30);
  Serial.println();
  Serial.println(F("Boot: deep-sleep cycle start (no external LED)"));

  pinMode(LED_BUILTIN, OUTPUT);
  ledOff(); // погасить

  // 1) Wi‑Fi
  Serial.print(F("Wi‑Fi: connecting (static IP) ... "));
  bool wifiOk = connectWiFi();
  if (!wifiOk) {
    Serial.println(F("FAILED"));
    // Индикация: Wi‑Fi ошибка — тройное мигание
    blinkBuiltin(3, 120, 130);
    goDeepSleepSec(SLEEP_SEC);
    return;
  }
  Serial.print(F("OK, IP="));
  Serial.println(WiFi.localIP());

  // 2) (опционально) NTP
  Serial.print(F("NTP: sync... "));
  if (syncTimeNTP(NTP_SYNC_TIMEOUT_MS)) {
    time_t now = nowTs();
    struct tm t;
    gmtime_r(&now, &t);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &t);
    Serial.print(F("OK, UTC="));
    Serial.println(ts);
  } else {
    Serial.println(F("FAILED (continue)"));
  }

  // 3) Измерение
  Serial.println(F("Sensor: DS18B20 read..."));
  sensors.begin();
  sensors.setResolution(DS_RESOLUTION);
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  if (tempC == DEVICE_DISCONNECTED_C || isnan(tempC)) {
    Serial.println(F("Error: DS18B20 not found/invalid. Sleep 5 min."));
    // Можно поморгать 4 раза, если хотите особую индикацию датчика:
    // blinkBuiltin(4, 120, 120);
    goDeepSleepSec(SLEEP_SEC);
    return;
  }
  Serial.print(F("Sensor: "));
  Serial.print(tempC, 2);
  Serial.println(F(" C"));

  // 4) Отправка
  Serial.print(F("HTTP: POST ... "));
  int httpCode = -1000;
  String resp;
  bool ok = postTemperature(tempC, httpCode, resp);

  if (!ok) {
    Serial.print(F("FAILED, code="));
    Serial.println(httpCode);
    Serial.print(F("Body: "));
    Serial.println(resp);
    // Индикация: ошибка сервера/отправки — двойное мигание
    blinkBuiltin(2, 150, 150);
  } else {
    Serial.println(F("OK (200)"));
    Serial.print(F("Body: "));
    Serial.println(resp);
    // Индикация: успех — одно длинное мигание
    blinkSuccessLong();
  }

  // 5) Сон на 5 минут
  Serial.println(F("Sleep: 300 sec"));
  goDeepSleepSec(SLEEP_SEC);
}

void loop() {
  // не используется
}
