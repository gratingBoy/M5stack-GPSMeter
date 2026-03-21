#include <M5Unified.h>
#include <TinyGPS++.h>
#include <SD.h>
#include <SPI.h>

#include "App.h"
#include "Logger.h"

// =================================
// グローバルオブジェクト
// =================================

// GPS用UART（Serial2使用）
HardwareSerial GPSRaw(2);

// GPSデータパーサ
TinyGPSPlus gps;

// 描画用キャンバス（ダブルバッファ）
M5Canvas canvas(&M5.Display);

// =================================
// ログ関連
// =================================

File gpsLogFile;           // ログファイル
unsigned long lastLogTime; // 最終ログ時刻
String nmeaLine;           // 受信中NMEA1行バッファ
int currentLogDate;        // 日付（ファイル切替用）

// =================================
// ソナー状態
// =================================

volatile int sonarR = -1;        // ソナー半径（割り込み/タスク共有）
volatile bool sonarTrig = false; // サウンドトリガ
bool sonarActive = false;        // エフェクト有効フラグ

// =================================
// アプリ状態
// =================================

AppState state;

// 外部タスク（Sonar.cpp）
extern void soundTask(void *pvParameters);

// =================================
// 初期化
// =================================
void setup()
{
    // M5初期化
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.setRotation(1);

    // スピーカー音量設定
    M5.Speaker.setVolume(MAX_SP_VOL);

    // GPS UART初期化
    GPSRaw.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    // 描画キャンバス生成
    canvas.createSprite(SCR_W, SCR_H);

    // SDログ初期化
    initLogger();

    // ソナー音タスク起動（別コア実行）
    xTaskCreateUniversal(soundTask, "soundTask", 4096, NULL, 1, NULL, 0);
}

// =================================
// メインループ
// =================================
void loop()
{
    // ボタン・内部状態更新
    M5.update();

    // 入力処理（モード切替など）
    updateInput(state);

    // GPS受信＋ログ処理
    updateLogger();

    // UI描画
    render(state);

    // CPU負荷軽減
    delay(MAIN_LOOP_DELAY);
}