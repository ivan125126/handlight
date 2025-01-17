/******************************************************
 * ESP8266 Client (Lamp) - Keep-Alive + AP 模式 範例
 ******************************************************/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h> // 開本地 WebServer
#include <ArduinoJson.h>      // 建議使用 ArduinoJson (v6)

// ------ 中繼站 AP 資訊 ------
const char* AP_SSID     = "Zone_7_AP";   
const char* AP_PASSWORD = "12345678";

// ------ 中繼站 AP IP (預設 192.168.4.1) ------
IPAddress intermediateIP(192,168,4,1); 
const int INTERMEDIATE_PORT = 80;

// ------ 本裝置同時也開啟 AP ------
const char* LAMP_AP_SSID     = "YoMama";    // 這是給使用者連的 AP 名稱
const char* LAMP_AP_PASSWORD = "";    // AP 密碼 (可自行更改，也可空字串)

// ------ 建立本地 WebServer 用於 AP 模式 ------
ESP8266WebServer localServer(80);

// ------ RGB LED 接腳 ------
const int RED_PIN   = D1;
const int GREEN_PIN = D2;
const int BLUE_PIN  = D3;

// ------ 模式參數 ------
String currentMode = "fade";
int currentR = 127, currentG = 127, currentB = 127;

// ------ Keep-Alive 用 ------
WiFiClient keepAliveClient;  // 維持和中繼站的連線

// ------ 定期詢問中繼站 /mode ------
unsigned long lastPollTime = 0;
unsigned long pollInterval = 1000; // 每 1 秒詢問一次

// ------ Blink ------
bool  isBlinkOn = false;
long  lastBlinkTime = 0;
int   blinkInterval = 500; // ms

// ------ Fade (呼吸) ------
int   fadeValue = 0;
int   fadeStep  = 50;
long  lastFadeTime = 0;
int   fadeInterval = 3;

// -------------------------------------------------------
//  初始化
// -------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("\n[Client] Start");

  // 同時啟用 AP + Station 模式
  WiFi.mode(WIFI_AP_STA);

  // 啟用本裝置 AP
  startLocalAP();

  // 嘗試連線到指定的中繼站 (Station)
  connectToAP();

  // 在 AP 狀態下跑一個簡易的 WebServer
  setupLocalServer();
}

// -------------------------------------------------------
//  主迴圈
// -------------------------------------------------------
void loop() {
  // 讓 AP 端的 WebServer 持續處理用戶端要求
  localServer.handleClient();

  // 若 AP 斷線，就維持 standby 動畫並定期重試
  if (WiFi.status() != WL_CONNECTED) {
    // 呼吸燈 (表示尚未連到中繼站)
    static unsigned long lastRetry = 0;
    if (millis() - lastRetry > 5000) {
      lastRetry = millis();
      connectToAP(); // 再嘗試連線中繼站
    }
  } 
  else {
    // 已連上中繼站，定期向中繼站詢問 mode
    unsigned long now = millis();
    if (now - lastPollTime > pollInterval) {
      lastPollTime = now;
      fetchCurrentMode(); 
    }
  }

  // 根據 currentMode 驅動 LED
  updateLED();
}

// -------------------------------------------------------
//  啟動本裝置 AP
// -------------------------------------------------------
void startLocalAP() {
  IPAddress myAPIP(192, 168, 5, 1);
  IPAddress gateway(192, 168, 5, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(myAPIP, gateway, subnet);

  bool result = WiFi.softAP(LAMP_AP_SSID, LAMP_AP_PASSWORD);
  if (result) {
    Serial.printf("[Client] AP (%s) started. IP: %s\n", 
                  LAMP_AP_SSID, WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("[Client] Failed to start AP!");
  }
}

// -------------------------------------------------------
//  嘗試連線到中繼站 AP
// -------------------------------------------------------
void connectToAP() {
  Serial.printf("[Client] Connecting to AP: %s\n", AP_SSID);
  WiFi.begin(AP_SSID, AP_PASSWORD);

  unsigned long start = millis();
  bool connected = false;
  while (millis() - start < 5000) { // 最多等 5 秒
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      break;
    }
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  
  if (connected) {
    Serial.println("[Client] AP Connected!");
    Serial.print("[Client] IP (Station): ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[Client] Connect timeout.");
  }
}

// -------------------------------------------------------
//  本地 WebServer (AP 模式) 初始化
// -------------------------------------------------------
void setupLocalServer() {
  // 設定根目錄，顯示簡單的 HTML 設定頁面
  localServer.on("/", HTTP_GET, [](){
    String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                  "<title>ESP Lamp Control</title></head><body>"
                  "<h2>ESP Lamp Control</h2>"
                  "<form action='/set' method='GET'>"
                    "<label>Mode: </label>"
                    "<select name='mode'>"
                      "<option value='off'>Off</option>"
                      "<option value='color'>Color</option>"
                      "<option value='blink'>Blink</option>"
                      "<option value='fade'>Fade</option>"
                    "</select><br/><br/>"
                    "<label>R: </label><input type='number' name='r' min='0' max='255' value='0'><br/>"
                    "<label>G: </label><input type='number' name='g' min='0' max='255' value='0'><br/>"
                    "<label>B: </label><input type='number' name='b' min='0' max='255' value='0'><br/><br/>"
                    "<input type='submit' value='Set'>"
                  "</form>"
                  "</body></html>";
    localServer.send(200, "text/html", html);
  });

  // 處理 /set?mode=xxx&r=xxx&g=xxx&b=xxx
  localServer.on("/set", HTTP_GET, [](){
    if (localServer.hasArg("mode")) {
      currentMode = localServer.arg("mode");
    }
    if (localServer.hasArg("r")) {
      currentR = localServer.arg("r").toInt();
    }
    if (localServer.hasArg("g")) {
      currentG = localServer.arg("g").toInt();
    }
    if (localServer.hasArg("b")) {
      currentB = localServer.arg("b").toInt();
    }
    // 確認已接收並回應
    String resp = "Mode set to: " + currentMode +
                  ", R=" + String(currentR) + 
                  ", G=" + String(currentG) + 
                  ", B=" + String(currentB);
    localServer.send(200, "text/plain", resp);
    Serial.println("[AP] " + resp);
  });

  // 未匹配的 URL
  localServer.onNotFound([](){
    localServer.send(404, "text/plain", "Not found");
  });

  // 啟動 WebServer
  localServer.begin();
  Serial.println("[Client] Local WebServer started");
}

// -------------------------------------------------------
//  確保跟中繼站(192.168.4.1:80)保持連線
// -------------------------------------------------------
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

// -------------------------------------------------------
//  向中繼站發送 GET /mode，解析並更新 currentMode / R / G / B
// -------------------------------------------------------
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

  // 讀取回應
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

// -------------------------------------------------------
//  解析中繼站的 JSON (mode / r / g / b)
// -------------------------------------------------------
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

// -------------------------------------------------------
//  根據 currentMode 驅動 RGB LED
// -------------------------------------------------------
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
    int pwmR = (int)map(standbyFade, 0, currentR, 0, 1023);
    int pwmG = (int)map(standbyFade, 0, currentG, 0, 1023);
    int pwmB = (int)map(standbyFade, 0, currentB, 0, 1023);
  analogWrite(RED_PIN,   pwmR);
  analogWrite(GREEN_PIN, pwmG);
  analogWrite(BLUE_PIN,  pwmB);
  }
  // ... 其他模式可擴充
}

// -------------------------------------------------------
//  待機呼吸 (尚未連到中繼站時)
// -------------------------------------------------------
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
