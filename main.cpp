#include <M5Unified.h> // M5Stack全機種を統合管理するライブラリ
#include <TinyGPS++.h> // GPS信号のNMEAフォーマットを解析するライブラリ

// --- 通信・ハードウェア定義 ---
#define GPS_BAUD 9600 // GPSユニットの標準通信速度
#define GPS_RX_PIN 13 // Core2の受信ピン (DIPスイッチ設定:13)
#define GPS_TX_PIN 14 // Core2の送信ピン (DIPスイッチ設定:14)

// --- 画面レイアウト定義 ---
#define SCR_W 320   // 画面の横幅
#define SCR_H 240   // 画面の縦幅
#define CTR_X 160   // 画面の水平中心
#define CTR_Y 120   // 画面の垂直中心
#define GAUGE_R 105 // メーター文字盤の半径
#define NUM_R 83    // 文字盤数字を配置する半径

// --- ソナー演出定義 ---
#define SONAR_F 850         // ソナー音の基本周波数(Hz)
#define SONAR_MAX_R 260     // 波紋が消滅する最大半径
#define SONAR_SPD 12        // 波紋が1フレームに広がるピクセル数
#define AUTO_SONAR_MS 10000 // GPS未受信時の自動ソナー間隔 (20秒)

// --- モード・速度レンジ定義 ---
#define MODE_ALT 0     // 高度計モード識別子
#define MODE_CLK 1     // 時計モード識別子
#define MODE_SPD 2     // 速度計モード識別子
#define RNG_MIN 20.0f  // 速度計：低速レンジ (徒歩・自転車用)
#define RNG_MID 180.0f // 速度計：中速レンジ (自動車用)
#define RNG_MAX 500.0f // 速度計：高速レンジ (航空機・新幹線用)

// --- カラー定義 (ブルーウォーター液晶スタイル) ---
#define COL_BG 0x0000   // 背景色：完全な黒 (漆黒)
#define COL_GRID 0x0110 // グリッド線：深い紺色
#define COL_MAIN 0x07FF // メインカラー：発光シアン (水色)
#define COL_BZ_D 0x0210 // 装飾：重厚な濃い青色

// その他定義
#define MAX_SP_VOL 100     // スピーカー最大音量
#define MAIN_LOOP_DELAY 10 // メインループのディレイ時間 (ms)
#define BUFF_SIZE 16       // バッファサイズ

// --- グローバルインスタンス ---
HardwareSerial GPSRaw(2);     // ESP32のUART2を使用
TinyGPSPlus gps;              // GPS解析エンジンのインスタンス
M5Canvas canvas(&M5.Display); // 描画用のメモリバッファ (スプライト)

// --- 状態管理変数 ---
int displayMode = MODE_CLK;      // 現在表示中の画面モード
float currentMaxSpd = RNG_MIN;   // 速度計の現在の最大スケール
volatile int sonarR = -1;        // 波紋の現在の半径 (-1は停止中)
volatile bool sonarTrig = false; // 音再生タスクへのトリガーフラグ

// --- 描画補助関数群 ---

/**
 * @brief 指定した色を指定した輝度(0-255)で暗くする
 * @param color 変換前の色(RGB565)
 * @param br 輝度(0:黒 〜 255:元の色)
 * @return 減衰後の色(RGB565)
 */
uint16_t fadeCol(uint16_t color, int br)
{
  uint8_t r = (uint8_t)((((color >> 11) & 0x1F) * br) >> 8); // 赤成分の減衰
  uint8_t g = (uint8_t)((((color >> 5) & 0x3F) * br) >> 8);  // 緑成分の減衰
  uint8_t b = (uint8_t)(((color & 0x1F) * br) >> 8);         // 青成分の減衰
  return (uint16_t)((r << 11) | (g << 5) | b);
}

/**
 * @brief メーターの指針（針）を描画する
 * @param val 現在の値
 * @param range 最大値
 * @param len 針の長さ
 * @param color 針の色
 * @param width 針の根元の幅 (1以下で細い針)
 */
void drawNeedle(float val, float range, int len, uint16_t color, int width)
{
  float angle = (((val / range) * 360.0f) - 90.0f);       // 割合を角度(-90度起点)に変換
  float rad = (angle * M_PI / 180.0f);                    // ラジアンへ変換
  int tX = (int)((float)CTR_X + ((float)len * cos(rad))); // 針の先端X
  int tY = (int)((float)CTR_Y + ((float)len * sin(rad))); // 針の先端Y
  if (width > 1)
  {                                                            // 幅指定がある場合は力強い三角形の針を描画
    int xOff = (int)((float)width * cos(rad + (M_PI / 2.0f))); // 垂直オフセットX
    int yOff = (int)((float)width * sin(rad + (M_PI / 2.0f))); // 垂直オフセットY
    canvas.fillTriangle((CTR_X + xOff), (CTR_Y + yOff), (CTR_X - xOff), (CTR_Y - yOff), tX, tY, color);
  }
  else
  { // 幅1以下の場合は細い線と先端のドットを描画
    canvas.drawLine(CTR_X, CTR_Y, tX, tY, color);
    canvas.fillCircle(tX, tY, 2, color);
  }
}

/**
 * @brief メーターの円周上に数字を配置する
 * @param count 分割数
 * @param stepValue 数値の増分 (0の場合はインデックス番号を表示)
 */
void drawDialNumbers(int count, float stepValue)
{
  canvas.setTextColor(COL_MAIN); // 数字の色を発光ブルーに設定
  for (int i = 0; i < count; i++)
  {                                                                   // 指定された数だけループ
    float angle = (((i * (360.0f / count)) - 90.0f) * M_PI / 180.0f); // 座標計算
    int val = (stepValue == 0) ? i : (int)(i * stepValue);            // 表示する数値の決定
    canvas.drawCenterString(String(val), (int)(CTR_X + NUM_R * cos(angle)), (int)(CTR_Y + NUM_R * sin(angle)), &fonts::Font2);
  }
}

/**
 * @brief 共通UI要素（背景・グリッド・ベゼル・デジタル値）を一括描画する
 * @param label モード名称 (下部)
 * @param val デジタル数値文字列 (中央)
 * @param unit 単位文字列 (中央下)
 */
void drawCommonUI(const char *label, const char *val, const char *unit)
{
  canvas.fillSprite(COL_BG); // 背景を黒で初期化
  for (int i = 1; i <= 4; i++)
  {
    canvas.drawCircle(CTR_X, CTR_Y, (GAUGE_R * i / 4), COL_GRID);
  } // 同心円
  for (int d = 0; d < 360; d += 30)
  { // 30度ごとの放射グリッド
    float rad = (d * M_PI / 180.0f);
    canvas.drawLine(CTR_X, CTR_Y, (int)(CTR_X + GAUGE_R * cos(rad)), (int)(CTR_Y + GAUGE_R * sin(rad)), COL_GRID);
  }
  canvas.drawCircle(CTR_X, CTR_Y, (GAUGE_R + 2), COL_BZ_D);           // 外側ベゼル
  canvas.drawCircle(CTR_X, CTR_Y, GAUGE_R, COL_MAIN);                 // 内側ベゼル
  canvas.drawCenterString(label, CTR_X, (CTR_Y + 45), &fonts::Font2); // 下部のラベル
  if (val != nullptr)
  {                                                                    // 数値が指定されている場合のみ描画
    canvas.drawCenterString(val, CTR_X, (CTR_Y - 5), &fonts::Font4);   // メイン数値
    canvas.drawCenterString(unit, CTR_X, (CTR_Y + 18), &fonts::Font0); // 単位
  }
}

// --- 音声再生タスク ---

/**
 * @brief 音再生専用タスク (Core 0で独立動作)
 * ソナー音をバックグラウンドで再生する
 */
void soundTask(void *pvParameters)
{
  for (;;)
  { // 無限ループ
    if (sonarTrig)
    { // メインループからの発報指示を確認
      for (int i = 0; i < 60; i++)
      {                                                   // 60ステップかけて音量を絞る
        int vol = (int)(180.0f * exp(-0.06f * (float)i)); // 指数関数的な減衰計算
        M5.Speaker.setVolume(vol);                        // 現在の音量を設定
        M5.Speaker.tone(SONAR_F, 30);                     // 850Hzの音を30ms再生
        vTaskDelay(30 / portTICK_PERIOD_MS);              // 再生時間に合わせてスリープ
      }
      sonarTrig = false;                // 再生完了フラグをリセット
      M5.Speaker.setVolume(MAX_SP_VOL); // 他の音のために標準音量に戻す
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); // フラグ監視のための待機
  }
}

// --- メイン描画管理 ---

/**
 * @brief 画面のリフレッシュ処理。GPS情報から各モードの描画を実行する
 */
void refresh()
{
  if (!gps.location.isValid()) // GPSが測位できていない場合
  {
    drawCommonUI("SEARCHING...", "NO SIGNAL", "WAITING FOR GPS"); // 警告画面
    if (sonarR == -1 && ((millis() % AUTO_SONAR_MS) < 20))
    {
      // 20秒おき自動ソナー
      sonarR = 0;
      sonarTrig = true;
    }
  }
  else // 測位完了時
  {
    char strBuf[BUFF_SIZE]; // 文字列変換用バッファ
    if (displayMode == MODE_ALT)
    {                                                    // 高度計モード
      sprintf(strBuf, "%d", (int)gps.altitude.meters()); // 整数に変換
      drawCommonUI("ALTITUDE", strBuf, "METERS");        // ベース描画
      drawDialNumbers(10, 0);                            // 0-9の目盛りを描画
      if (gps.altitude.isValid())                        // 高度が有効なら針を描画
      {
        drawNeedle(fmod((float)gps.altitude.meters(), 10000.0f),
                   10000.0f, (GAUGE_R - 40), COL_BZ_D, 5); // 1万m計
        drawNeedle(fmod((float)gps.altitude.meters(), 1000.0f),
                   1000.0f, (GAUGE_R - 10), COL_MAIN, 1); // 1千m計
      }
    }
    else if (displayMode == MODE_CLK) // 時計モード
    {
      int tz = (int)((gps.location.lng() / 15.0) +
                     (gps.location.lng() >= 0 ? 0.5 : -0.5)); // 簡易時差算出
      int h = (gps.time.hour() + tz);                         // 現地時間計算
      if (h >= 24)
      {
        h -= 24;
      }
      else if (h < 0)
      {
        h += 24;
      } // 24時間補正
      sprintf(strBuf, "%02d:%02d", h, gps.time.minute()); // 時刻文字列
      drawCommonUI("CHRONOMETER", strBuf, "LOCAL TIME");  // ベース描画
      for (int i = 1; i <= 12; i++)
      { // 時計専用の1-12目盛り配置
        float rad = (((i * 30.0f) - 90.0f) * M_PI / 180.0f);
        canvas.drawCenterString(String(i), (int)(CTR_X + NUM_R * cos(rad)), (int)(CTR_Y + NUM_R * sin(rad)), &fonts::Font2);
      }
      if (gps.time.isValid())
      { // 時刻が有効なら針を描画
        drawNeedle(fmod((float)h, 12.0f) + ((float)gps.time.minute() / 60.0f), 12.0f, (GAUGE_R - 45), COL_BZ_D, 6);
        drawNeedle((float)gps.time.minute() + ((float)gps.time.second() / 60.0f), 60.0f, (GAUGE_R - 20), COL_MAIN, 3);
        drawNeedle((float)gps.time.second(), 60.0f, (GAUGE_R - 10), COL_MAIN, 1);
      }
    }
    else // 速度計モード
    {
      float currentSpeed = (float)gps.speed.kmph(); // 時速取得
      // オートレンジロジック: 90%で拡大、30%で縮小
      if (currentSpeed > (currentMaxSpd * 0.9f))
      { // 飛行機用
        currentMaxSpd = (currentMaxSpd < RNG_MID) ? RNG_MID : RNG_MAX;
      }
      // 徒歩用はいらないのでコメントアウト
      // else if (currentSpeed < (currentMaxSpd * 0.3f) && (currentMaxSpd > RNG_MIN))
      // {
      //   currentMaxSpd = (currentMaxSpd > RNG_MID) ? RNG_MID : RNG_MIN;
      // }
      else // 自動車用
      {
        currentMaxSpd = RNG_MAX;
      }
      sprintf(strBuf, "%.1f", currentSpeed);      // 小数点1位まで
      drawCommonUI("SPEED", strBuf, "KM/H");      // ベース描画
      drawDialNumbers(5, (currentMaxSpd / 4.0f)); // 4分割の速度目盛りを描画
      if (gps.speed.isValid())
      { // 速度が有効なら針を描画
        drawNeedle(currentSpeed, currentMaxSpd, (GAUGE_R - 15), COL_MAIN, 1);
      }
    }
  }

  // ソナー波紋演出 (常に描画レイヤーの最前面に重ねる)
  if (sonarR >= 0)
  {
    int br = (255 - ((sonarR * 255) / SONAR_MAX_R)); // 半径が広がるほど透過(暗く)させる
    if (br > 0)
    {
      uint16_t fC = fadeCol(COL_MAIN, br);         // フェードカラー生成
      canvas.drawCircle(CTR_X, CTR_Y, sonarR, fC); // メインリング
      if (sonarR > 10)
      {
        canvas.drawCircle(CTR_X, CTR_Y, (sonarR - 4), fC);
      } // 二重リング演出
    }
    sonarR += SONAR_SPD; // 波紋を拡大
    if (sonarR > SONAR_MAX_R)
    {
      sonarR = -1;
    } // 画面外で停止
  }
  canvas.pushSprite(0, 0); // バッファに描いた画像をLCDへ転送
}

// --- メインエントリ ---
/**
 * @brief プログラム開始位置
 */
void setup()
{
  auto cfg = M5.config();                                     // デバイス設定取得
  M5.begin(cfg);                                              // デバイス初期化
  M5.Speaker.setVolume(MAX_SP_VOL);                           // スピーカー音量を中間値に設定
  GPSRaw.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN); // GPSシリアル通信開始
  canvas.createSprite(SCR_W, SCR_H);                          // 描画バッファ用のメモリを確保
  // 音再生処理をCore 0(サブコア)に割り当てて、描画への影響をゼロにする
  xTaskCreateUniversal(soundTask, "soundTask", 4096, NULL, 1, NULL, 0);
}

/**
 * @brief メインループ
 */
void loop()
{
  M5.update(); // ボタンやタッチの最新状態を確認
  if (M5.BtnA.wasPressed())
  // Aボタン(左)：高度計
  {
    displayMode = MODE_ALT;
  }
  if (M5.BtnB.wasPressed())
  // Bボタン(中)：時計
  {
    displayMode = MODE_CLK;
  }
  if (M5.BtnC.wasPressed())
  // Cボタン(右)：速度計
  {
    displayMode = MODE_SPD;
  }
  auto touch = M5.Touch.getDetail(); // タッチパネル情報の取得
  if (touch.wasPressed())
  { // 画面タッチでセンターからソナー発射
    sonarR = 0;
    sonarTrig = true;
  }
  while (GPSRaw.available() > 0) // 測位が有効の間ループ
  {
    gps.encode(GPSRaw.read()); // GPSパケットの受信と解析
  }
  refresh();              // 画面描画の実行
  delay(MAIN_LOOP_DELAY); // CPUの過負荷防止
}
