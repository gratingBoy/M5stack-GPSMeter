#include <M5Unified.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "M5GPS.h"
#include "Sonar.h"
#include "App.h"

extern volatile int sonarR;

/**
 * @brief RGB565カラーを減衰させる
 * @param color 元の色
 * @param br 明るさ係数（0〜255）
 * @return 減衰後の色
 */
static uint16_t fadeCol(uint16_t color, int br)
{
    uint8_t r = (((color >> 11) & 0x1F) * br) >> 8;
    uint8_t g = (((color >> 5) & 0x3F) * br) >> 8;
    uint8_t b = ((color & 0x1F) * br) >> 8;
    return (r << 11) | (g << 5) | b;
}

/**
 * @brief ソナー音再生タスク
 * @param pvParameters タスク引数（未使用）
 *
 * sonarTrigが立った際に減衰音を再生する。
 * FreeRTOS上で常時ループ実行される。
 */
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

/**
 * @brief ソナーエフェクト描画
 *
 * 中心から広がるリング状の波紋を描画し、
 * 減衰しながら消えていく。
 */
void drawSonarEffect()
{
    static float speed = 0; // 拡大速度（減衰）

    if (!sonarActive)
    {
        return;
    }
    if (sonarR < 0) // 初期化
    {
        sonarR = 0;
        speed = 20;
    }

    int r = sonarR;
    sonarTrig = true; // ソナー音再生

    // 中心発光（初期のみ）
    if (r < 10)
    {
        canvas.fillCircle(SCR_X, SCR_Y, 6 - r / 2, fadeCol(currentColor.needleMain, 255 - r * 20));
    }
    // 減衰強度
    int br = 255 * exp(-0.015 * r);
    // 波紋描画
    if (br > 5)
    {
        uint16_t col = fadeCol(currentColor.needleMain, br);

        // 太さを持たせるため複数円描画
        for (int i = -2; i <= 2; i++)
        {
            canvas.drawCircle(SCR_X, SCR_Y, r + i, col);
        }
    }

    // 半径更新（減速しながら拡大）
    sonarR += speed;
    speed *= 0.95;

    if (speed < 2)
    {
        speed = 2;
    }
    // 終了判定
    if (sonarR > SONAR_MAX_R)
    {
        sonarR = -1;
        sonarActive = false;
    }
}