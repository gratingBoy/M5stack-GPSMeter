#include "Logger.h"

/**
 * @brief ログバッファ（まとめ書き用）
 */
static String logBuffer = "";

/**
 * @brief 日付ごとのログファイルを開く
 *
 * 日付が変わったら新しいファイルへ切替
 */
static void openLogFile()
{
    auto dt = M5.Rtc.getDateTime();

    int date = dt.date.year * 10000 +
               dt.date.month * 100 +
               dt.date.date;

    // 同日なら何もしない
    if (date == currentLogDate && gpsLogFile)
        return;

    currentLogDate = date;

    if (gpsLogFile)
        gpsLogFile.close();

    char filename[32];
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
 * @brief バッファを書き込み
 */
static void flushLog()
{
    if (!gpsLogFile || logBuffer.length() == 0)
        return;

    gpsLogFile.print(logBuffer);
    gpsLogFile.flush();
    logBuffer = "";
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
        char c = GPSRaw.read();
        gps.encode(c);

        if (c == '\n')
        {
            if (!LOG_ONLY_RMC || nmeaLine.startsWith("$GPRMC"))
            {
                logBuffer += nmeaLine + "\n";
            }
            nmeaLine = "";
        }
        else if (c != '\r')
        {
            nmeaLine += c;
        }
    }

    // 定期書き込み
    if (millis() - lastLogTime >= LOG_INTERVAL)
    {
        flushLog();
        lastLogTime = millis();
    }

    openLogFile();
}