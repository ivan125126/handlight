專案名稱：ESP8266 多區域燈光控制 (Node.js + ESP8266 中繼站 + ESP8266 Client)
專案簡介
本專案實作了一個多階層的燈光控制系統，包含：

Node.js 伺服器

執行在電腦上，維護多個「區域（Zone）」的燈光模式（如：off、color、blink、fade…）。
提供網頁界面（前端）和 REST API 讓中繼站查詢或更新模式。
ESP8266 中繼站（Intermediate）

同時啟用 STA + AP 模式：
STA 端：連接到家中的路由器，並定期向 Node.js 伺服器取得自己所對應區域（Zone ID）的最新燈光模式。
AP 端：建立獨立的 AP（如 Zone_0_AP），對外提供 HTTP 服務。
當有 Client 連線到該 AP 並詢問 /mode 時，中繼站即回傳當前的燈光模式（JSON 格式）。
ESP8266 Client（終端燈具）

連線到對應中繼站 AP（例如 Zone_0_AP）。
若尚未連上中繼站 AP，則持續執行「待機呼吸燈」並週期性嘗試重連。
連上後，透過 HTTP 週期性查詢 /mode，取得最新的燈光模式並驅動 RGB LED。
如此可讓一台 Node.js 伺服器集中管理多個區域（多達 8 個或更多），而每個區域都有一台中繼站負責「拉取模式、對外提供 AP、回應 Client」。每個區域可連多台 Client，達到分區燈光控制的需求。

架構總覽
┌─────────────┐          ┌────────────────┐
│ Node.js     │          │    Home Router │
│(Server & API)◄───WiFi──►(同網段)        │
└─────────────┘          └────────────────┘
        ▲  
        │(STA 模式)
        ▼
┌────────────────────────────┐
│ ESP8266 中繼站 (Intermediate)│
│ - ZONE_ID = 0,1,2...        │
│ - STA: 連到路由器抓取模式    │
│ - AP: 提供 SSID "Zone_X_AP"  │
│ - HTTP Server(port 80)      │
└────────────────────────────┘
          ▲
          │(Client 連線到 AP)
          ▼
     ┌────────────┐
     │ESP8266 Client│
     │ - STA: 連 Zone_X_AP
     │ - 定期 GET /mode
     │ - 待機: 呼吸燈
     │ - 接收模式: off/color/blink/fade
     └────────────┘
安裝與部署
1. Node.js 伺服器端
安裝 Node.js 與 npm

至 Node.js 官方網站 下載並安裝，或透過 nvm、包管理器等方式安裝。
下載或複製本專案程式碼

假設主程式檔為 server.js，並有一個 package.json 來管理依賴套件。
安裝依賴套件
npm install
執行伺服器
npm start
預設伺服器在 http://localhost:3000 執行。
若有對應的前端 index.html，可直接在瀏覽器開啟 http://localhost:3000 查看與測試。
設定伺服器機器的 IP

在專案程式（ESP8266 中繼站端）中，你需要手動指定 Node.js 伺服器所在電腦的 IP（例如 192.168.0.100）及埠號（3000）。
確保此電腦與 ESP8266 都在同一個路由器或網段。
2. ESP8266 中繼站
Zone ID 與 AP SSID

在程式碼中定義 #define ZONE_ID 0（或 1~7 等），並把 AP_SSID 設為 "Zone_0_AP"（或 "Zone_1_AP", ...）。
每一台中繼站都要用 不同的 ZONE_ID 與 AP_SSID 來對應不同區域。
Node.js 伺服器 IP

修改 HOST 和 HOST_PORT 變數，例如：
cpp
複製程式碼
const char* HOST      = "192.168.0.100";  // Node.js 伺服器 IP
const int   HOST_PORT = 3000;
請務必與實際電腦 IP 相符。
Wi-Fi STA 帳密

修改 STA_SSID 與 STA_PASSWORD 為你的家用路由器的 Wi-Fi 名稱與密碼。
編譯與燒錄

使用 Arduino IDE 或 PlatformIO 等工具，將程式上傳至對應的 ESP8266 開發板（如 NodeMCU、Wemos D1 mini）。
確認序列埠與開發板型號設定無誤。
啟動與檢查

中繼站上電後，會自動連線到路由器（STA）並開啟自己的 AP。
透過序列埠監視器（baud rate 115200）可觀察連線結果。
成功後，你會看到相關印出：
scss
複製程式碼
[Intermediate] STA connected!
[Intermediate] AP started! SSID=Zone_0_AP
[Intermediate] HTTP Server started (port 80)
表示此中繼站已就緒，可供 Client 連線。
3. ESP8266 Client（燈具端）
要連接哪個中繼站的 AP？

在程式碼中，設定 AP_SSID = "Zone_0_AP"（或對應區域），以及相同密碼。
預設中繼站 AP IP 為 192.168.4.1，可在程式中修改（若你有透過 softAPConfig 調整 IP）。
RGB LED 腳位

預設範例可能使用 D1、D2、D3 分別對應 R/G/B。請視硬體連線自行調整。
編譯與燒錄

與中繼站相同方式，上傳到另一塊 ESP8266（用於控制實際 RGB LED）。
啟動與檢查

Client 上電後，會嘗試連接 Zone_0_AP。
若連線失敗，將進入「待機模式」並讓 LED 做呼吸閃爍（以藍色或指定顏色），並定時重試。
若連線成功，序列監視器可看到：
css
複製程式碼
[Client] Connected!
[Client] /mode => mode=color, R=120, G=60, B=180
表示成功向中繼站詢問了當前模式，並驅動 RGB LED。
操作範例
設定燈光模式
在電腦上瀏覽 http://localhost:3000
設定燈光模式

在電腦上瀏覽 http://localhost:3000（或伺服器實際埠、IP），進入 Node.js 提供的網頁，看到 8 個區域（Zone）設定介面。
選擇某個區域，如 Zone 0，設定模式（off、color、blink、fade…）與顏色（R/G/B）。
按下「Set」或類似按鈕，即可更新該區的設定。
觀察中繼站接收模式

Zone 0 的中繼站每隔數秒（程式中 fetchInterval）向伺服器抓取 /api/zones/0，取得更新後的模式。
序列監視器會印出新的模式資訊，例如：
css
複製程式碼
[Intermediate] Updated => mode=color, R=100, G=200, B=50
客戶端（終端燈具）同步更新

連到 Zone_0_AP 的 Client 週期性對 GET /mode 進行詢問。
收到的模式若有變更，即更新 RGB LED，實現即時同步。
在序列監視器可看到：
css
複製程式碼
[Client] /mode => mode=color, R=100, G=200, B=50
常見問題 (FAQ)
為什麼 Node.js 伺服器 IP 要指定成 192.168.x.x？

因為 ESP8266 與伺服器需要在同一網段才能互相通訊（透過路由器 / AP ）。要確保你的電腦 IP 與路由器分配的 IP 能對應。
中繼站 AP 為什麼會是 192.168.4.1？

這是 ESP8266 AP 模式的預設網段與閘道 IP。若需要修改，可以使用 WiFi.softAPConfig() 進行自定義。
客戶端沒有連上 AP，怎麼辦？

檢查中繼站 AP SSID 與密碼是否正確。
檢查程式中 AP_SSID、AP_PASSWORD 變數是否與中繼站設定吻合。
觀察序列監視器訊息，可看出是否一直在重試連線。
我要擴充更多模式或參數（如閃爍速度、漸層周期）怎麼做？

在 Node.js 後端 zones[i] 補充對應屬性，如 blinkInterval, fadeSpeed，並在 ESP8266 中繼站與 Client 端程式中一併解析與應用。
我要增加安全性，怎麼辦？

可以考慮在 HTTP 通訊中加上 Token 或簡易身份驗證。
若要使用 HTTPS，需要在 ESP8266 上使用 WiFiClientSecure，但須注意憑證大小與記憶體限制。
版本資訊
Node.js：v14 或以上（測試通過版本）
ESP8266 Arduino Core：2.7.x ~ 3.x
Arduino IDE 或 PlatformIO：測試皆可
硬體：NodeMCU、Wemos D1 mini、或其他 ESP8266 相容開發板
授權條款
本專案範例程式碼僅作為學習與示範用途，若要應用於商業產品或大規模部署，請確保硬體、網路與安全需求都已充分測試與評估。
若需引用或分發，請保留原作者資訊與此 Readme 說明。
後續開發方向
新增更多燈效：如多色循環、音樂節拍閃爍、陣列花樣等。
整合更多感測器：讓 Node.js 伺服器或中繼站可接收感測資訊（溫溼度、亮度…），自動調整燈光。
使用 WebSocket/MQTT：達到更即時的雙向通訊，減少輪詢延遲。
硬體優化：加入 MOSFET / LED driver，以承受更大電流或更多組 LED。
如對本專案有任何疑問或建議，歡迎提出 Issue 或 Pull Request。感謝你的使用與參與，祝開發順利！
