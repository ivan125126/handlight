/*****************************************************
 * server.js - Node.js 後端
 *****************************************************/
const express = require('express');
const path = require('path');
const app = express();
const port = 3000;

/**
 * zones[i] 結構：
 * {
 *   id: number,        // 0 ~ 7
 *   mode: string,      // "off", "color", "blink", "fade", ...
 *   r: number,         // 紅色 (0~255)
 *   g: number,         // 綠色 (0~255)
 *   b: number          // 藍色 (0~255)
 *   // 可擴充：blinkInterval, fadeSpeed, ...
 * }
 */
let zones = [];
for (let i = 0; i < 8; i++) {
  zones.push({
    id: i,
    mode: 'off',
    r: 255,
    g: 255,
    b: 255
  });
}

app.use(express.json());

// 簡易前端，讓你可以在瀏覽器上調整 8 區域的模式
app.use(express.static(path.join(__dirname, 'public')));

/**
 * 取得單一區域
 * GET /api/zones/:id
 */
app.get('/api/zones/:id', (req, res) => {
  const zoneId = parseInt(req.params.id, 10);
  if (isNaN(zoneId) || zoneId < 0 || zoneId >= 8) {
    return res.status(400).json({ error: 'Invalid zone id' });
  }
  res.json(zones[zoneId]);
});

/**
 * 取得所有區域 - 若要測試用
 * GET /api/zones
 */
app.get('/api/zones', (req, res) => {
  res.json(zones);
});

/**
 * 更新某個區域 - 用於前端修改
 * PATCH /api/zones/:id
 */
app.patch('/api/zones/:id', (req, res) => {
  const zoneId = parseInt(req.params.id, 10);
  if (isNaN(zoneId) || zoneId < 0 || zoneId >= 8) {
    return res.status(400).json({ error: 'Invalid zone id' });
  }

  let { mode, r, g, b } = req.body;
  if (mode !== undefined) zones[zoneId].mode = mode;
  if (r !== undefined) zones[zoneId].r = r;
  if (g !== undefined) zones[zoneId].g = g;
  if (b !== undefined) zones[zoneId].b = b;

  res.json(zones[zoneId]);
});

app.listen(port, () => {
  console.log(`Node.js server listening on http://localhost:${port}`);
});
