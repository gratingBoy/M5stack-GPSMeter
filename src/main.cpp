#include <M5Unified.h>
#include <TinyGPS++.h>
#include <SD.h>
#include <SPI.h>
#include "App.h"
#include "Logger.h"
#include "VHSNoise.h"

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
    // 画面角度初期化
    M5.Display.setRotation(1);
    // スピーカー音量設定
    M5.Speaker.setVolume(MAX_SP_VOL);
    // GPS UART初期化
    GPSRaw.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    // 描画キャンバス生成
    canvas.createSprite(SCR_W, SCR_H);
    // SDログ初期化
    initLogger();
    // VHSエフェクトフラグ初期化
    vhsNoiseTrig = true;
    // ソナー音タスク起動（別コア実行）
    xTaskCreateUniversal(soundTask, "soundTask", 4096, NULL, 1, NULL, 0);
}

// =================================
// メインループ
// =================================
void loop()
{
    static bool touched = false; // 画面タッチフラグ
                                 // フレーム間でタッチ状態を保持

    M5.update();        // ボタン・内部状態更新
    updateInput(state); // 入力処理（モード切替など）
    updateLogger();     // GPS受信＋ログ処理
    render(state);      // UI描画

    // -----------------------------
    // タッチ入力処理
    // タップ時のみ1回だけ実行
    // -----------------------------
    if (M5.Touch.getCount())
    {
        auto t = M5.Touch.getDetail();

        if (!touched)
        {
            touched = true;
            // 画面中央部のみ有効
            if ((t.x > 100) && (t.x < 220) && (t.y > 80) && (t.y < 160))
            {
                nextColorPreset(); // カラープリセット変更
            }
            sonarActive = true;
            sonarR = -1;
        }
    }
    else
    {
        // タッチ解除（次のタップ検出準備）
        touched = false;
    }
    delay(MAIN_LOOP_DELAY); // CPU負荷軽減
}