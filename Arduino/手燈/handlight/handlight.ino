/******************************************************
 * ESP8266 Client (Lamp) - Keep-Alive 範例
 ******************************************************/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>  // 建議使用 ArduinoJson

// ------ 要連到的中繼站 AP ------
const char* AP_SSID     = "Zone_0_AP";   
const char* AP_PASSWORD = "12345678";

// ------ 中繼站 AP IP (預設 192.168.4.1) ------
IPAddress intermediateIP(192,168,4,1); 
const int INTERMEDIATE_PORT = 80;

// ------ RGB LED ------
const int RED_PIN   = D1;
const int GREEN_PIN = D2;
const int BLUE_PIN  = D3;


// ------ 模式參數 ------
String currentMode = "off";
int currentR = 0, currentG = 0, currentB = 0;

// ------ Keep-Alive 用 ------
WiFiClient keepAliveClient;  // 維持和中繼站的連線

// ------ 定期詢問模式 ------
unsigned long lastPollTime = 0;
unsigned long pollInterval = 100; // 每 1 秒詢問一次

// ------ Blink ------
bool  isBlinkOn = false;
long  lastBlinkTime = 0;
int   blinkInterval = 500; // ms

// ------ Fade (呼吸) ------
int   fadeValue = 0;
int   fadeStep  = 50;
long  lastFadeTime = 0;
int   fadeInterval = 3;

// ------ 待機 ------
bool isConnected = false;

void setup() {
  Serial.begin(115200);
  Serial.println("\n[Client] Start");

  WiFi.mode(WIFI_STA);
  connectToAP();
}

/** 嘗試連線到中繼站 AP */
void connectToAP() {
  Serial.printf("[Client] Connecting to AP: %s\n", AP_SSID);
  WiFi.begin(AP_SSID, AP_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start < 500)) {
    delay(100);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[Client] AP Connected!");
    Serial.print("[Client] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[Client] Connect timeout.");
  }
}

void loop() {
  // 若 AP 斷線，就嘗試重連
  if (WiFi.status() != WL_CONNECTED) {
    standbyBreathing();
    static unsigned long lastRetry = 0;
    if (millis() - lastRetry > 5000) {
      lastRetry = millis();
      connectToAP();
    }
    return; 
  }

  // 已連上 AP
  unsigned long now = millis();
  if (now - lastPollTime > pollInterval) {
    lastPollTime = now;
    fetchCurrentMode();
  }
  updateLED();
}

/** 
 * 確保跟中繼站(192.168.4.1:80)保持連線 
 * 若斷線則重新連
 */
bool ensureKeepAliveConnected() {
  if (keepAliveClient.connected()) {
    return true; // 已連線
  }

  Serial.println("[Client] Connecting to Intermediate (Keep-Alive)...");
  if (!keepAliveClient.connect(intermediateIP, INTERMEDIATE_PORT)) {
    Serial.println("[Client] connect failed!");
    return false;
  }
  Serial.println("[Client] keepAliveClient connected!");
  return true;
}

/** 
 * 發送 GET /mode 並讀取回應 (不 stop() 連線) 
 */
void fetchCurrentMode() {
  if (!ensureKeepAliveConnected()) {
    return;
  }

  // 發送 HTTP 請求
  keepAliveClient.print(
    String("GET /mode HTTP/1.1\r\n") +
    "Host: 192.168.4.1\r\n" +
    "Connection: keep-alive\r\n" + // 關鍵：要求保持連線
    "\r\n"
  );

  // 讀取回應 (簡易做法)
  unsigned long start = millis();
  String response;
  while (millis() - start < 2000) {
    while (keepAliveClient.available()) {
      char c = keepAliveClient.read();
      response += c;
    }
    // 偵測到 HTTP header 結束 (空行)
    if (response.indexOf("\r\n\r\n") != -1) {
      // 稍等一下讀 Body
      delay(50);
      while (keepAliveClient.available()) {
        char c = keepAliveClient.read();
        response += c;
      }
      break;
    }
  }

  if (response.length() == 0) {
    // 可能連線被中繼站關了
    Serial.println("[Client] Empty response, forcing stop()");
    keepAliveClient.stop();
    return;
  }

  // 在回應裡找 JSON
  int jsonStart = response.indexOf("{");
  int jsonEnd   = response.lastIndexOf("}");
  if (jsonStart < 0 || jsonEnd < 0) {
    Serial.println("[Client] No JSON found in /mode response");
    return;
  }
  String jsonStr = response.substring(jsonStart, jsonEnd + 1);
  
  // 解析 JSON
  parseModeJson(jsonStr);
}

void parseModeJson(const String &jsonStr) {
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

/** 簡易待機閃爍 (用來表示程式在跑) */
void standbyBlink() {
  static unsigned long lastBlink = 0;
  static bool on = false;
  if (millis() - lastBlink > 500) {
    lastBlink = millis();
    on = !on;
    digitalWrite(LED_BUILTIN, on ? LOW : HIGH); // ESP8266內建LED反向
  }
}
/** 根據 currentMode 驅動 LED */
void updateLED() {
  if (currentMode == "off") {
    analogWrite(RED_PIN,   0);
    analogWrite(GREEN_PIN, 0);
    analogWrite(BLUE_PIN,  0);
  }
  else if (currentMode == "color") {
    analogWrite(RED_PIN,   (int)map(currentR, 0, 255, 0, 1023));
    analogWrite(GREEN_PIN, (int)map(currentG, 0, 255, 0, 1023));
    analogWrite(BLUE_PIN,  (int)map(currentB, 0, 255, 0, 1023));
  }
  else if (currentMode == "blink") {
    long now = millis();
    if (now - lastBlinkTime >= blinkInterval) {
      lastBlinkTime = now;
      isBlinkOn = !isBlinkOn;
    }
    if (isBlinkOn) {
      analogWrite(RED_PIN,   (int)map(currentR, 0, 255, 0, 1023));
      analogWrite(GREEN_PIN, (int)map(currentG, 0, 255, 0, 1023));
      analogWrite(BLUE_PIN,  (int)map(currentB, 0, 255, 0, 1023));
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
/** 待機呼吸燈 (未連線狀態) */
void standbyBreathing() {
  static long lastTime = 0;
  static int  standbyFade = 0;
  static int  standbyStep = 5;

  long now = millis();
  if (now - lastTime >= 30) {
    lastTime = now;
    standbyFade += standbyStep;
    if (standbyFade >= 127 || standbyFade <= 0) {
      standbyStep = -standbyStep;
    }
  }
  int pwm = map(standbyFade, 0, 255, 0, 1023);
  analogWrite(RED_PIN,   pwm);
  analogWrite(GREEN_PIN, pwm);
  analogWrite(BLUE_PIN,  pwm);
}