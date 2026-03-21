#pragma once

/**
 * @file Logger.h
 * @brief GPSログ出力
 *
 * 1分周期でGPSデータを生データ形式でログ出力する。
 */

#include "M5GPS.h"

/**
 * @brief ログ機能の初期化
 *
 * GPSログ出力に必要な初期設定を行う。
 *
 */
void initLogger();

/**
 * @brief ログ更新
 * @param gps GPSデータ
 *
 * 60秒周期で位置・速度・高度などをシリアル出力する。
 */
void updateLogger();
