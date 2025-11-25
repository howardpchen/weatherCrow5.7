// Configuration file for weatherCrow5.7

// Network configuration
#define WIFI_SSID "Your WiFi SSID"
#define WIFI_PASSWORD "Your WiFi Password"

// API keys (keep these out of source control)
#define OPEN_WEATHER_MAP_API_KEY "owm_api_key_here"
#define AIRNOW_API_KEY "airnow_api_key_here"  // leave empty to skip AQI

// choose metric or imperial
#define UNITS "metric"

// Location, use google maps to get the latitude and longitude of your location, https://www.google.com/maps
#define LATITUDE "20.4255911"
#define LONGITUDE "136.0809294"

// kyoto
// #define LATITUDE "35.09"
// #define LONGITUDE "135.55"

// Night-time window configuration (optional)
// #define NIGHT_START_HOUR 22  // 10 PM
// #define NIGHT_END_HOUR   6   // 6 AM

// Refresh rate for the weather data in minuites (min:1)
#define REFRESH_MINUITES 60

// Fallback default timezone offset in seconds (e.g. Eastern Time: -5 hours from UTC)
#define DEFAULT_TIMEZONE_OFFSET -18000

// location name
#define LOCATION_NAME "Toronto"

// Warning for the high UV index
#define UVI_THRESHOLD 3

// Forecast hour interval (min:1 to max:8)
#define HOUR_INTERVAL 3

// Low power mode, when you powring the device with battery set to true
#define LOW_POWER_MODE false
