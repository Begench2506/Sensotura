#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <time.h>

// ===== User settings =====
const char* SSID       = "BEGA_MASHA";
const char* PASS       = "!*25Be06ga15Ma04sha*!";
const char* SERVER     = "https://beove.ru/api/post_temp.php";
const int   SENSOR_ID  = 1;   // отправляем как число

// Period: every 5 minutes, aligned slots: :00, :05, :10 … :55 (UTC)
const uint32_t SLOT_MINUTES   = 5;

// Time: keep UTC (если нужна МСК, выставить GMT_OFFSET = 3*3600 и использовать localtime_r)
const char* NTP_SERVER_1 = "pool.ntp.org";
const char* NTP_SERVER_2 = "time.google.com";
const long  GMT_OFFSET   = 0;
const int   DST_OFFSET   = 0;

// Timeouts (ms)
const uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
const uint32_t NTP_SYNC_TIMEOUT_MS     = 4000;
const uint32_t POST_TIMEOUT_MS         = 5000;

// DS18B20
const uint8_t ONE_WIRE_PIN = D2;    // GPIO4
const uint8_t DS_RESOLUTION = 9;    // fast enough

// RGB (common anode to 3V3; LOW = on, HIGH = off)
const uint8_t PIN_LED_R = D3; // GPIO0 (boot pin)
const uint8_t PIN_LED_G = D4; // GPIO2 (boot pin)
const uint8_t PIN_LED_B = D1; // GPIO5 (safe)

// Blink durations
const uint16_t BLINK_ERR_MS   = 120; // error blink
const uint16_t BLINK_OK_MS    = 80;  // success blink

// ===== Globals =====
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

enum class Phase { Idle, WaitingNextSlot, Measuring, Sending };
Phase phase = Phase::Idle;

unsigned long nextActionMs = 0;  // миллисекунды (millis) для следующего шага
bool haveTime = false;

// —— Helpers ——
void ledsAllOff() {
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  digitalWrite(PIN_LED_R, HIGH);
  digitalWrite(PIN_LED_G, HIGH);
  digitalWrite(PIN_LED_B, HIGH);
}

void blinkRGB(bool r, bool g, bool b, uint16_t ms) {
  ledsAllOff();
  if (r) digitalWrite(PIN_LED_R, LOW);
  if (g) digitalWrite(PIN_LED_G, LOW);
  if (b) digitalWrite(PIN_LED_B, LOW);
  delay(ms);
  ledsAllOff();
}

bool connectWiFi(uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(SSID, PASS);

  Serial.print(F("Wi-Fi: connecting to '"));
  Serial.print(SSID);
  Serial.print(F("' ... "));
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("OK, IP="));
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println(F("FAILED (timeout)"));
    return false;
  }
}

time_t nowTs() { time_t n=0; time(&n); return n; }
bool hasValidTime() { return nowTs() > 1700000000; } // после ~2023-11-14

bool syncTimeNTP(uint32_t timeoutMs) {
  Serial.print(F("NTP: syncing UTC... "));
  configTime(GMT_OFFSET, DST_OFFSET, NTP_SERVER_1, NTP_SERVER_2);
  uint32_t start = millis();
  while (!hasValidTime() && (millis() - start) < timeoutMs) {
    delay(100);
  }
  if (hasValidTime()) {
    time_t now = nowTs();
    struct tm t;
    gmtime_r(&now, &t);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &t);
    Serial.print(F("OK, UTC="));
    Serial.println(ts);
    return true;
  } else {
    Serial.println(F("FAILED"));
    return false;
  }
}

// Возвращает секунды до ближайшей пятиминутки (UTC)
uint32_t secondsToNextAlignedSlotUTC() {
  time_t now = nowTs();
  struct tm t;
  gmtime_r(&now, &t);
  uint8_t m = t.tm_min;
  uint8_t s = t.tm_sec;

  uint8_t nextMin = ((m / SLOT_MINUTES) + 1) * SLOT_MINUTES;
  uint8_t addMin  = (nextMin >= 60) ? (60 - m) : (nextMin - m);
  // корректная формула: до «ровной минуты»
  int32_t remain = (int32_t)addMin * 60 - (int32_t)s;
  if (remain <= 0) remain = SLOT_MINUTES * 60;
  return (uint32_t)remain;
}

bool postTemperature(float tempC, int& httpCodeOut, String& bodyOut) {
  httpCodeOut = -1000;
  bodyOut = "";
  if (WiFi.status() != WL_CONNECTED) return false;

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure(); // no cert validation

  HTTPClient http;
  http.setTimeout(POST_TIMEOUT_MS);
  if (!http.begin(*client, SERVER)) {
    return false;
  }

  // JSON с числовым sensor_id
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"sensor_id\":") + SENSOR_ID + ",\"tvalue\":" + String(tempC, 2) + "}";
  httpCodeOut = http.POST(body);
  bodyOut = http.getString();
  http.end();
  return (httpCodeOut == HTTP_CODE_OK);
}

// Планирует следующий «ровный» слот: ставит nextActionMs
void scheduleNextAlignedSlot() {
  if (!haveTime) {
    // если нет времени — просто через 5 минут
    nextActionMs = millis() + (uint32_t)SLOT_MINUTES * 60UL * 1000UL;
    Serial.println(F("Align: no time; schedule fixed 5 min later"));
    return;
  }
  uint32_t waitSec = secondsToNextAlignedSlotUTC();
  nextActionMs = millis() + waitSec * 1000UL;
  Serial.print(F("Align: next slot in "));
  Serial.print(waitSec);
  Serial.println(F(" sec"));
}

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println(F("Boot: sensor node starting (no deep sleep)..."));

  // LEDs off
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  ledsAllOff();

  // Wi‑Fi
  if (!connectWiFi(WIFI_CONNECT_TIMEOUT_MS)) {
    // Wi‑Fi недоступен: красная вспышка, план через 5 мин по таймеру
    blinkRGB(true, false, false, BLINK_ERR_MS);
    haveTime = syncTimeNTP(NTP_SYNC_TIMEOUT_MS); // попробуем получить время через мобильный/другую сеть; если нет — всё равно планируем по таймеру
    scheduleNextAlignedSlot();
    phase = Phase::WaitingNextSlot;
    return;
  }

  // NTP
  haveTime = syncTimeNTP(NTP_SYNC_TIMEOUT_MS);
  scheduleNextAlignedSlot();
  phase = Phase::WaitingNextSlot;
}

void loop() {
  unsigned long nowMs = millis();

  // поддерживаем Wi‑Fi (на случай, если роутер «засыпает»)
  if (WiFi.status() != WL_CONNECTED) {
    // попытка переподключения в фоне
    static unsigned long lastRetry = 0;
    if (nowMs - lastRetry > 5000) {
      lastRetry = nowMs;
      Serial.println(F("Wi‑Fi: reconnect..."));
      WiFi.disconnect();
      WiFi.begin(SSID, PASS);
    }
  }

  switch (phase) {
    case Phase::WaitingNextSlot:
      if ((long)(nowMs - nextActionMs) >= 0) {
        // пришло время слота
        phase = Phase::Measuring;
      }
      break;

    case Phase::Measuring: {
      Serial.println(F("Sensor: requesting temperature..."));
      sensors.begin();
      sensors.setResolution(DS_RESOLUTION);
      sensors.requestTemperatures();
      float tempC = sensors.getTempCByIndex(0);
      if (tempC == DEVICE_DISCONNECTED_C || isnan(tempC)) {
        Serial.println(F("Error: DS18B20 not found or invalid reading."));
        // планируем следующий слот
        scheduleNextAlignedSlot();
        phase = Phase::WaitingNextSlot;
        return;
      }
      Serial.print(F("Sensor: temperature = "));
      Serial.print(tempC, 2);
      Serial.println(F(" C"));

      // сразу отправка
      phase = Phase::Sending;

      // Отправка
      int httpCode = -1000;
      String resp;
      bool ok = postTemperature(tempC, httpCode, resp);
      if (!ok) {
        Serial.print(F("HTTP: FAILED, code="));
        Serial.println(httpCode);
        Serial.print(F("HTTP body: "));
        Serial.println(resp);
        // оранжевая вспышка
        blinkRGB(true, true, false, BLINK_ERR_MS);
      } else {
        Serial.println(F("HTTP: OK (200)"));
        Serial.print(F("HTTP body: "));
        Serial.println(resp);
        // зелёная вспышка
        blinkRGB(false, true, false, BLINK_OK_MS);
      }

      // спланировать следующий «ровный» слот
      scheduleNextAlignedSlot();
      phase = Phase::WaitingNextSlot;
    } break;

    case Phase::Sending:
    case Phase::Idle:
    default:
      // ничего
      break;
  }

  // чтобы цикл не крутился на полную
  delay(10);
}
