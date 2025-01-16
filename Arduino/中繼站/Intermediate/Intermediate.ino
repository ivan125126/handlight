/******************************************************
 * ESP8266 Intermediate (中繼站) - Keep-Alive 範例
 ******************************************************/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

// ------ 區域 ID ------
#define ZONE_ID 0  

// ------ STA: 連到家裡路由器 ------
const char* STA_SSID     = "superfan";
const char* STA_PASSWORD = "20031114";

// ------ AP: 提供給 Client 連線 ------
const char* AP_SSID     = "Zone_0_AP";   
const char* AP_PASSWORD = "12345678";    

// ------ Node.js 伺服器 ------
const char* HOST        = "192.168.137.1"; // 你的電腦 IP
const int   HOST_PORT   = 3000;            // Node.js 埠號

// ------ 定期抓取區域模式 ------
unsigned long lastFetchTime = 0;
unsigned long fetchInterval = 100; // 每 1 秒抓一次

// ------ HTTP Server (port 80) for AP ------
WiFiServer apServer(80);

// ------ Node Client for Keep-Alive ------
WiFiClient nodeClient;  // 用來跟 Node.js 保持連線

// ------ 當前模式變數 ------
String currentMode = "off";
int currentR = 255, currentG = 255, currentB = 255;

void setup() {
  Serial.begin(115200);
  Serial.println("\n[Intermediate] Start");

  // 同時啟用 AP + STA
  WiFi.mode(WIFI_AP_STA);

  // 1) STA 端 - 連路由器
  WiFi.begin(STA_SSID, STA_PASSWORD);
  Serial.print("[Intermediate] Connecting to STA WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[Intermediate] STA connected!");
  Serial.print("[Intermediate] STA IP: ");
  Serial.println(WiFi.localIP());

  // 2) AP 端 - 啟用 AP
  bool apRes = WiFi.softAP(AP_SSID, AP_PASSWORD);
  if (apRes) {
    Serial.print("[Intermediate] AP started! SSID=");
    Serial.println(AP_SSID);
    Serial.print("[Intermediate] AP IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("[Intermediate] AP start failed!");
  }

  // 3) 啟動 AP 下的 HTTP Server
  apServer.begin();
  Serial.println("[Intermediate] AP HTTP Server started (port 80)");
}

/**
 * 確保與 Node.js 的連線處於已連線(connected)狀態，若斷線則嘗試重新連線
 */
bool ensureNodeClientConnected() {
  if (nodeClient.connected()) {
    // 已連線
    return true;
  }
  // 若尚未連線或斷線，就嘗試重新連線
  Serial.println("[Intermediate] Connecting to Node.js with keep-alive...");
  if (!nodeClient.connect(HOST, HOST_PORT)) {
    Serial.println("[Intermediate] Node.js connect failed!");
    return false;
  }
  Serial.println("[Intermediate] Node.js connected (Keep-Alive)!");
  return true;
}

/**
 * 向 Node.js 伺服器抓取 /api/zones/:ZONE_ID (Keep-Alive)
 */
void fetchZoneMode() {
  if (!ensureNodeClientConnected()) {
    return;
  }

  // 構造 HTTP 請求
  // 一定要帶 "Connection: keep-alive"
  String url = "/api/zones/" + String(ZONE_ID);
  nodeClient.print(String("GET ") + url + " HTTP/1.1\r\n" +
                   "Host: " + HOST + "\r\n" +
                   "Connection: keep-alive\r\n" +  // 關鍵
                   "\r\n");

  // 讀取回應 (簡易做法: 讀到空行後繼續讀, 最後找到 JSON)
  unsigned long start = millis();
  String response;
  while (millis() - start < 2000) {
    while (nodeClient.available()) {
      char c = nodeClient.read();
      response += c;
    }
    if (response.indexOf("\r\n\r\n") != -1) {
      // 收到 HTTP Header 結束 (空行)
      // 可能還有 Body, 再稍等一下
      delay(50); 
      // 再把剩下的資料讀進來
      while (nodeClient.available()) {
        char c = nodeClient.read();
        response += c;
      }
      break;
    }
  }

  // 如果完全沒收到任何資料, 可能斷線
  if (response.length() == 0) {
    Serial.println("[Intermediate] fetchZoneMode got empty response, forcing stop()");
    nodeClient.stop();
    return;
  }

  // 在 response 中找到 JSON
  int jsonStart = response.indexOf("{");
  int jsonEnd   = response.lastIndexOf("}");
  if (jsonStart < 0 || jsonEnd < 0) {
    Serial.println("[Intermediate] No JSON found in Node.js response");
    // 可能伺服器沒回 Content-Length?
    // 若要更嚴謹，需要進一步解析HTTP Header
    return;
  }
  String json = response.substring(jsonStart, jsonEnd + 1);

  // 簡易解析（可用 ArduinoJson）
  parseZoneJson(json);
}

/** 解析 {"id":0,"mode":"color","r":100,"g":50,"b":255} */
void parseZoneJson(const String &json) {
  // 以下是最簡單 indexOf 方式
  // 建議用 ArduinoJson
  int mPos = json.indexOf("\"mode\":\"");
  if (mPos >= 0) {
    mPos += 8;
    int mEnd = json.indexOf("\"", mPos);
    currentMode = json.substring(mPos, mEnd);
  }
  int rPos = json.indexOf("\"r\":");
  if (rPos >= 0) {
    rPos += 4;
    int rEnd = json.indexOf(",", rPos);
    currentR = json.substring(rPos, rEnd).toInt();
  }
  int gPos = json.indexOf("\"g\":");
  if (gPos >= 0) {
    gPos += 4;
    int gEnd = json.indexOf(",", gPos);
    currentG = json.substring(gPos, gEnd).toInt();
  }
  int bPos = json.indexOf("\"b\":");
  if (bPos >= 0) {
    bPos += 4;
    int bEnd = json.indexOf("}", bPos);
    currentB = json.substring(bPos, bEnd).toInt();
  }

  Serial.printf("[Intermediate] => mode=%s, R=%d, G=%d, B=%d\n",
                currentMode.c_str(), currentR, currentG, currentB);
}

void loop() {
  // (A) 定期抓取模式 (與 Node.js 保持 Keep-Alive)
  unsigned long now = millis();
  if (now - lastFetchTime > fetchInterval) {
    lastFetchTime = now;
    fetchZoneMode();
  }

  // (B) 處理 AP 下的客戶端 HTTP 請求 (有可能也要 Keep-Alive)
  WiFiClient client = apServer.available();
  if (client) {
    // 讀取 HTTP 請求的一行(或多行)
    String request = readRequest(client);

    // 只示範 /mode
    if (request.indexOf("GET /mode") >= 0) {
      sendModeResponse(client);
      // 不呼叫 client.stop()，嘗試維持 Keep-Alive
      // 但若要保證多次請求，需要更完整的協議實作
    } else {
      client.println("HTTP/1.1 404 Not Found");
      client.println("Connection: close");
      client.println();
      // 這裡可以選擇 keep-alive 或 close
      client.stop();
    }
  }
}

/** 讀取一個基本的 HTTP Request (直到空行或逾時) */
String readRequest(WiFiClient &client) {
  unsigned long start = millis();
  String request;
  while (millis() - start < 2000) {
    while (client.available()) {
      char c = client.read();
      request += c;
      // 如果讀到空行 (HTTP header 結束)
      if (request.indexOf("\r\n\r\n") != -1) {
        return request;
      }
    }
  }
  return request;
}

/** 回傳當前模式的 JSON，示範 Keep-Alive 回應 */
void sendModeResponse(WiFiClient &client) {
  // HTTP Header
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: keep-alive");  // 關鍵：宣告繼續使用連線
  client.println();

  // 回傳 JSON
  String json = "{\"mode\":\"" + currentMode + "\"," +
                "\"r\":" + String(currentR) + "," +
                "\"g\":" + String(currentG) + "," +
                "\"b\":" + String(currentB) + "}";
  client.print(json);

  // 不呼叫 client.stop()，暫時讓連線保持
  // 若之後要處理多次請求，就要在 loop() 中持續偵測同一個 WiFiClient 是否又發送新請求
}

