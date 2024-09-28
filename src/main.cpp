#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "epd_driver.h"
#include "roboto12.h"
#include "roboto20.h"
#include "roboto30.h"
#include "icons.h"
#include "Logger.h"
#include "Secrets.h"
#include "Defines.h"
#include "esp_adc_cal.h"
#include "weatherIcon72.h"

uint8_t *framebuffer;

#define BUTTON_WAKEUP_PIN_BITMASK 0x800000000

#define SEPERATOR_THICKNESS 4
#define STATUS_BAR_SEPERATOR_HEIGHT_OFFSET 12

#define DISPLAY_REFRESH_RATE_SECS 120

#define BATT_PIN 36
#define BATT_MIN_OFFSET 3.1
#define BATT_MAX_OFFSET 4.2

long startAt = millis();
int vref = 1100;

RTC_DATA_ATTR int32_t channel = -1;
RTC_DATA_ATTR uint8_t bssid[6];

const uint8_t * translateIcon(String icon_id)
{
    if (icon_id == "0d") 
        return wi_day_sunny_data;
    if (icon_id == "0n") 
        return wi_night_clear_data;
    if (icon_id == "1d" || icon_id == "2d") 
        return wi_day_sunny_overcast_data;
    if (icon_id == "1n" || icon_id == "2n") 
        return wi_night_partly_cloudy_data;
    if (icon_id == "3d") 
        return wi_day_cloudy_data;
    if (icon_id == "3n") 
        return wi_night_cloudy_data;
    if (icon_id == "45d" || icon_id == "48d") 
        return wi_day_fog_data;
    if (icon_id == "45n" || icon_id == "48n") 
        return wi_night_fog_data;
    if (icon_id == "51d" || icon_id == "53d" || icon_id == "55d" || icon_id == "56d" || icon_id == "57d") 
        return wi_day_sprinkle_data;
    if (icon_id == "51n" || icon_id == "53n" || icon_id == "55n" || icon_id == "56n" || icon_id == "57n") 
        return wi_night_alt_sprinkle_data;
    if (icon_id == "61d" || icon_id == "63d" || icon_id == "65d" || icon_id == "66d" || icon_id == "67d") 
        return wi_day_rain_data;
    if (icon_id == "61n" || icon_id == "63n" || icon_id == "65n" || icon_id == "66n" || icon_id == "67n") 
        return wi_night_alt_rain_data;
    if (icon_id == "71d" || icon_id == "73d" || icon_id == "75d" || icon_id == "77d" || icon_id == "85d") 
        return wi_day_snow_data;
    if (icon_id == "71n" || icon_id == "73n" || icon_id == "75n" || icon_id == "77n" || icon_id == "85n") 
        return wi_night_snow_data;
    if (icon_id == "80d" || icon_id == "81d" || icon_id == "82d") 
        return wi_day_showers_data;
    if (icon_id == "80n" || icon_id == "81n" || icon_id == "82n") 
        return wi_night_alt_showers_data;
    if (icon_id == "95d" || icon_id == "96d" || icon_id == "99d") 
        return wi_day_lightning_data;
    if (icon_id == "95n" || icon_id == "96n" || icon_id == "99n") 
        return wi_night_alt_lightning_data;

    return wi_day_sunny_data;
}

void drawStatusBar(int y, String updated, float batteryVoltage)
{
    int x = 20;
    const char *updatedChar = String("Updated: " + updated).c_str();
    writeln((GFXfont *)&Roboto12, updatedChar, &x, &y, framebuffer);

    // Battery outline
    int batX = EPD_WIDTH - 70;
    int batY = y - 17;
    int batW = 50;
    int batH = 20;

    epd_draw_rect(batX, batY, batW, batH, 0, framebuffer);
    int connectorH = 8;
    int connectorW = 4;
    for (int i = 0; i < connectorH; i++)
    {
        epd_draw_hline(batX + batW, batY + batH / 2 - (connectorH / 2) + i, connectorW, 0, framebuffer);
    }

    float normalizedVoltage = batteryVoltage - BATT_MIN_OFFSET;
    // Just ensure that > 4.2 is shown as 100.
    int level = min((int)(100 / (BATT_MAX_OFFSET - BATT_MIN_OFFSET) * normalizedVoltage), 100);
    Logger.log("Battery voltage %f", batteryVoltage);
    Logger.log("Battery level %i", level);
    int levelW = (batW - 4) * level / 100;

    // Battery level
    for (int i = 0; i < batH - 4; i++)
    {
        epd_draw_hline(batX + 2, batY + 2 + i, levelW, 0, framebuffer);
    }

    char cstr[5];
    sprintf(cstr, "%d%%", level);
    int batPerX = batX - 65;
    int batPerY = y + 2;
    writeln((GFXfont *)&Roboto12, cstr, &batPerX, &batPerY, framebuffer);

    for (int i = 0; i < SEPERATOR_THICKNESS; i++)
    {
        epd_draw_hline(0, y + STATUS_BAR_SEPERATOR_HEIGHT_OFFSET + i, EPD_WIDTH, 0, framebuffer);
    }
}

void drawRoom(int x, int y, float humidity, float temperature, const char *title, bool windowOpen)
{
    int titleX = x + 50;
    int titleY = y - 20;
    writeln((GFXfont *)&Roboto20, title, &titleX, &titleY, framebuffer);

    Rect_t humidityArea = {
        .x = x,
        .y = y,
        .width = humidity_width,
        .height = humidity_height};
    epd_copy_to_framebuffer(humidityArea, (uint8_t *)humidity_data, framebuffer);

    int humValX = x + humidity_width + 10;
    int humValY = y + humidity_height / 2 + 15;
    char humidityChar[10];
    snprintf(humidityChar, sizeof(humidityChar), "%0.1f %%", humidity);
    writeln((GFXfont *)&Roboto20, humidityChar, &humValX, &humValY, framebuffer);

    Rect_t temperatureArea = {
        .x = x + 8,
        .y = y + humidity_height + 20,
        .width = temperature_width,
        .height = temperature_height};
    epd_copy_to_framebuffer(temperatureArea, (uint8_t *)temperature_data, framebuffer);
    // https://github.com/erikflowers/weather-icons/tree/master/svg
    int tempValX = x + humidity_width + 10;
    int tempValY = temperatureArea.y + temperature_height / 2 + 15;

    char tempChar[10];
    snprintf(tempChar, sizeof(tempChar), "%0.1f °C", temperature);
    writeln((GFXfont *)&Roboto20, tempChar, &tempValX, &tempValY, framebuffer);

    if (windowOpen) {
        Rect_t windowArea = {
            .x = titleX + 27,
            .y = titleY - 38,
            .width = open_window_width,
            .height = open_window_height};
        epd_copy_to_framebuffer(windowArea, (uint8_t *)open_window_data, framebuffer);
    }
}

uint8_t* getBSSID(String ssid)
{
    int networksFound = WiFi.scanNetworks();
    int i;
    for (i = 0; i < networksFound; i++)
    {
        if (ssid == WiFi.SSID(i))
        {
            return WiFi.BSSID(i);
        }
    }
    return NULL;
}

void connectToWifi()
{
    WiFi.hostname(HOSTNAME);
    WiFi.mode(WIFI_STA);

    IPAddress local_IP(192, 168, 178, 56);
	IPAddress gateway(192, 168, 178, 1);
	IPAddress subnet(255, 255, 255, 0);
	WiFi.config(local_IP, gateway, subnet);
	WiFi.setAutoConnect(true);
	WiFi.setAutoReconnect(true);
	WiFi.persistent(true);

    if (channel < 0)
    {
        WiFi.begin(WIFI_SSID, WIFI_PASSWD);
    }
    else
    {
        Logger.log("Connect with channel %i", channel);
        Logger.log("Connect with BSSID %s", bssid);
        WiFi.begin(WIFI_SSID, WIFI_PASSWD, channel, bssid);
    }

    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Logger.log("Could not connect to WiFi. Channel changed?");
        channel = -1;
        WiFi.begin(WIFI_SSID, WIFI_PASSWD);

        if (WiFi.waitForConnectResult() != WL_CONNECTED)
        {
            Logger.log("Could not connect to WiFi. Try later...");
            ESP.deepSleep(DISPLAY_REFRESH_RATE_SECS * 1e6);
        }
    }

    if (channel == -1) {
        channel = WiFi.channel();
        uint8_t* currentBssid = getBSSID(WIFI_SSID);

        bssid[0] = currentBssid[0];
        bssid[1] = currentBssid[1];
        bssid[2] = currentBssid[2];
        bssid[3] = currentBssid[3];
        bssid[4] = currentBssid[4];
        bssid[5] = currentBssid[5];
    }

    Logger.log("...Connected! IP Address: %s", WiFi.localIP().toString().c_str());
    Logger.log("DNS IP Address: %s", WiFi.dnsIP().toString().c_str());
    Logger.log("Signal strength: %i", WiFi.RSSI());
}

DynamicJsonDocument retrieveData()
{
    HTTPClient http;

    http.begin(DASHBOARD_URL);
    http.GET();
    String payload = http.getString();
    Serial.println(payload);
    DynamicJsonDocument doc(64 * 1024);
    DeserializationError error = deserializeJson(doc, payload);

    if (error)
    {
        http.begin(DASHBOARD_LOG_URL);
        http.POST(error.c_str());
        ESP.deepSleep(DISPLAY_REFRESH_RATE_SECS * 1e6);
    }

    if (doc.isNull())
    {
        http.begin(DASHBOARD_LOG_URL);
        http.POST("document is null");
        ESP.deepSleep(DISPLAY_REFRESH_RATE_SECS * 1e6);
    }

    return doc;
}

float readBatteryLevel()
{
    // Correct the ADC reference voltage
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
    {
        vref = adc_chars.vref;
    }

    delay(10); // Make adc measurement more accurate
    uint16_t v = analogRead(BATT_PIN);
    return ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
}

void drawHourForecast(DynamicJsonDocument &doc, int startY)
{
    int y = startY + 160;

    for (int i = 0; i < 5; i++)
    {
        int hourX = 40 + (i * 180);
        // Icon
        Rect_t windowArea = {
            .x = hourX + 5,
            .y = y - 110,
            .width = 142,
            .height = 142};

        const uint8_t* sas = translateIcon(doc["weather"]["hourly"][i]["icon"].as<char *>());
        // writeln((GFXfont *)&WeatherIcons72, sas.c_str(), &hourX, &y, framebuffer);
        epd_copy_to_framebuffer(windowArea, (uint8_t *)sas, framebuffer);

        // Time
        int timeY = startY + 50;
        int timeX = hourX + 28;
        writeln((GFXfont *)&Roboto20, doc["weather"]["hourly"][i]["time"].as<char *>(), &timeX, &timeY, framebuffer);

        // Propability
        int propY = startY + 210;
        int propX = hourX + 50;
        char propability[10];
        snprintf(propability, sizeof(propability), "%i %%", doc["weather"]["hourly"][i]["propability"].as<int>());
        writeln((GFXfont *)&Roboto12, propability, &propX, &propY, framebuffer);
    }
}

void draw(DynamicJsonDocument &doc)
{
    epd_init();

    framebuffer = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM);
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    int statusY = 32;
    drawStatusBar(statusY, doc["updated"], readBatteryLevel());

    int x = 20;
    int width = (EPD_WIDTH - x) / 3;

    int statusEndY = statusY + SEPERATOR_THICKNESS + STATUS_BAR_SEPERATOR_HEIGHT_OFFSET;
    int roomSeperatorHeight = EPD_HEIGHT - statusEndY - 270;
    for (int i = 0; i < SEPERATOR_THICKNESS; i++)
    {
        epd_draw_vline(x + width - 20 - i, statusEndY, roomSeperatorHeight, 0, framebuffer);
        epd_draw_vline(x + 2 * width - 20 - i, statusEndY, roomSeperatorHeight, 0, framebuffer);
    }

    drawRoom(x + 40, 110, doc["outdoor-temp-sensor"]["humidity"].as<float>(), doc["outdoor-temp-sensor"]["temperature"].as<float>(), "Balkon", false);
    drawRoom(x + width + 40, 110, doc["living-temp-sensor"]["humidity"].as<float>(), doc["living-temp-sensor"]["temperature"].as<float>(), "Wohn", false);
    drawRoom(x + width + width + 40, 110, doc["sleep-temp-sensor"]["humidity"].as<float>(), doc["sleep-temp-sensor"]["temperature"].as<float>(), "Schlaf", false);

    // Weather h separator
    for (int i = 0; i < SEPERATOR_THICKNESS; i++)
    {
        epd_draw_hline(0, roomSeperatorHeight + statusEndY + i, EPD_WIDTH, 0, framebuffer);
    }

    int weatherForecastY = roomSeperatorHeight + statusEndY + SEPERATOR_THICKNESS;

    drawHourForecast(doc, weatherForecastY);

    epd_poweron();
    epd_clear_area_cycles(epd_full_screen(), 1, 50);

    // Render it twice will increase the contrast when there is no voltage on the display.
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
}

void setup()
{
    Serial.begin(115200);
    long wifiStart = millis();
    connectToWifi();
    Logger.log("Wifi connnect took %i ms.", (millis() - wifiStart));

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason)
    {
    case ESP_SLEEP_WAKEUP_EXT1:
    {
        Logger.log("Wakeup caused by external signal using RTC_CNTL");
        uint64_t GPIO_reason = esp_sleep_get_ext1_wakeup_status();
        Logger.log("GPIO Reason: ", GPIO_reason);
        Serial.print("GPIO that triggered the wake up: GPIO ");
        Serial.println((log(GPIO_reason)) / log(2), 0);
        break;
    }
    default:
        Logger.log("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
        break;
    }

    DynamicJsonDocument data = retrieveData();
    draw(data);
    epd_poweroff();
    epd_poweroff_all();
    Logger.log("Going to deep sleep");
    esp_sleep_enable_ext1_wakeup(BUTTON_WAKEUP_PIN_BITMASK, ESP_EXT1_WAKEUP_ALL_LOW);
    ESP.deepSleep(DISPLAY_REFRESH_RATE_SECS * 1e6);
}

void loop()
{
    delay(3000);
}