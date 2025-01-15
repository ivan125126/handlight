/******************************************************
 * ESP8266 Client (Lamp) - Polling the Intermediate
 ******************************************************/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>  // 建議使用 ArduinoJson 來解析 JSON

// ------ 要連到的中繼站 AP ------
const char* AP_SSID     = "Zone_0_AP";   
const char* AP_PASSWORD = "12345678";

// ------ 中繼站 AP IP (預設 192.168.4.1) ------
IPAddress intermediateIP(192,168,4,1); 

// ------ RGB LED ------
const int RED_PIN   = D1;
const int GREEN_PIN = D2;
const int BLUE_PIN  = D3;

// ------ 模式參數 ------
String currentMode = "off";
int currentR = 0, currentG = 0, currentB = 0;

// ------ Blink ------
bool  isBlinkOn = false;
long  lastBlinkTime = 0;
int   blinkInterval = 500; // ms

// ------ Fade (呼吸) ------
int   fadeValue = 0;
int   fadeStep  = 5;
long  lastFadeTime = 0;
int   fadeInterval = 30;

// ------ 待機 ------
bool isConnected = false;

// ------ 週期向中繼站詢問 (pull) ------
unsigned long lastPollTime = 0;
unsigned long pollInterval = 200; // 每 2 秒詢問一次

void setup() {
  Serial.begin(115200);
  Serial.println("\n[Client] Start");

  pinMode(RED_PIN,   OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN,  OUTPUT);

  WiFi.mode(WIFI_STA);
  connectToAP();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    // 未連線 -> 待機呼吸燈 + 定時重連
    isConnected = false;
    standbyBreathing();

    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt > 100) {
      lastReconnectAttempt = millis();
      connectToAP();
    }
  } else {
    if (!isConnected) {
      isConnected = true;
      Serial.println("[Client] Now connected to AP!");
    }

    // 定期向中繼站詢問模式
    unsigned long now = millis();
    if (now - lastPollTime > pollInterval) {
      lastPollTime = now;
      fetchCurrentMode();
    }

    // 依據 currentMode 更新LED
    updateLED();
  }
}

/** 嘗試連線到中繼站 AP */
void connectToAP() {
  Serial.printf("[Client] Connecting to AP: %s\n", AP_SSID);
  WiFi.begin(AP_SSID, AP_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start < 5000)) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[Client] Connected!");
    Serial.print("[Client] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[Client] Connect timeout.");
  }
}

/** 待機呼吸燈 (未連線狀態) */
void standbyBreathing() {
  static long lastTime = 0;
  static int  standbyFade = 0;
  static int  standbyStep = 5;

  long now = millis();
  if (now - lastTime >= 30) {
    lastTime = now;
    standbyFade += standbyStep;
    if (standbyFade >= 255 || standbyFade <= 0) {
      standbyStep = -standbyStep;
    }
  }
  int pwm = map(standbyFade, 0, 255, 0, 1023);
  analogWrite(RED_PIN,   0);
  analogWrite(GREEN_PIN, 0);
  analogWrite(BLUE_PIN,  pwm);
}

/** 向中繼站 GET /mode，解析並更新 currentMode, R, G, B */
void fetchCurrentMode() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  if (!client.connect(intermediateIP, 80)) {
    Serial.println("[Client] Can't connect to Intermediate for /mode");
    return;
  }

  // GET /mode
  client.print(String("GET /mode HTTP/1.1\r\n") +
               "Host: 192.168.4.1\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long start = millis();
  while (!client.available()) {
    if (millis() - start > 3000) {
      Serial.println("[Client] Timeout waiting /mode response");
      client.stop();
      return;
    }
  }

  String response;
  while (client.available()) {
    response += client.readStringUntil('\n');
  }
  client.stop();

  // 簡單分段取JSON
  int jsonStart = response.indexOf("{");
  int jsonEnd   = response.lastIndexOf("}");
  if (jsonStart < 0 || jsonEnd < 0) {
    Serial.println("[Client] Invalid JSON in response");
    return;
  }
  String jsonStr = response.substring(jsonStart, jsonEnd + 1);

  // 使用 ArduinoJson 解析
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err) {
    Serial.println("[Client] JSON parse error");
    return;
  }

  if (doc.containsKey("mode")) {
    currentMode = doc["mode"].as<String>();
  }
  if (doc.containsKey("r")) {
    currentR = doc["r"];
  }
  if (doc.containsKey("g")) {
    currentG = doc["g"];
  }
  if (doc.containsKey("b")) {
    currentB = doc["b"];
  }
  Serial.printf("[Client] /mode => mode=%s, R=%d, G=%d, B=%d\n",
                currentMode.c_str(), currentR, currentG, currentB);
}

/** 根據 currentMode 驅動 LED */
void updateLED() {
  if (currentMode == "off") {
    analogWrite(RED_PIN,   0);
    analogWrite(GREEN_PIN, 0);
    analogWrite(BLUE_PIN,  0);
  }
  else if (currentMode == "color") {
    analogWrite(RED_PIN,   map(currentR, 0, 255, 0, 1023));
    analogWrite(GREEN_PIN, map(currentG, 0, 255, 0, 1023));
    analogWrite(BLUE_PIN,  map(currentB, 0, 255, 0, 1023));
  }
  else if (currentMode == "blink") {
    long now = millis();
    if (now - lastBlinkTime >= blinkInterval) {
      lastBlinkTime = now;
      isBlinkOn = !isBlinkOn;
    }
    if (isBlinkOn) {
      analogWrite(RED_PIN,   map(currentR, 0, 255, 0, 1023));
      analogWrite(GREEN_PIN, map(currentG, 0, 255, 0, 1023));
      analogWrite(BLUE_PIN,  map(currentB, 0, 255, 0, 1023));
    } else {
      analogWrite(RED_PIN,   0);
      analogWrite(GREEN_PIN, 0);
      analogWrite(BLUE_PIN,  0);
    }
  }
  else if (currentMode == "fade") {
    long now = millis();
    if (now - lastFadeTime >= fadeInterval) {
      lastFadeTime = now;
      fadeValue += fadeStep;
      if (fadeValue >= 255 || fadeValue <= 0) {
        fadeStep = -fadeStep;
      }
    }
    int pwm = map(fadeValue, 0, 255, 0, 1023);
    // 簡化: R/G/B 同步呼吸, 也可依 currentR/G/B 做不同比率
    int pwmR = min(pwm, (int)map(currentR, 0, 255, 0, 1023));
    int pwmG = min(pwm, (int)map(currentG, 0, 255, 0, 1023));
    int pwmB = min(pwm, (int)map(currentB, 0, 255, 0, 1023));

    analogWrite(RED_PIN,   pwmR);
    analogWrite(GREEN_PIN, pwmG);
    analogWrite(BLUE_PIN,  pwmB);
  }
  // ... 其他模式可擴充
}
