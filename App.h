#pragma once

#include <M5Unified.h>
#include <TinyGPSPlus.h>

/**
 * @file App.h
 * @brief アプリケーションのメイン制御
 *
 * UI描画・入力処理・状態管理を担当する。
 * 各モード（ALT / CLK / SPD）の描画を統括する。
 */

// =================================
// 外部依存
// =================================

extern M5Canvas canvas; ///< 描画用スプライト（ダブルバッファ）
extern TinyGPSPlus gps; ///< GPSデータ管理

// =================================
// 定数（UIレイアウト）
// =================================

constexpr int CTR_X = 160;   ///< メーター中心X座標
constexpr int CTR_Y = 120;   ///< メーター中心Y座標
constexpr int GAUGE_R = 100; ///< メーター半径

// =================================
// 色定義
// =================================

constexpr uint16_t COL_BG = 0x0000;   ///< 背景色
constexpr uint16_t COL_GRID = 0x0110; // グリッド線：深い紺色
constexpr uint16_t COL_MAIN = 0x07FF; // メインカラー：発光シアン (水色)
constexpr uint16_t COL_SUB = 0xf800;  // サブカラー (赤)
constexpr uint16_t COL_BZ_D = 0x0210; // 濃い青色

// =================================
// モード定義
// =================================

enum DisplayMode
{
    MODE_ALT, ///< 高度計モード
    MODE_CLK, ///< 時計モード
    MODE_SPD  ///< 速度計モード
};

// =================================
// 状態
// =================================

struct AppState
{
    DisplayMode displayMode; ///< 現在の表示モード
};

// =================================
// API
// =================================

/**
 * @brief ボタン入力の更新
 * @param s アプリ状態
 *
 * ボタンA/B/Cで表示モードを切り替える。
 */
void updateInput(AppState &s);

/**
 * @brief 描画処理
 * @param s アプリ状態
 *
 * 現在のモードに応じて画面描画を行う。
 */
void render(AppState &s);