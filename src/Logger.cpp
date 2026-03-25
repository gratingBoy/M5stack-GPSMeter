#include "Logger.h"
#include "VHSNoise.h"

/**
 * @brief ログバッファ（まとめ書き用）
 */
static String logBuffer = "";

/**
 * @brief 日付に応じたログファイルを開く
 *
 * 日付が変わった場合は新しいファイルを生成し、
 * 既存ファイルをクローズする。
 */
static void openLogFile()
{
    auto dt = M5.Rtc.getDateTime();
    int date = dt.date.year * 10000 +
               dt.date.month * 100 +
               dt.date.date;
    char filename[FNAME_BUF_SIZE];

    // 同日なら何もしない
    if ((date == currentLogDate) && gpsLogFile)
    {
        return;
    }

    currentLogDate = date;
    if (gpsLogFile)
    {
        gpsLogFile.close();
    }
    sprintf(filename, "/gps_%d.txt", date);
    gpsLogFile = SD.open(filename, FILE_APPEND);
}

/**
 * @brief ログ初期化
 */
void initLogger()
{
    SD.begin();
    openLogFile();
    lastLogTime = millis();
}

/**
 * @brief ログバッファを書き込み
 *
 * バッファ内容をSDカードへ書き出し、
 * 書き込み後にバッファをクリアする。
 */
static void flushLog()
{
    if (!gpsLogFile || (logBuffer.length() == 0))
    {
        return;
    }

    gpsLogFile.print(logBuffer);
    gpsLogFile.flush();
    logBuffer = "";
    vhsNoiseTrig = true;
}

/**
 * @brief GPS読み取り＋ログ処理
 *
 * - NMEA受信
 * - 必要な文だけ保存
 * - 1分ごとに書き込み
 */
void updateLogger()
{
    while (GPSRaw.available())
    {
        char c = GPSRaw.read(); // GPSからの情報読込み
        gps.encode(c);
        if (c == '\n')
        {
            if ((nmeaLine.length() > 6) &&
                nmeaLine[0] == '$' &&
                (nmeaLine.startsWith("$GPRMC") ||
                 nmeaLine.startsWith("$GPGGA") ||
                 nmeaLine.startsWith("$GNRMC") ||
                 nmeaLine.startsWith("$GNGGA")))
            {
                logBuffer += nmeaLine + "\n";
            }
            nmeaLine = "";
        }
    }
    // 定期書き込み
    if ((millis() - lastLogTime) >= LOG_INTERVAL)
    {
        flushLog();
        lastLogTime = millis(); // 最終ログ書き込み時刻取得
        vhsNoiseTrig = true;    // ジッターノイズ開始
    }
    openLogFile();
}