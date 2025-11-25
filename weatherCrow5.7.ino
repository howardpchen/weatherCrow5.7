#include "config.h"
#include <WiFi.h> 
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "EPD.h"
#include "weatherIcons.h"
#include <esp_sleep.h>
#include <time.h>  // Still needed for display functions using gmtime()

// Communication and data constants
#define BAUD_RATE 115200
#define HTTP_NOT_REQUESTED_YET -999
#define STRING_BUFFER_SIZE 256
#define JSON_CAPACITY 32768

// Display and UI constants
#define TEMPERATURE_GRAPH_LINE_WIDTH 2
#define UVI_DISPLAY_THRESHOLD 4.0
#define WIND_SPEED_DISPLAY_THRESHOLD 4.0

class WeatherCrow
{
private:
  // Display buffer
  uint8_t imageBW[27200];

  // Network and API configuration
  const char *ssid = WIFI_SSID;
  const char *password = WIFI_PASSWORD;
  String openWeatherMapApiKey = OPEN_WEATHER_MAP_API_KEY;
  String airNowApiKey = AIRNOW_API_KEY;
  String apiParamLatitude = LATITUDE;
  String apiParamLongitude = LONGITUDE;

  // Data buffers
  String jsonBuffer;
  String errorMessageBuffer;
  int httpResponseCode = HTTP_NOT_REQUESTED_YET;
  DynamicJsonDocument weatherApiResponse = DynamicJsonDocument(JSON_CAPACITY);
  DynamicJsonDocument airnowDoc{4096};

  // --------------------------------------------------------------
  // Weather information structure
  struct WeatherInfo
  {
    String weather;
    long currentDateTime;  // local epoch time (with offset applied)
    long sunrise;
    long sunset;
    String temperature;
    String tempIntegerPart;
    String tempDecimalPart;
    String humidity;
    String pressure;
    String windSpeed;
    String timezone;
    String icon;
    String precipProb;     // NEW: 0–100 (%) chance of rain
    String aqi;          // e.g., "67"
    String aqiCategory;  // e.g., "Moderate"
    String aqiParam;     // e.g., "PM2.5" or "O3"
    String uvi;
} weatherInfo;
  // --------------------------------------------------------------

  void logPrint(const char *msg) { Serial.print(msg); }
  void logPrint(int msg) { Serial.print(msg); }
  void logPrint(String msg) { Serial.print(msg); }
  void logPrintln(const char *msg) { Serial.println(msg); }
  void logPrintln(int msg) { Serial.println(msg); }
  void logPrintln(String msg) { Serial.println(msg); }

  String httpsGETRequest(const char *serverName)
  {
    WiFiClientSecure client;
    HTTPClient http;
    client.setInsecure();
    http.begin(client, serverName);
    httpResponseCode = http.GET();
    String payload = "{}";
    if (httpResponseCode > 0)
    {
      payload = http.getString();
    }
    http.end();
    return payload;
  }

  void connectToWiFi()
  {
    WiFi.begin(ssid, password);
    logPrintln("WiFi Connecting");
    unsigned long startAttemptTime = millis();
    const unsigned long WIFI_TIMEOUT_MS = 180000; // 3 minutes timeout

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS)
    {
      delay(300);
      logPrint(".");
    }

    if (LOW_POWER_MODE)
    {
      // Restore CPU frequency to 240MHz for faster WiFi connection
      setCpuFrequencyMhz(240);
    }

    if (WiFi.status() != WL_CONNECTED)
    {
      logPrintln("Failed to connect to WiFi. Retrying...");
      errorMessageBuffer = "Failed to connect to the WiFi network.";
    }
    logPrintln("");
  }

  void screenPowerOn()
  {
    const int SCREEN_POWER_PIN = 7;
    pinMode(SCREEN_POWER_PIN, OUTPUT);
    digitalWrite(SCREEN_POWER_PIN, HIGH);
  }

  void clearScreen()
  {
    Paint_NewImage(imageBW, EPD_W, EPD_H, Rotation, WHITE);
    Paint_Clear(WHITE);
    EPD_FastMode1Init();
    EPD_Display_Clear();
    EPD_Update();
    EPD_Clear_R26A6H();
  }

  void displaySystemInfo(uint16_t baseX, uint16_t baseY)
  {
    uint16_t x = baseX;
    uint16_t y = baseY;
    uint16_t yOffset = 8;
    char buffer[STRING_BUFFER_SIZE];

    // Display WiFi SSID
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "WIFI SSID: %s", String(WIFI_SSID));
    EPD_ShowString(x, y, buffer, FONT_SIZE_8, BLACK, false);
    y += yOffset;

    // Display WiFi connection status
    memset(buffer, 0, sizeof(buffer));
    if (WL_CONNECTED == WiFi.status())
    {
      snprintf(buffer, sizeof(buffer), "DEVICE IS CONNECTED TO WIFI. IP: %s", WiFi.localIP().toString().c_str());
    }
    else
    {
      snprintf(buffer, sizeof(buffer), "WIFI NOT CONNECTED. CHECK WIFI SSID AND CREDENTIALS. ROUTER REBOOT MAY HELP.");
    }
    EPD_ShowString(x, y, buffer, FONT_SIZE_8, BLACK, false);
    y += yOffset;

    // Display location coordinates
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "LOCATION LATITUDE: %s, LONGITUDE: %s", String(LATITUDE), String(LONGITUDE));
    EPD_ShowString(x, y, buffer, FONT_SIZE_8, BLACK, false);
    y += yOffset;

    // Display latest API call time
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "LATEST SUCCESSFUL API CALL: %s", convertUnixTimeToDateTimeString(weatherInfo.currentDateTime).c_str());
    EPD_ShowString(x, y, buffer, FONT_SIZE_8, BLACK, false);
  }

  String convertUnixTimeToDateTimeString(long unixTime)
  {
    if (unixTime == 0)
    {
      return "N/A";
    }
    time_t rawtime = unixTime;
    struct tm *dt;
    char buffer[30];
    dt = gmtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", dt);
    return String(buffer);
  }

  String convertUnixTimeToShortDateTimeString(long unixTime)
  {
    if (unixTime == 0)
    {
      return "N/A";
    }
    time_t rawtime = unixTime;
    struct tm *dt;
    char buffer[30];
    dt = gmtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%I %p", dt);

    // Remove leading zero from hour
    if (buffer[0] == '0')
    {
      memmove(buffer, buffer + 1, strlen(buffer));
    }
    return String(buffer);
  }

  String convertUnixTimeToDisplayFormat(long unixTime)
  {
    if (unixTime == 0)
    {
      return "N/A";
    }
    time_t rawtime = unixTime;
    struct tm *dt;
    char buffer[30];
    dt = gmtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%b %d, %a", dt);
    return String(buffer);
  }

  void displayErrorMessage(char *title, char *description)
  {
    clearScreen();

    uint16_t baseXpos = 258;
    char buffer[STRING_BUFFER_SIZE];

    // Display error icon (randomly choose a fallback error style)
    const char *icons[] = {"emma_cupcake_lg", "emma_mon1_lg", "leo_face_lg", "leo_gator_lg"};
    int randomIndex = random(0, sizeof(icons) / sizeof(icons[0]));
    EPD_drawImage(30, 20, icon_map[icons[randomIndex]]);
    EPD_DrawLine(baseXpos, 110, 740, 110, BLACK);

    // Display error title
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s ", title);
    EPD_ShowString(baseXpos, 70, buffer, FONT_SIZE_36, BLACK);

    // Display error description
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s ", description);
    EPD_ShowString(baseXpos, 140, buffer, FONT_SIZE_16, BLACK);

    // Display system info
    displaySystemInfo(baseXpos, 230);

    // Update and sleep
    EPD_Display(imageBW);
    EPD_PartUpdate();
    EPD_DeepSleep();
  }

  String constructAirNowUrl() {
    // AirNow current observation by lat/long (U.S. AQI, 0–500)
    // Docs: https://docs.airnowapi.org/ (Current Observations by Lat/Long)
    String url = "https://www.airnowapi.org/aq/observation/latLong/current/"
                "?format=application/json"
                "&latitude=" + apiParamLatitude +
                "&longitude=" + apiParamLongitude +
                "&distance=50"
                "&API_KEY=" + airNowApiKey;
    logPrintln(url);
    return url;
  }

  String constructApiEndpointUrl()
  {
    String str = "https://api.openweathermap.org/data/3.0/onecall?lat=" +
                 apiParamLatitude + "&lon=" + apiParamLongitude +
                 "&exclude=minutely" +
                 "&APPID=" + openWeatherMapApiKey +
                 // Force imperial so wind_speed arrives in mph (OpenWeatherMap uses m/s for metric)
                 "&units=imperial";
    logPrintln(str);
    return str;
  }

  bool processWeatherData()
  {
    weatherInfo.weather = weatherApiResponse["current"]["weather"][0]["main"].as<String>();
    weatherInfo.icon    = weatherApiResponse["current"]["weather"][0]["icon"].as<String>();

    // Add time offset to get local time
    weatherInfo.currentDateTime = 
      weatherApiResponse["current"]["dt"].as<long>() + weatherApiResponse["timezone_offset"].as<long>();

    weatherInfo.sunrise    = weatherApiResponse["current"]["sunrise"].as<long>();
    weatherInfo.sunset     = weatherApiResponse["current"]["sunset"].as<long>();
    weatherInfo.temperature = String(weatherApiResponse["current"]["temp"].as<float>(), 1);
    weatherInfo.humidity    = String(weatherApiResponse["current"]["humidity"].as<int>());
    weatherInfo.pressure    = String(weatherApiResponse["current"]["pressure"].as<int>());
    float windSpeedMph      = weatherApiResponse["current"]["wind_speed"].as<float>(); // already mph via units=imperial
    weatherInfo.windSpeed   = String(windSpeedMph, 1);
    weatherInfo.timezone    = weatherApiResponse["timezone"].as<String>();

    // UVI (as string with 1 decimal)
    if (weatherApiResponse["current"].containsKey("uvi")) {
      weatherInfo.uvi = String(weatherApiResponse["current"]["uvi"].as<float>(), 1);
    } else {
      weatherInfo.uvi = "N/A";
    }


    float pop = -1.0f;
    if (weatherApiResponse.containsKey("hourly") &&
        weatherApiResponse["hourly"].size() > 0 &&
        weatherApiResponse["hourly"][0].containsKey("pop"))
    {
      pop = weatherApiResponse["hourly"][0]["pop"].as<float>();    // 0.0 - 1.0
    }
    else if (weatherApiResponse.containsKey("daily") &&
             weatherApiResponse["daily"].size() > 0 &&
             weatherApiResponse["daily"][0].containsKey("pop"))
    {
      pop = weatherApiResponse["daily"][0]["pop"].as<float>();     // 0.0 - 1.0
    }

    if (pop >= 0.0f) {
      int popPct = (int)(pop * 100.0f + 0.5f); // round to nearest int
      weatherInfo.precipProb = String(popPct);
    } else {
      weatherInfo.precipProb = "N/A";
    }


    // Process temperature for display
    int decimalPos = weatherInfo.temperature.indexOf('.');
    weatherInfo.tempIntegerPart = String(weatherInfo.temperature.substring(0, decimalPos).toInt());
    weatherInfo.tempDecimalPart = String(weatherInfo.temperature.substring(decimalPos + 1).toInt());
    if (weatherInfo.tempDecimalPart.length() > 1)
    {
      weatherInfo.tempDecimalPart = weatherInfo.tempDecimalPart.substring(0, 1);
    }
    return true;
  }


  bool getAirNowAQI() {
    // Reset defaults
    weatherInfo.aqi = "N/A";
    weatherInfo.aqiCategory = "";
    weatherInfo.aqiParam = "";

    if (WiFi.status() != WL_CONNECTED) {
      errorMessageBuffer = "WiFi not connected for AirNow AQI.";
      weatherInfo.aqi = "-1";
      return false;
    }

    String url = constructAirNowUrl();
    String payload = httpsGETRequest(url.c_str());
    if (httpResponseCode != HTTP_CODE_OK) {
      logPrint("AirNow HTTP status: "); logPrintln(httpResponseCode);
      return false;
    }

    DeserializationError err = deserializeJson(airnowDoc, payload);
    if (err) {
      logPrint("AirNow JSON error: "); logPrintln(err.c_str());
      return false;
    }

    // AirNow returns an array of observations by pollutant: { ParameterName, AQI, Category:{Name}, ... }
    if (!airnowDoc.is<JsonArray>()) {
      logPrintln("AirNow: expected array.");
      return false;
    }
    JsonArray arr = airnowDoc.as<JsonArray>();
    if (arr.size() == 0) return false;

    // Prefer PM2.5 if present; otherwise use max AQI among entries (common fallback).
    int chosenIdx = -1;
    int highestAqi = -1;

    for (size_t i = 0; i < arr.size(); ++i) {
      JsonObject o = arr[i].as<JsonObject>();
      if (!o.containsKey("AQI")) continue;
      int aqi = o["AQI"].as<int>();
      String param = o["ParameterName"].as<String>();

      if (param == "PM2.5") { chosenIdx = (int)i; break; }
      if (aqi > highestAqi) { highestAqi = aqi; chosenIdx = (int)i; }
    }

    if (chosenIdx < 0) return false;

    JsonObject sel = arr[(size_t)chosenIdx];
    int aqiVal = sel["AQI"].as<int>();
    weatherInfo.aqi        = String(aqiVal);
    weatherInfo.aqiParam   = sel["ParameterName"].as<String>();
    if (sel.containsKey("Category") && sel["Category"].containsKey("Name")) {
      weatherInfo.aqiCategory = sel["Category"]["Name"].as<String>(); // e.g., Good/Moderate/…
    } else {
      // Category missing? Derive roughly per AirNow ranges.
      if      (aqiVal <= 50)  weatherInfo.aqiCategory = "Good";
      else if (aqiVal <= 100) weatherInfo.aqiCategory = "Moderate";
      else if (aqiVal <= 150) weatherInfo.aqiCategory = "USG";
      else if (aqiVal <= 200) weatherInfo.aqiCategory = "Unhealthy";
      else if (aqiVal <= 300) weatherInfo.aqiCategory = "Very Unhealthy";
      else                    weatherInfo.aqiCategory = "Hazardous";
    }
    return true;
  }



  bool getWeatherInfo(uint8_t maxRetries = 3)
  {
    uint8_t currentRetry = 0;
    bool success = false;

    while (currentRetry <= maxRetries && !success)
    {
      if (currentRetry > 0)
      {
        logPrint("Retry attempt ");
        logPrint(currentRetry);
        logPrintln(" of weather data fetch...");
        // Exponential backoff
        delay(1000 * (1 << (currentRetry - 1)));
      }

      errorMessageBuffer = "getWeatherInfo() failed.";
      httpResponseCode = HTTP_NOT_REQUESTED_YET;
      if (WiFi.status() != WL_CONNECTED)
      {
        errorMessageBuffer = "Wireless network not available.";
        return false;
      }

      String apiEndPoint = constructApiEndpointUrl();
      jsonBuffer = httpsGETRequest(apiEndPoint.c_str());

      const unsigned long HTTP_TIMEOUT_MS = 5000; 
      unsigned long startTime = millis();
      while (httpResponseCode < 0 && (millis() - startTime) < HTTP_TIMEOUT_MS)
      {
        delay(500);
      }

      if (httpResponseCode < 0)
      {
        logPrintln("HTTP request timed out.");
        currentRetry++;
        continue;
      }

      logPrint("HTTP response code: ");
      logPrintln(httpResponseCode);

      if (httpResponseCode != HTTP_CODE_OK)
      {
        errorMessageBuffer = "Weather API failed with HTTP code: " + String(httpResponseCode) +
                             "\n\nThis typically happens due to a weather API server issue or an invalid API key.";
        if (httpResponseCode >= HTTP_CODE_INTERNAL_SERVER_ERROR || httpResponseCode == HTTP_CODE_TOO_MANY_REQUESTS)
        {
          currentRetry++;
          continue;
        }
        return false; 
      }

      if (LOW_POWER_MODE)
      {
        // Turn off wireless as soon as possible
        wirelessOff();
      }

      // Parse JSON
      DeserializationError error = deserializeJson(weatherApiResponse, jsonBuffer);
      if (error)
      {
        logPrint(F("deserializeJson() failed: "));
        logPrintln(error.c_str());
        errorMessageBuffer = "JSON parsing error: " + String(error.c_str());
        currentRetry++;
        continue;
      }

      success = processWeatherData();
      if (!success)
      {
        currentRetry++;
      }
    }
    if (!success)
    {
      logPrintln("Failed to get weather info after all retry attempts");
    }
    return success;
  }

  void drawForecastItem(uint16_t x, uint16_t y, JsonObject hourlyData)
  {
    char buffer[STRING_BUFFER_SIZE];

    String icon;
    if (hourlyData["weather"][0].containsKey("icon"))
    {
      icon = "icon_" + hourlyData["weather"][0]["icon"].as<String>();
    }
    else
    {
      icon = "na_md"; // fallback
    }

    snprintf(buffer, sizeof(buffer), "%s_sm", icon.c_str());
    if (icon_map.count(buffer) > 0)
    {
      EPD_drawImage(x + 2, y, icon_map[buffer]);
    }
    else
    {
     EPD_drawImage(x + 2, y, icon_map["error_sm"]);
    }

    long forecastTimeInt = hourlyData["dt"].as<long>() + weatherApiResponse["timezone_offset"].as<long>();
    String forecastTime = convertUnixTimeToShortDateTimeString(forecastTimeInt);

    int splitIndex = forecastTime.indexOf(' ');
    String hour = forecastTime.substring(0, splitIndex);
    String period = forecastTime.substring(splitIndex + 1);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", hour.c_str());
    EPD_ShowStringRightAligned(x + 24, y + 80, buffer, FONT_SIZE_16, BLACK);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", period.c_str());
    EPD_ShowStringRightAligned(x + 26, y + 96, buffer, FONT_SIZE_16, BLACK);

    // Vertical separator line
    uint16_t lineLength = 103;
    EPD_DrawLine(x + 35, y + 75, x + 35, y + lineLength, BLACK);
    EPD_DrawLine(x + 36, y + 75, x + 36, y + lineLength, BLACK);
    EPD_DrawLine(x + 37, y + 75, x + 37, y + lineLength, BLACK);

    // Temperature
    memset(buffer, 0, sizeof(buffer));
    float temp = hourlyData["temp"].as<float>();
    int tempInt = (int)temp;
    snprintf(buffer, sizeof(buffer), "%d", tempInt);
    EPD_ShowString(x + 46, y + 87, buffer, FONT_SIZE_38, BLACK, true);
  }

  void drawWeatherFutureForecast(uint16_t baseX, uint16_t baseY, uint16_t length)
  {
    uint16_t x = baseX;
    uint16_t y = baseY;
    char buffer[STRING_BUFFER_SIZE];

    if (!weatherApiResponse.containsKey("hourly"))
    {
      snprintf(buffer, sizeof(buffer), "%s", "Error: API not responding with hourly data.");
      EPD_ShowString(x, y, buffer, FONT_SIZE_16, BLACK, true);
      return;
    }

    size_t availableHours = weatherApiResponse["hourly"].size();
    if (availableHours == 0)
    {
      snprintf(buffer, sizeof(buffer), "%s", "Error: API responding with ZERO forecast data.");
      EPD_ShowString(x, y, buffer, FONT_SIZE_16, BLACK, true);
      return;
    }

    int forecastsToShow = min(length, (uint16_t)availableHours);

    for (uint16_t i = 1, counter = 0; counter < forecastsToShow; i += HOUR_INTERVAL, counter++)
    {
      if (i >= availableHours)
      {
        Serial.println("Index out of bounds for hourly forecast");
        break;
      }

      JsonObject hourly = weatherApiResponse["hourly"][i];

      if (!hourly.containsKey("weather") || hourly["weather"].size() == 0)
      {
        Serial.println("No weather data for this hour");
        continue;
      }
      drawForecastItem(x, y, hourly);
      x = x + 107;
    }
  }

  void prepareGraphData(float *temps, float &minTemp, float &maxTemp, int &hoursToShow, const size_t availableHours)
  {
    hoursToShow = min((int)24, (int)availableHours);

    minTemp = 100.0;
    maxTemp = -100.0;

    for (int i = 0; i < hoursToShow; i++)
    {
      if (i >= availableHours) break;
      temps[i] = weatherApiResponse["hourly"][i]["temp"].as<float>();
      if (temps[i] < minTemp) minTemp = temps[i];
      if (temps[i] > maxTemp) maxTemp = temps[i];
    }

    // Add padding
    float tempRange = maxTemp - minTemp;
    if (tempRange < 5.0)
    {
      float padding = (5.0 - tempRange) / 2;
      minTemp -= padding;
      maxTemp += padding;
    }
    else
    {
      minTemp -= tempRange * 0.1;
      maxTemp += tempRange * 0.1;
    }
  }

  void drawGraph(uint16_t baseX, uint16_t baseY, uint16_t graphWidth, uint16_t graphHeight, uint8_t lineWidth = 1)
  {
    uint16_t x = baseX;
    uint16_t y = baseY;
    char buffer[STRING_BUFFER_SIZE];

    if (!weatherApiResponse.containsKey("hourly"))
    {
      snprintf(buffer, sizeof(buffer), "%s", "Error: No hourly data available");
      EPD_ShowString(x, y, buffer, FONT_SIZE_16, BLACK, true);
      return;
    }

    size_t availableHours = weatherApiResponse["hourly"].size();
    if (availableHours < 2)
    {
      snprintf(buffer, sizeof(buffer), "%s", "Error: Insufficient forecast data");
      EPD_ShowString(x, y, buffer, FONT_SIZE_16, BLACK, true);
      return;
    }

    float temps[24];
    float minTemp, maxTemp;
    int hoursToShow;
    prepareGraphData(temps, minTemp, maxTemp, hoursToShow, availableHours);

    float tempRange = maxTemp - minTemp;
    float scale = graphHeight / tempRange;
    uint16_t pointSpacing = graphWidth / (hoursToShow - 1);

    for (int i = 1; i < hoursToShow; i++)
    {
      uint16_t x1 = x + (i - 1) * pointSpacing;
      uint16_t y1 = y + graphHeight - (uint16_t)((temps[i - 1] - minTemp) * scale);
      uint16_t x2 = x + i * pointSpacing;
      uint16_t y2 = y + graphHeight - (uint16_t)((temps[i] - minTemp) * scale);

      // Thicker line if needed
      for (int w = 0; w < lineWidth; w++)
      {
        EPD_DrawLine(x1, y1 + w, x2, y2 + w, BLACK);
      }
      EPD_DrawCircle(x1, y1, 2, BLACK, true);
      if (i == hoursToShow - 1)
      {
        EPD_DrawCircle(x2, y2, 2, BLACK, true);
      }

      // Label every 6 hours
      if (i % 6 == 0 || i == 1)
      {
        long time = weatherApiResponse["hourly"][i]["dt"].as<long>() +
                    weatherApiResponse["timezone_offset"].as<long>();
        String timeStr = convertUnixTimeToShortDateTimeString(time);
        snprintf(buffer, sizeof(buffer), "%s", timeStr.c_str());
        EPD_ShowString(x2 - 10, y + graphHeight + 5, buffer, FONT_SIZE_8, BLACK, true);
      }
    }

    // Temperature scale on right
    snprintf(buffer, sizeof(buffer), "%.1f°", maxTemp);
    EPD_ShowString(x + graphWidth + 5, y, buffer, FONT_SIZE_8, BLACK, true);

    snprintf(buffer, sizeof(buffer), "%.1f°", minTemp);
    EPD_ShowString(x + graphWidth + 5, y + graphHeight - 10, buffer, FONT_SIZE_8, BLACK, true);
  }

  void displayCurrentInfo(uint16_t baseX, uint16_t baseY)
  {
    uint16_t centerX = baseX;
    uint16_t y = baseY;
    uint16_t unitOffsetX = 4;
    uint16_t unitOffsetY = 5;
    char buffer[STRING_BUFFER_SIZE];

    // ----- Line 1: Chance of rain (left) and AQI (right column) -----
    // Chance of rain value
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", weatherInfo.precipProb.c_str());
    EPD_ShowStringRightAligned(centerX, y, buffer, FONT_SIZE_36, BLACK);

    // Chance of rain label
    memset(buffer, 0, sizeof(buffer));
    if (weatherInfo.precipProb == "N/A") {
      snprintf(buffer, sizeof(buffer), "rain");
    } else {
      snprintf(buffer, sizeof(buffer), "%% rain");
    }
    EPD_ShowString(centerX + unitOffsetX, y + unitOffsetY, buffer, FONT_SIZE_16, BLACK, false);

    // Right column: AQI label (Font 16)
    const uint16_t rightColLabelX = centerX + 70;   // label position
    const uint16_t rightColValueX = rightColLabelX + 40; // value position, adjust spacing

    EPD_ShowString(rightColLabelX, y + unitOffsetY, "AQI", FONT_SIZE_16, BLACK, false);

    // AQI value (Font 36) to the right of the label
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", weatherInfo.aqi.c_str());
    EPD_ShowString(rightColValueX, y, buffer, FONT_SIZE_36, BLACK);

    // ----- Line 2: Humidity (left) and UVI (right column) -----
    y += 40;

    // Humidity value
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", weatherInfo.humidity.c_str());
    EPD_ShowStringRightAligned(centerX, y, buffer, FONT_SIZE_36, BLACK);

    // Humidity label
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%%RH");
    EPD_ShowString(centerX + unitOffsetX, y + unitOffsetY, buffer, FONT_SIZE_16, BLACK, false);

    // UVI label (Font 16)
    EPD_ShowString(rightColLabelX, y + unitOffsetY, "UVI", FONT_SIZE_16, BLACK, false);

    // UVI value (Font 36)
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", weatherInfo.uvi.c_str());
    EPD_ShowString(rightColValueX, y, buffer, FONT_SIZE_36, BLACK);

    // ----- Line 3: Additional line -----
    y += 40;
    displayAdditionalInfoLine(centerX, y, unitOffsetX, unitOffsetY);
  }

  void displayAdditionalInfoLine(uint16_t centerX, uint16_t y, uint16_t unitOffsetX, uint16_t unitOffsetY)
  {
    char buffer[STRING_BUFFER_SIZE];
    if ((weatherApiResponse["current"].containsKey("snow")) &&
        (weatherApiResponse["current"]["snow"].containsKey("1h")))
    {
      // Snow
      snprintf(buffer, sizeof(buffer), "%s", weatherApiResponse["current"]["snow"]["1h"].as<String>().c_str());
      EPD_ShowStringRightAligned(centerX, y, buffer, FONT_SIZE_36, BLACK);

      snprintf(buffer, sizeof(buffer), "mm snow");
      EPD_ShowString(centerX + unitOffsetX, y + unitOffsetY, buffer, FONT_SIZE_16, BLACK, false);
    }
    else if ((weatherApiResponse["current"].containsKey("rain")) &&
             (weatherApiResponse["current"]["rain"].containsKey("1h")))
    {
      // Rain
      snprintf(buffer, sizeof(buffer), "%s", weatherApiResponse["current"]["rain"]["1h"].as<String>().c_str());
      EPD_ShowStringRightAligned(centerX, y, buffer, FONT_SIZE_36, BLACK);

      snprintf(buffer, sizeof(buffer), "mm rain");
      EPD_ShowString(centerX + unitOffsetX, y + unitOffsetY, buffer, FONT_SIZE_16, BLACK, false);
    }
    else if (weatherInfo.windSpeed.toFloat() > WIND_SPEED_DISPLAY_THRESHOLD)
    {
      // Wind
      snprintf(buffer, sizeof(buffer), "%s", weatherInfo.windSpeed.c_str());
      EPD_ShowStringRightAligned(centerX, y, buffer, FONT_SIZE_36, BLACK);

      snprintf(buffer, sizeof(buffer), "mph wind");
      EPD_ShowString(centerX + unitOffsetX, y + unitOffsetY, buffer, FONT_SIZE_16, BLACK, false);
    }
  }

  void displayWeatherForecast()
  {
    clearScreen();
    char buffer[STRING_BUFFER_SIZE];

    // Date
    snprintf(buffer, sizeof(buffer), "%s ", convertUnixTimeToDisplayFormat(weatherInfo.currentDateTime).c_str());
    EPD_ShowStringRightAligned(790, 25, buffer, FONT_SIZE_36, BLACK);

    // Temperature
    uint16_t yPos = 105;
    snprintf(buffer, sizeof(buffer), "%s", weatherInfo.tempIntegerPart.c_str());
    EPD_ShowStringRightAligned(740, yPos, buffer, FONT_SIZE_92, BLACK);

    // Units
    memset(buffer, 0, sizeof(buffer));
    if (String(UNITS) == "metric")
    {
      snprintf(buffer, sizeof(buffer), "C");
      EPD_drawImage(740, yPos, degrees_sm);
    }
    else
    {
      snprintf(buffer, sizeof(buffer), "F");
    }
    EPD_ShowString(750, yPos + 24, buffer, FONT_SIZE_36, BLACK);

    // Weather icon with error fallback
    String iconName = "icon_" + weatherInfo.icon + "_lg";
    if (icon_map.count(iconName.c_str()) > 0)
      EPD_drawImage(10, 1, icon_map[iconName.c_str()]);
    else
      EPD_ShowString(10, EPD_H/2-20, weatherInfo.weather.c_str(), FONT_SIZE_36, BLACK, false);
      //EPD_drawImage(10, 1, icon_map["error_lg"]);

    // Pressure, humidity, etc.
    displayCurrentInfo(380, 30);

    // Future forecast
    drawWeatherFutureForecast(270, 160, 5);

    // Display location name
    EPD_ShowStringCenterAligned(135, EPD_H - 20, LOCATION_NAME, FONT_SIZE_16, BLACK);

    // Lower CPU freq for power saving
    if (LOW_POWER_MODE)
    {
      setCpuFrequencyMhz(80);
    }
    // Update display
    EPD_Display(imageBW);
    EPD_PartUpdate();
    EPD_DeepSleep();
  }

  void displayError()
  {
    char title[] = "Something went wrong.";
    char description[STRING_BUFFER_SIZE];
    memset(description, 0, sizeof(description));
    strncpy(description, errorMessageBuffer.c_str(), sizeof(description) - 1);
    displayErrorMessage(title, description);
  }

  void wirelessOff()
  {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    btStop();
  }

  static const uint8_t MAX_WEATHER_API_RETRIES = 3;

public:
  // Make the weatherInfo's currentDateTime accessible externally:
  long getCurrentDateTime() const
  {
    return weatherInfo.currentDateTime;
  }

  void begin()
  {
    errorMessageBuffer = "";
    delay(1000);
    Serial.begin(BAUD_RATE);
    screenPowerOn();
    EPD_GPIOInit();
  }

  bool run()
  {
    try
    {
      connectToWiFi();

      // Try AQI first while Wi-Fi is definitely on.
      (void)getAirNowAQI(); // best-effort; failures don’t block

      if (!getWeatherInfo(MAX_WEATHER_API_RETRIES))
      {
        char title[] = "Weather API failed.";
        char msg[256];
        memset(msg, 0, sizeof(msg));
        snprintf(msg, sizeof(msg), "%s\n\n(After %d retry attempts)",
                 errorMessageBuffer.c_str(), MAX_WEATHER_API_RETRIES);
        displayErrorMessage(title, msg);
        return false;
      }
      else
      {
        logPrintln("Weather information retrieved successfully.");
        displayWeatherForecast();
        return true;
      }
    }
    catch (const std::exception &e)
    {
      errorMessageBuffer = String("Exception: ") + e.what();
      displayError();
    }
    return false;
  }
};

WeatherCrow weatherCrow;

void setup()
{
  if (LOW_POWER_MODE)
  {
    setCpuFrequencyMhz(80);
    btStop();
  }
  weatherCrow.begin();
}

void loop()
{
  Serial.println("Starting weatherCrow.run()");
  if (weatherCrow.run() == true)
  {
    Serial.println("weatherCrow.run() completed successfully");

    // Simplified deep-sleep scheduling logic: always sleep for REFRESH_MINUITES minutes.
    long now = weatherCrow.getCurrentDateTime();
    long nextWake = now + (REFRESH_MINUITES * 60);  // Fixed sleep interval
    uint64_t secondsToSleep = nextWake - now;
    esp_sleep_enable_timer_wakeup(secondsToSleep * 1000000ULL);
    esp_deep_sleep_start();
  }
  else
  {
    // If retrieval failed, try again in 5 minutes
    esp_sleep_enable_timer_wakeup(1000000ULL * 60ULL * 5);
    esp_deep_sleep_start();
  }
}
