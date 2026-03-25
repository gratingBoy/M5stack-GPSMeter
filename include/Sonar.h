#pragma once

/**
 * @file Sonar.h
 * @brief ソナーエフェクト描画
 *
 * 画面上にレーダー風のアニメーションを描画する。
 */

// =================================
// 状態フラグ
// =================================
extern bool sonarActive;        ///< ソナー有効状態
extern volatile bool sonarTrig; ///< トリガーフラグ
extern volatile int sonarR; ///< トリガーフラグ

// =================================
// API
// =================================
/**
 * @brief ソナーエフェクト描画
 *
 * 呼び出すと現在の状態に応じてリングアニメーションを描画する。
 */
void drawSonarEffect();