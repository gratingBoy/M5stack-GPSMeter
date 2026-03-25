#include "App.h"
#include "Sonar.h"
#include "VHSNoise.h"

static unsigned long lastSonarTime = 0; // 最後にソナー演出した時刻
static ColorPreset presets[] = {
    // カラープリセット
    {0x7FFA, 0xF800, 0xEFFF, 0x07FF, 0x0000, 0x0210, 0x03EF}, // シアン
    {0x97D2, 0xF800, 0xE7FF, 0x07E0, 0x0000, 0x0200, 0x03E0}, // グリーン
    {0xFCEE, 0xF800, 0xFFDF, 0xFD20, 0x0000, 0x4000, 0xFC00}, // アンバー
};

static int presetIndex = 0;            // カラープリセットのインデックス
ColorPreset currentColor = presets[0]; // カラープリセットにデフォルトを設定

/**
 * @brief 度をラジアンに変換
 * @param deg 度数法の角度
 * @return ラジアン値
 */
static float deg2rad(float deg) { return deg * M_PI / 180.0f; }

/**
 * @brief カラープリセットを次に切り替える
 *
 * 現在のカラープリセットインデックスをインクリメントし、
 * 定義済みプリセット配列の範囲内でループさせながら
 * 次のテーマカラーへ変更する。
 *
 * タッチ入力などから呼び出すことで、
 * UI全体の配色（メイン・サブ・背景・グリッド）を更新する。
 */
void nextColorPreset()
{
    presetIndex = (presetIndex + 1) % (sizeof(presets) / sizeof(presets[0]));
    currentColor = presets[presetIndex];
}

/**
 * @brief 値を角度（度）に変換
 * @param val 現在値
 * @param range 最大値（スケール）
 * @param offset 開始角度オフセット
 * @return 計算された角度（度）
 *
 * valをrangeに対する割合として0〜360度にマッピングし、
 * offsetを加算して最終角度を求める。
 */
static float calcAngle(float val, float range, float offset)
{
    return ((val / range) * 360.0f) + offset;
}

/**
 * @brief メーターの針を描画
 * @param val 現在値
 * @param range 最大値
 * @param len 針の長さ
 * @param color 色
 * @param width 太さ（1で線、それ以上で三角形）
 * @param offset 開始角度オフセット
 *
 * width > 1 の場合は三角形で太い針を描画する。
 */
static void drawNeedle(float val, float range, int len, uint16_t color, int width, float offset)
{
    float rad = deg2rad(calcAngle(val, range, offset));
    int tx = CTR_X + len * cos(rad);
    int ty = CTR_Y + len * sin(rad);
    int xo = width * cos(rad + M_PI / 2);
    int yo = width * sin(rad + M_PI / 2);

    if (width > 1)
    {
        // 太い針（扇形）
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

/**
 * @brief 共通UI（背景・グリッド・中央表示）を描画
 * @param val 表示する値（文字列）
 * @param unit 単位文字列
 *
 * メーター背景、同心円グリッド、放射線、中央数値を描画する。
 */
static void drawCommonUI(const char *val, const char *unit)
{
    float r = 0; // 描画角度
    int y = 0;   // 数値のY位置

    // 背景初期化
    canvas.fillSprite(currentColor.bg);

    // =========================
    // グリッド（同心円）描画
    // =========================
    for (int i = 1; i <= 4; i++)
    {
        canvas.drawCircle(CTR_X, CTR_Y, GAUGE_R * i / 4, currentColor.grid);
    }

    // 放射線（30度ごと）
    for (int d = 0; d < 360; d += 30)
    {
        r = deg2rad(d);
        canvas.drawLine(
            CTR_X, CTR_Y,
            CTR_X + GAUGE_R * cos(r),
            CTR_Y + GAUGE_R * sin(r),
            currentColor.grid);
    }

    // 外枠
    canvas.drawCircle(CTR_X, CTR_Y, GAUGE_R + 2, currentColor.bezel);
    //    canvas.drawCircle(CTR_X, CTR_Y, GAUGE_R, currentColor.grid);
    canvas.setTextDatum(MC_DATUM);

    if (val != nullptr)
    {
        // 数値のY位置（少し下げて中央寄り）
        y = CTR_Y + VALUE_Y_OFFSET;
        // フォントサイズ設定
        canvas.setFont(&fonts::Font4);
        // 影（視認性向上）
        canvas.setTextColor(currentColor.grid);
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

/**
 * @brief 任意スケールの目盛り数値を描画
 * @param maxVal 最大値
 * @param step 表示間隔
 * @param offset 開始角度オフセット
 */
static void drawDialNumbers(int maxVal, int step, float offset)
{
    char buf[8] = {0};
    float rad = 0;
    int r = 0;
    int x = 0;
    int y = 0;

    canvas.setFont(&fonts::Font2);              // フォントサイズ設定
    canvas.setTextColor(currentColor.dialText); // フォントカラー設定

    for (int v = 0; v < maxVal; v += step)
    {
        rad = deg2rad(calcAngle(v, maxVal, offset));
        r = GAUGE_R - 25;
        x = CTR_X + r * cos(rad);
        y = CTR_Y + r * sin(rad);
        sprintf(buf, "%d", v);
        canvas.drawString(buf, x, y);
    }
}

/**
 * @brief 高度計用の目盛り（0〜9）を描画
 *
 * ALTモード専用の簡易スケール。
 */
static void drawDialNumbersAlt()
{
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(currentColor.dialText);

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

/**
 * @brief 入力処理（ボタン操作）
 * @param s アプリ状態
 *
 * ボタンA/B/Cで表示モードを切り替え、
 * 同時にソナーエフェクトをトリガする。
 */
void updateInput(AppState &s)
{
    if (M5.BtnA.wasPressed())
    {
        s.displayMode = MODE_ALT;
        sonarActive = true;
    }
    if (M5.BtnB.wasPressed())
    {
        s.displayMode = MODE_CLK;
        sonarActive = true;
    }
    if (M5.BtnC.wasPressed())
    {
        s.displayMode = MODE_SPD;
        sonarActive = true;
    }
}

/**
 * @brief 高度計画面の描画
 *
 * 2本の針で高度（10000m / 1000m）を表示する。
 */
static void renderALT()
{
    char buf[16];
    static float altSmooth = 0.0f;

    sprintf(buf, "%d", (int)gps.altitude.meters());
    drawCommonUI(buf, "m");
    drawDialNumbersAlt();

    if (gps.altitude.isValid())
    {
        float rawAlt = gps.altitude.meters();
        float diff = fabs(rawAlt - altSmooth); // スムージング（指数移動平均）
        float alpha = (diff > 50.0f) ? 0.2f : 0.03f;

        // 初期ジャンプ防止
        if (altSmooth == 0.0f)
        {
            altSmooth = rawAlt;
        }

        // スムージング
        altSmooth = altSmooth * (1.0f - alpha) + rawAlt * alpha;
        // スムーズ値を使用

        // 長針（メイン）
        drawNeedle(fmod(altSmooth, 10000), 10000, GAUGE_R - 40, currentColor.needleMain, 5, -90.0f);
        // 短針（サブ）
        drawNeedle(fmod(altSmooth, 1000), 1000, GAUGE_R - 10, currentColor.needleSub, 3, -90.0f);
    }
}

/**
 * @brief 時計画面の描画
 *
 * 時・分・秒の3針でアナログ時計を描画する。
 */
static void renderCLK()
{
    auto dt = M5.Rtc.getDateTime();
    char buf[16];

    sprintf(buf, "%02d:%02d", dt.time.hours, dt.time.minutes);
    drawCommonUI(buf, "");
    drawDialNumbers(12, 1, -90.0f);
    drawNeedle(dt.time.hours, 12, GAUGE_R - 45, currentColor.needleMain, 6, -90.0f);
    drawNeedle(dt.time.minutes, 60, GAUGE_R - 20, currentColor.needleMain, 3, -90.0f);
    drawNeedle(dt.time.seconds, 60, GAUGE_R - 10, currentColor.needleSub, 1, -90.0f);
}

/**
 * @brief 速度計画面の描画
 *
 * GPS速度（km/h）を0〜250スケールで表示する。
 */
static void renderSPD()
{
    static float spdSmooth = 0.0f;
    float rawSpd = gps.speed.kmph();
    float diff = fabs(rawSpd - spdSmooth); // スムージング（指数移動平均）
    float alpha = (diff > 10.0f) ? 0.3f : 0.05f;
    char buf[16];

    if (spdSmooth == 0.0f) // 初期化（起動直後のジャンプ防止）
    {
        spdSmooth = rawSpd;
    }
    spdSmooth = spdSmooth * (1.0f - alpha) + rawSpd * alpha;

    sprintf(buf, "%d", (int)(spdSmooth + 0.5f));
    drawCommonUI(buf, "km/h");

    // 目盛り（250は0と重複するため非表示）
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(currentColor.dialText);

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
        drawNeedle(spdSmooth, 250, GAUGE_R - 15, currentColor.needleSub, 6, -90.0f);
    }
}

/**
 * @brief 画面描画のメイン処理
 * @param s アプリ状態
 *
 * モードに応じたUI描画とエフェクト（ソナー・ノイズ）を実行する。
 */
void render(AppState &s)
{
    if (!gps.location.isValid()) // GPS未受信
    {
        if (millis() - lastSonarTime > 5000) // 5秒おきにソナー演出する
        {
            sonarActive = true;       // ソナー演出ON
            sonarR = -1;              // ソナーリスタート
            lastSonarTime = millis(); // ソナー演出を行った時刻を取得
        }
        if (!gps.location.isValid()) // 測位が無効の場合
        {
            drawCommonUI("NO SIGNAL", "");
        }
    }
    else // GPS受信時
    {
        switch (s.displayMode)
        {
        case MODE_ALT: // 高度計モード
            renderALT();
            break;
        case MODE_CLK: // 時計モード
            renderCLK();
            break;
        case MODE_SPD: // 速度計モード
            renderSPD();
            break;
        default: // 上記以外
            renderCLK();
            break;
        }
    }
    drawSonarEffect();
    drawVHSNoise();
    canvas.pushSprite(0, 0);
}
