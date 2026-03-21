#include "App.h"
#include "Sonar.h"

// 度→ラジアン変換
static float deg2rad(float deg) { return deg * M_PI / 180.0f; }

// =================================
// 角度計算（統一）
// =================================
// valをrangeに対する割合で角度（度）へ変換し、offsetを加算
static float calcAngle(float val, float range, float offset)
{
    return ((val / range) * 360.0f) + offset;
}

// =================================
// 針
// =================================
// メーターの針を描画
// width > 1 の場合は三角形で太い針を描画
static void drawNeedle(float val, float range, int len, uint16_t color, int width, float offset)
{
    float rad = deg2rad(calcAngle(val, range, offset));

    int tx = CTR_X + len * cos(rad);
    int ty = CTR_Y + len * sin(rad);

    if (width > 1)
    {
        // 太い針（扇形）
        int xo = width * cos(rad + M_PI / 2);
        int yo = width * sin(rad + M_PI / 2);

        canvas.fillTriangle(
            CTR_X + xo, CTR_Y + yo,
            CTR_X - xo, CTR_Y - yo,
            tx, ty, color);
    }
    else
    {
        // 細い針
        canvas.drawLine(CTR_X, CTR_Y, tx, ty, color);
        canvas.fillCircle(tx, ty, 2, color);
    }
}

// =================================
// UI（中央数値）
// =================================
// 背景・グリッド・外枠・中央数値と単位を描画
static void drawCommonUI(const char *val, const char *unit)
{
    canvas.fillSprite(COL_BG);

    // =========================
    // グリッド（同心円）
    // =========================
    for (int i = 1; i <= 4; i++)
        canvas.drawCircle(CTR_X, CTR_Y, GAUGE_R * i / 4, COL_GRID);

    // 放射線（30度ごと）
    for (int d = 0; d < 360; d += 30)
    {
        float r = deg2rad(d);
        canvas.drawLine(
            CTR_X, CTR_Y,
            CTR_X + GAUGE_R * cos(r),
            CTR_Y + GAUGE_R * sin(r),
            COL_GRID);
    }

    // 外枠（影＋本体）
    canvas.drawCircle(CTR_X, CTR_Y, GAUGE_R + 2, COL_BZ_D);
    canvas.drawCircle(CTR_X, CTR_Y, GAUGE_R, COL_MAIN);

    canvas.setTextDatum(MC_DATUM);

    if (val != nullptr)
    {
        // 数値のY位置（少し下げて中央寄り）
        int y = CTR_Y + 5;

        canvas.setFont(&fonts::Font4);

        // 影（視認性向上）
        canvas.setTextColor(COL_GRID);
        canvas.drawString(val, CTR_X + 2, y + 2);

        // 本体（白）
        canvas.setTextColor(TFT_WHITE);
        canvas.drawString(val, CTR_X, y);

        // 単位（白・下側）
        canvas.setFont(&fonts::Font4);
        canvas.setTextColor(TFT_WHITE);
        canvas.drawString(unit, CTR_X, CTR_Y + 45);
    }
}

// =================================
// 目盛り（共通）
// =================================
// 任意スケールの目盛り数値を描画
static void drawDialNumbers(int maxVal, int step, float offset)
{
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(COL_MAIN);

    for (int v = 0; v <= maxVal; v += step)
    {
        float rad = deg2rad(calcAngle(v, maxVal, offset));

        int r = GAUGE_R - 25;

        int x = CTR_X + r * cos(rad);
        int y = CTR_Y + r * sin(rad);

        char buf[8];
        sprintf(buf, "%d", v);

        canvas.drawString(buf, x, y);
    }
}

// ALT専用（0〜9表示）
static void drawDialNumbersAlt()
{
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(COL_MAIN);

    for (int v = 0; v <= 9; v++)
    {
        float rad = deg2rad(calcAngle(v, 10, -90.0f));

        int r = GAUGE_R - 25;

        int x = CTR_X + r * cos(rad);
        int y = CTR_Y + r * sin(rad);

        char buf[4];
        sprintf(buf, "%d", v);

        canvas.drawString(buf, x, y);
    }
}

// =================================
// 入力
// =================================
// ボタン入力で表示モードを切り替え＋ソナー起動
void updateInput(AppState &s)
{
    if (M5.BtnA.wasPressed())
    {
        s.displayMode = MODE_ALT;
        sonarActive = true;
        sonarTrig = true;
    }
    if (M5.BtnB.wasPressed())
    {
        s.displayMode = MODE_CLK;
        sonarActive = true;
        sonarTrig = true;
    }
    if (M5.BtnC.wasPressed())
    {
        s.displayMode = MODE_SPD;
        sonarActive = true;
        sonarTrig = true;
    }
}

// =================================
// ALT
// =================================
// 高度計表示（2針：10000m・1000m）
static void renderALT()
{
    char buf[16];
    sprintf(buf, "%d", (int)gps.altitude.meters());

    drawCommonUI(buf, "m");
    drawDialNumbersAlt();

    if (gps.altitude.isValid())
    {
        float alt = gps.altitude.meters();

        drawNeedle(fmod(alt, 10000), 10000, GAUGE_R - 40, COL_MAIN, 5, -90.0f);
        drawNeedle(fmod(alt, 1000), 1000, GAUGE_R - 10, COL_SUB, 3, -90.0f);
    }
}

// =================================
// CLK
// =================================
// 時計表示（時・分・秒針）
static void renderCLK()
{
    auto dt = M5.Rtc.getDateTime();

    char buf[16];
    sprintf(buf, "%02d:%02d", dt.time.hours, dt.time.minutes);

    drawCommonUI(buf, "");

    drawDialNumbers(11, 1, -90.0f);

    drawNeedle(dt.time.hours, 12, GAUGE_R - 45, COL_MAIN, 6, -90.0f);
    drawNeedle(dt.time.minutes, 60, GAUGE_R - 20, COL_MAIN, 3, -90.0f);
    drawNeedle(dt.time.seconds, 60, GAUGE_R - 10, COL_SUB, 1, -90.0f);
}

// =================================
// SPD（最終仕様）
// =================================
// 速度計（0〜250km/h、12時=0、時計回り）
static void renderSPD()
{
    float spd = gps.speed.kmph();

    char buf[16];
    sprintf(buf, "%.1f", spd);

    drawCommonUI(buf, "km/h");

    // 目盛り（250は0と重複するため非表示）
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(COL_MAIN);

    for (int v = 0; v < 250; v += 50)
    {
        float rad = deg2rad(calcAngle(v, 250, -90.0f));

        int r = GAUGE_R - 25;

        int x = CTR_X + r * cos(rad);
        int y = CTR_Y + r * sin(rad);

        char t[8];
        sprintf(t, "%d", v);

        canvas.drawString(t, x, y);
    }

    // 針（太め）
    if (gps.speed.isValid())
    {
        drawNeedle(spd, 250, GAUGE_R - 15, COL_SUB, 6, -90.0f);
    }
}

// =================================
// 描画
// =================================
// モードに応じて画面描画＋ソナーエフェクト
void render(AppState &s)
{
    if (!gps.location.isValid())
    {
        drawCommonUI("NO SIGNAL", "");
    }
    else
    {
        switch (s.displayMode)
        {
        case MODE_ALT:
            renderALT();
            break;
        case MODE_CLK:
            renderCLK();
            break;
        case MODE_SPD:
            renderSPD();
            break;
        }
    }

    drawSonarEffect();
    canvas.pushSprite(0, 0);
}