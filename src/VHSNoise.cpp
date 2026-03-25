#include "VHSNoise.h"
#include "M5GPS.h"
#include "App.h"

volatile bool vhsNoiseTrig = false;

/**
 * @brief VHS風ノイズ描画
 *
 * 横方向のジッターとランダムノイズを描画し、
 * 走査線のようなエフェクトを生成する。
 */
void drawVHSNoise()
{
    static int y = -1;

    // トリガ待ち
    if (!vhsNoiseTrig && y < 0)
    {
        return;
    }

    // 開始
    if (vhsNoiseTrig)
    {
        y = 0;
        vhsNoiseTrig = false;
    }
    int bandH = 12;
    for (int i = 0; i < bandH; i++)
    {
        int yy = y + i;
        if (yy >= SCR_H)
        {
            continue;
        }
        for (int x = 0; x < SCR_W; x += 4)
        {
            // ノイズ+ズレ
            int offset = random(-2, 3);
            uint16_t col = (random(0, 100) < 50) ? TFT_WHITE : currentColor.grid;
            canvas.drawFastHLine(x + offset, yy, 4, col);
        }
    }
    y += 8;
    if (y > SCR_H)
    {
        y = -1;
    }
}