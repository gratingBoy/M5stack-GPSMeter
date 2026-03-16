#ifndef M5GPS_H // includeガード
#define M5GPS_H

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
#define SONAR_F 800         // ソナー音の基本周波数(Hz)
#define SONAR_MAX_R 260     // 波紋が消滅する最大半径
#define SONAR_SPD 12        // 波紋が1フレームに広がるピクセル数
#define AUTO_SONAR_MS 10000 // GPS未受信時の自動ソナー間隔 (ms)

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
#define COL_SUB 0xf800  // サブカラー (赤)
#define COL_BZ_D 0x0210 // 装飾：重厚な濃い青色
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
bool isRTCInitialSynced = false; // 同期管理用フラグ

#endif
