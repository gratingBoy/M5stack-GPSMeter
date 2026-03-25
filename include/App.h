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
// 定数（UIレイアウト）
// =================================
constexpr int CTR_X = 160;         ///< メーター中心X座標
constexpr int CTR_Y = 120;         ///< メーター中心Y座標
constexpr int GAUGE_R = 100;       ///< メーター半径
constexpr int VALUE_Y_OFFSET = 20; // 現在値の縦位置

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
// 表示状態
// =================================
/**
 * @brief アプリケーション状態管理
 *
 * 現在の表示モードなど、UI全体で共有する状態を保持する構造体。
 * メインループや描画処理間で状態を受け渡すために使用する。
 *
 */
struct AppState
{
    DisplayMode displayMode; ///< 現在の表示モード
};

/**
 * @brief UIカラープリセット定義
 *
 * メーター表示に使用する配色セットをまとめた構造体。
 * UIの各要素ごとに色を分離することで、
 * 視認性とデザイン性を両立しつつテーマ切り替えを容易にする。
 *
 * 【設計方針】
 * - 針は視認性優先（高コントラスト・明るめ）
 * - 文字はテーマカラーに追従
 * - 目盛り・ベゼルはやや抑えた色で階層を作る
 *
 * 【各フィールドの役割】
 * - needleMain : メイン針（長針・主要指標）
 * - needleSub  : サブ針（短針・秒針など）
 * - text       : 中央の数値表示
 * - dialText   : 目盛り・ベゼル上の数値
 * - bg         : 背景色
 * - grid       : グリッド線・補助線
 * - bezel      : 外枠・リング
 *
 * ※ 色はRGB565形式（uint16_t）で指定する
 */
struct ColorPreset
{
    uint16_t needleMain; ///< メイン針
    uint16_t needleSub;  ///< サブ針
    uint16_t text;       ///< 中央数値
    uint16_t dialText;   ///< 目盛り文字（ベゼル）
    uint16_t bg;         ///< 背景
    uint16_t grid;       ///< グリッド
    uint16_t bezel;      ///< 外枠
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

/**
 * @brief カラープリセットを次に切り替える
 *
 */
void nextColorPreset();

// =================================
// 外部依存
// =================================
extern M5Canvas canvas;          ///< 描画用スプライト（ダブルバッファ）
extern TinyGPSPlus gps;          ///< GPSデータ管理
extern ColorPreset currentColor; ///< カラープリセット管理
