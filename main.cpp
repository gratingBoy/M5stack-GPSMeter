#include "M5GPS.h"

// --- 描画補助関数群 ---

/**
 * @brief 指定した色を指定した輝度(0-255)で暗くする
 * @param color 変換前の色(RGB565)
 * @param br 輝度(0:黒 〜 255:元の色)
 * @return 減衰後の色(RGB565)
 */
uint16_t fadeCol(uint16_t color, int br)
{
    uint8_t r = (uint8_t)((((color >> 11) & 0x1F) * br) >> 8); // 赤成分の減衰
    uint8_t g = (uint8_t)((((color >> 5) & 0x3F) * br) >> 8);  // 緑成分の減衰
    uint8_t b = (uint8_t)(((color & 0x1F) * br) >> 8);         // 青成分の減衰
    return (uint16_t)((r << 11) | (g << 5) | b);
}

/**
 * @brief 経度からタイムゾーンを推定し、RTCを同期する関数
 * 地球360度を24時間で分割（15度＝1時間）してオフセットを算出する
 */
void syncRTCWithAutoTZ()
{
    // GPSの各データ（位置・日付・時刻）がすべて有効か確認
    if ((!gps.location.isValid()) || (!gps.date.isValid()) || (!gps.time.isValid()))
    {
        return;
    }

    // --- タイムゾーン計算 ---
    // 経度(lng)を15.0で割り、四捨五入することで最も近い時間帯の時差を求める
    // 例: 日本(135.0度) / 15.0 = +9時間
    double lng = gps.location.lng();
    int tzOffset = round(lng / 15.0);

    // RTC設定用の構造体を宣言
    m5::rtc_datetime_t dt;

    // GPS(UTC)から基本情報を取得
    int year = gps.date.year();
    int month = gps.date.month();
    int day = gps.date.day();
    int hour = gps.time.hour() + tzOffset; // 時差を加算
    int min = gps.time.minute();
    int sec = gps.time.second();

    // --- 日付の境界線補正（簡易処理） ---
    // 時差加算により24時を超えた場合、または0時を下回った場合の処理
    if (hour >= 24)
    {
        hour -= 24;
        day++;
    }
    else if (hour < 0)
    {
        hour += 24;
        day--;
    }

    // RTC構造体に値を代入 (Core2の内部RTC: BM8563用)
    dt.date.year = (uint16_t)year;
    dt.date.month = (uint8_t)month;
    dt.date.date = (uint8_t)day;
    dt.time.hours = (uint8_t)hour;
    dt.time.minutes = (uint8_t)min;
    dt.time.seconds = (uint8_t)sec;

    M5.Rtc.setDateTime(dt);    // 内部RTCへの書き込み実行
    isRTCInitialSynced = true; // 同期完了フラグを立てる
}

/**
 * @brief メーターの指針（針）を描画する
 * @param val 現在の値
 * @param range 最大値
 * @param len 針の長さ
 * @param color 針の色
 * @param width 針の根元の幅 (1以下で細い針)
 */
void drawNeedle(float val, float range, int len, uint16_t color, int width)
{
    float angle = (((val / range) * 360.0f) - 90.0f);       // 割合を角度(-90度起点)に変換
    float rad = (angle * M_PI / 180.0f);                    // ラジアンへ変換
    int tX = (int)((float)CTR_X + ((float)len * cos(rad))); // 針の先端X
    int tY = (int)((float)CTR_Y + ((float)len * sin(rad))); // 針の先端Y
    if (width > 1)
    {                                                              // 幅指定がある場合は力強い三角形の針を描画
        int xOff = (int)((float)width * cos(rad + (M_PI / 2.0f))); // 垂直オフセットX
        int yOff = (int)((float)width * sin(rad + (M_PI / 2.0f))); // 垂直オフセットY
        canvas.fillTriangle((CTR_X + xOff), (CTR_Y + yOff), (CTR_X - xOff), (CTR_Y - yOff), tX, tY, color);
    }
    else
    { // 幅1以下の場合は細い線と先端のドットを描画
        canvas.drawLine(CTR_X, CTR_Y, tX, tY, color);
        canvas.fillCircle(tX, tY, 2, color);
    }
}

/**
 * @brief メーターの円周上に数字を配置する
 * @param count 分割数
 * @param stepValue 数値の増分 (0の場合はインデックス番号を表示)
 */
void drawDialNumbers(int count, float stepValue)
{
    canvas.setTextColor(COL_MAIN); // 数字の色を発光ブルーに設定
    for (int i = 0; i < count; i++)
    {                                                                     // 指定された数だけループ
        float angle = (((i * (360.0f / count)) - 90.0f) * M_PI / 180.0f); // 座標計算
        int val = (stepValue == 0) ? i : (int)(i * stepValue);            // 表示する数値の決定
        canvas.drawCenterString(String(val), (int)(CTR_X + NUM_R * cos(angle)), (int)(CTR_Y + NUM_R * sin(angle)), &fonts::Font2);
    }
}

/**
 * @brief 共通UI要素（背景・グリッド・ベゼル・デジタル値）を一括描画する
 * @param label モード名称 (下部)
 * @param val デジタル数値文字列 (中央)
 * @param unit 単位文字列 (中央下)
 */
void drawCommonUI(const char *label, const char *val, const char *unit)
{
    canvas.fillSprite(COL_BG); // 背景を黒で初期化
    for (int i = 1; i <= 4; i++)
    {
        canvas.drawCircle(CTR_X, CTR_Y, (GAUGE_R * i / 4), COL_GRID);
    } // 同心円
    for (int d = 0; d < 360; d += 30)
    { // 30度ごとの放射グリッド
        float rad = (d * M_PI / 180.0f);
        canvas.drawLine(CTR_X, CTR_Y, (int)(CTR_X + GAUGE_R * cos(rad)), (int)(CTR_Y + GAUGE_R * sin(rad)), COL_GRID);
    }
    canvas.drawCircle(CTR_X, CTR_Y, (GAUGE_R + 2), COL_BZ_D);           // 外側ベゼル
    canvas.drawCircle(CTR_X, CTR_Y, GAUGE_R, COL_MAIN);                 // 内側ベゼル
    canvas.drawCenterString(label, CTR_X, (CTR_Y + 45), &fonts::Font2); // 下部のラベル
    if (val != nullptr)
    {                                                                      // 数値が指定されている場合のみ描画
        canvas.drawCenterString(val, CTR_X, (CTR_Y - 5), &fonts::Font4);   // メイン数値
        canvas.drawCenterString(unit, CTR_X, (CTR_Y + 18), &fonts::Font0); // 単位
    }
}

// --- 音声再生タスク ---

/**
 * @brief 音再生専用タスク (Core 0で独立動作)
 * ソナー音をバックグラウンドで再生する
 */
void soundTask(void *pvParameters)
{
    for (;;)
    { // 無限ループ
        if (sonarTrig)
        { // メインループからの発報指示を確認
            for (int i = 0; i < 60; i++)
            {                                                     // 60ステップかけて音量を絞る
                int vol = (int)(180.0f * exp(-0.06f * (float)i)); // 指数関数的な減衰計算
                M5.Speaker.setVolume(vol);                        // 現在の音量を設定
                M5.Speaker.tone(SONAR_F, 30);                     // 850Hzの音を30ms再生
                vTaskDelay(30 / portTICK_PERIOD_MS);              // 再生時間に合わせてスリープ
            }
            sonarTrig = false;                // 再生完了フラグをリセット
            M5.Speaker.setVolume(MAX_SP_VOL); // 他の音のために標準音量に戻す
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); // フラグ監視のための待機
    }
}

// --- メイン描画管理 ---

/**
 * @brief 画面のリフレッシュ処理。GPS情報から各モードの描画を実行する
 */
void refresh()
{
    if (!gps.location.isValid()) // GPSが測位できていない場合
    {
        drawCommonUI("SEARCHING...", "NO SIGNAL", "WAITING FOR GPS"); // 警告画面
        if (sonarR == -1 && ((millis() % AUTO_SONAR_MS) < 20))
        {
            // 20秒おき自動ソナー
            sonarR = 0;
            sonarTrig = true;
        }
    }
    else // 測位完了時
    {
        char strBuf[BUFF_SIZE]; // 文字列変換用バッファ
        if (displayMode == MODE_ALT)
        {                                                      // 高度計モード
            sprintf(strBuf, "%d", (int)gps.altitude.meters()); // 整数に変換
            drawCommonUI("ALTITUDE", strBuf, "METERS");        // ベース描画
            drawDialNumbers(10, 0);                            // 0-9の目盛りを描画
            if (gps.altitude.isValid())                        // 高度が有効なら針を描画
            {
                drawNeedle(fmod((float)gps.altitude.meters(), 10000.0f),
                           10000.0f, (GAUGE_R - 40), COL_MAIN, 5); // 10000m計
                drawNeedle(fmod((float)gps.altitude.meters(), 1000.0f),
                           1000.0f, (GAUGE_R - 10), COL_SUB, 3); // 1000m計
            }
        }
        else if (displayMode == MODE_CLK) // 時計モード
        {
            auto dt = M5.Rtc.getDateTime(); // 現地時間取得
            int hour = dt.time.hours;       // 時取得
            int minute = dt.time.minutes;   // 分取得
            int second = dt.time.seconds;   // 秒取得

            sprintf(strBuf, "%02d:%02d", hour, minute);        // 時刻文字列
            drawCommonUI("CHRONOMETER", strBuf, "LOCAL TIME"); // ベース描画
            for (int i = 1; i <= 12; i++)
            { // 時計専用の1-12目盛り配置
                float rad = (((i * 30.0f) - 90.0f) * M_PI / 180.0f);
                canvas.drawCenterString(String(i), (int)(CTR_X + NUM_R * cos(rad)), (int)(CTR_Y + NUM_R * sin(rad)), &fonts::Font2);
            }
            drawNeedle(fmod((float)hour, 12.0f) + ((float)minute / 60.0f), 12.0f, (GAUGE_R - 45), COL_MAIN, 6);
            drawNeedle((float)minute + ((float)second / 60.0f), 60.0f, (GAUGE_R - 20), COL_MAIN, 3);
            drawNeedle((float)second, 60.0f, (GAUGE_R - 10), COL_SUB, 1);
        }
        else // 速度計モード
        {
            float currentSpeed = (float)gps.speed.kmph(); // 時速取得
            currentMaxSpd = RNG_MID;
            sprintf(strBuf, "%.1f", currentSpeed);      // 小数点1位まで
            drawCommonUI("SPEED", strBuf, "KM/H");      // ベース描画
            drawDialNumbers(5, (currentMaxSpd / 4.0f)); // 4分割の速度目盛りを描画
            if (gps.speed.isValid())
            { // 速度が有効なら針を描画
                drawNeedle(currentSpeed, currentMaxSpd, (GAUGE_R - 15), COL_SUB, 3);
            }
        }
    }

    // ソナー演出 (常に描画レイヤーの最前面に重ねる)
    if (sonarR >= 0)
    {
        int br = (255 - ((sonarR * 255) / SONAR_MAX_R)); // 半径が広がるほど透過(暗く)させる
        if (br > 0)
        {
            uint16_t fC = fadeCol(COL_MAIN, br);         // フェードカラー生成
            canvas.drawCircle(CTR_X, CTR_Y, sonarR, fC); // メインリング
        }
        sonarR += SONAR_SPD; // 波紋を拡大
        if (sonarR > SONAR_MAX_R)
        {
            sonarR = -1;
        } // 画面外で停止
    }
    canvas.pushSprite(0, 0); // バッファに描いた画像をLCDへ転送
}

// --- メインエントリ ---
/**
 * @brief プログラム開始位置
 */
void setup()
{
    auto cfg = M5.config();                                     // デバイス設定取得
    M5.begin(cfg);                                              // デバイス初期化
    M5.Speaker.setVolume(MAX_SP_VOL);                           // スピーカー音量を中間値に設定
    GPSRaw.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN); // GPSシリアル通信開始
    canvas.createSprite(SCR_W, SCR_H);                          // 描画バッファ用のメモリを確保
    // 音再生処理をCore 0(サブコア)に割り当てて、描画への影響をゼロにする
    xTaskCreateUniversal(soundTask, "soundTask", 4096, NULL, 1, NULL, 0);
}

/**
 * @brief メインループ
 */
void loop()
{
    M5.update(); // ボタンやタッチの最新状態を確認
    if (M5.BtnA.wasPressed())
    // Aボタン(左)：高度計
    {
        displayMode = MODE_ALT;
    }
    if (M5.BtnB.wasPressed())
    // Bボタン(中)：時計
    {
        displayMode = MODE_CLK;
    }
    if (M5.BtnC.wasPressed())
    // Cボタン(右)：速度計
    {
        displayMode = MODE_SPD;
    }
    auto touch = M5.Touch.getDetail(); // タッチパネル情報の取得
    if (touch.wasPressed())
    { // 画面タッチでセンターからソナー発射
        sonarR = 0;
        sonarTrig = true;
    }
    while (GPSRaw.available() > 0) // 測位が有効の間ループ
    {
        gps.encode(GPSRaw.read()); // GPSパケットの受信と解析
    }

    // 位置・時刻が確定し、かつ未同期の場合にRTC同期を実行
    if ((!isRTCInitialSynced) && (gps.location.isValid()) && (gps.time.isUpdated()))
    {
        syncRTCWithAutoTZ();
    }
    refresh();              // 画面描画の実行
    delay(MAIN_LOOP_DELAY); // CPUの過負荷防止
}
