/******************************************************
 * ESP8266 Intermediate (中繼站) - 範例：Zone 0
 ******************************************************/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

// ------ 區域 ID ------
#define ZONE_ID 0  

// ------ STA: 連到家裡路由器 ------
const char* STA_SSID     = "superfan";
const char* STA_PASSWORD = "20031114";

// ------ AP: 提供給 Client 連線 ------
const char* AP_SSID     = "Zone_0_AP";   // 可依 zone 編號不同
const char* AP_PASSWORD = "12345678";    

// ------ Node.js 伺服器 ------
const char* HOST        = "192.168.137.1"; // 請換成你電腦 IP
const int   HOST_PORT   = 3000;            

// ------ 週期抓取區域模式 ------
unsigned long lastFetchTime = 0;
unsigned long fetchInterval = 200; // 每 2 秒抓一次

// ------ TCP Server (HTTP) for AP ------
WiFiServer server(80);

// ------ 當前模式變數 ------
String currentMode = "off";
int currentR = 255, currentG = 255, currentB = 255;

/** 向 Node.js 伺服器抓取 /api/zones/:ZONE_ID */
String fetchZoneMode(int zoneId) {
  WiFiClient client;
  if (!client.connect(HOST, HOST_PORT)) {
    Serial.println("[Intermediate] Cannot connect to Node.js");
    return "";
  }

  // GET /api/zones/:id
  String url = "/api/zones/" + String(zoneId);
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + HOST + "\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long start = millis();
  while (!client.available()) {
    if (millis() - start > 3000) {
      Serial.println("[Intermediate] Timeout");
      client.stop();
      return "";
    }
  }

  String response;
  while (client.available()) {
    response += client.readStringUntil('\n');
  }
  client.stop();

  // 簡易解析：找到 JSON 部分
  int jsonStart = response.indexOf("{");
  int jsonEnd   = response.lastIndexOf("}");
  if (jsonStart < 0 || jsonEnd < 0) {
    Serial.println("[Intermediate] Invalid JSON in response");
    return "";
  }
  return response.substring(jsonStart, jsonEnd + 1);
}

/** 將 currentMode, currentR, currentG, currentB 設定自 JSON */
void parseZoneJson(const String &json) {
  // 這裡直接用最陽春的 indexOf 方式
  // 建議可用 ArduinoJson
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
  Serial.printf("[Intermediate] Updated => mode=%s, R=%d, G=%d, B=%d\n",
                currentMode.c_str(), currentR, currentG, currentB);
}

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

  // 3) 啟動 HTTP Server (port 80)
  server.begin();
  Serial.println("[Intermediate] HTTP Server started (port 80)");
}

void loop() {
  // (A) 週期向 Node.js 伺服器抓取該 zone 的模式
  unsigned long now = millis();
  if (now - lastFetchTime > fetchInterval) {
    lastFetchTime = now;
    String zoneJson = fetchZoneMode(ZONE_ID);
    if (zoneJson.length() > 0) {
      parseZoneJson(zoneJson);
    }
  }

  // (B) 處理 AP 下客戶端的 HTTP 請求
  WiFiClient client = server.available();
  if (client) {
    // 讀取 HTTP 請求的一行
    Serial.println("[Intermediate] client request");
    String request = client.readStringUntil('\r');
    client.flush();

    // 簡單判斷：若包含 "/mode" 就回傳 JSON
    // 例如：GET /mode HTTP/1.1
    if (request.indexOf("GET /mode") >= 0) {
      // 回應當前模式
      Serial.println("[Intermediate] client request mode");
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println("Connection: close");
      client.println();

      // 準備 JSON，如 {"mode":"color","r":100,"g":50,"b":200}
      String json = "{\"mode\":\"" + currentMode + "\","
                    "\"r\":" + String(currentR) + ","
                    "\"g\":" + String(currentG) + ","
                    "\"b\":" + String(currentB) + "}";

      client.print(json);
      client.stop();
    }
    else {
      // 若需要回傳其他網頁，可在這裡處理
      // 目前僅示範 /mode
      client.println("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
      client.stop();
    }
  }
}
