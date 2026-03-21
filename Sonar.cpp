#include <M5Unified.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "M5GPS.h"
#include "Sonar.h"

// =================================
// 色フェード
// =================================
// RGB565カラーを明るさ係数で減衰させる
static uint16_t fadeCol(uint16_t color, int br)
{
    uint8_t r = (((color >> 11) & 0x1F) * br) >> 8;
    uint8_t g = (((color >> 5) & 0x3F) * br) >> 8;
    uint8_t b = ((color & 0x1F) * br) >> 8;
    return (r << 11) | (g << 5) | b;
}

// =================================
// サウンドタスク
// =================================
// ソナートリガ時に減衰音を再生するRTOSタスク
void soundTask(void *pvParameters)
{
    for (;;)
    {
        if (sonarTrig)
        {
            // 減衰しながら音を鳴らす
            for (int i = 0; i < 60; i++)
            {
                int vol = 180 * exp(-0.06 * i);
                M5.Speaker.setVolume(vol);
                M5.Speaker.tone(SONAR_F, 30);
                vTaskDelay(30 / portTICK_PERIOD_MS);
            }

            // トリガリセット
            sonarTrig = false;

            // 音量を元に戻す
            M5.Speaker.setVolume(MAX_SP_VOL);
        }

        // タスク待機
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// =================================
// ソナー描画
// =================================
// 中心から広がる波紋エフェクトを描画
void drawSonarEffect()
{
    static float speed = 0; // 拡大速度（減衰）

    if (!sonarActive)
        return;

    // 初期化
    if (sonarR < 0)
    {
        sonarR = 0;
        speed = 20;
    }

    int r = sonarR;

    // 中心発光（初期のみ）
    if (r < 10)
        canvas.fillCircle(
            CTR_X, CTR_Y,
            6 - r / 2,
            fadeCol(COL_MAIN, 255 - r * 20));

    // 減衰強度
    int br = 255 * exp(-0.015 * r);

    // 波紋描画
    if (br > 5)
    {
        uint16_t col = fadeCol(COL_MAIN, br);

        // 太さを持たせるため複数円描画
        for (int i = -2; i <= 2; i++)
            canvas.drawCircle(CTR_X, CTR_Y, r + i, col);
    }

    // 半径更新（減速しながら拡大）
    sonarR += speed;
    speed *= 0.95;

    if (speed < 2)
        speed = 2;

    // 終了判定
    if (sonarR > SONAR_MAX_R)
    {
        sonarR = -1;
        sonarActive = false;
    }
}