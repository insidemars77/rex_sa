#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Preferences.h>

Preferences prefs;

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ILI9486 _panel_instance;
  lgfx::Bus_Parallel8 _bus_instance;

public:
  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();
      cfg.freq_write = 20000000;
      cfg.pin_wr = 9; cfg.pin_rd = 10; cfg.pin_rs = 8;
      cfg.pin_d0 = 4; cfg.pin_d1 = 5; cfg.pin_d2 = 6; cfg.pin_d3 = 7;
      cfg.pin_d4 = 15; cfg.pin_d5 = 16; cfg.pin_d6 = 17; cfg.pin_d7 = 18;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 11; cfg.pin_rst = 12;
      cfg.panel_width = 320; cfg.panel_height = 480;
      cfg.readable = true; cfg.invert = false; cfg.rgb_order = false;
      cfg.dlen_16bit = false; cfg.bus_shared = true;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;

// ============ WiFi ============
const char* ssid     = "POCO X7 Pro";
const char* password = "77777777";

// Colors
#define BLACK     0x0000
#define WHITE     0xFFFF
#define ORANGE    0xFD20
#define GRAY      0x7BEF
#define DARKGRAY  0x39C7
#define GREEN     0x07E0
#define RED       0xF800
#define PANEL     0x18E3

// ---------------------------------------------------------------------
// Pages: Home, Weather, Stopwatch, RexStatus, Device, Settings = 6 total
// ---------------------------------------------------------------------
const int totalPages = 6;
const int STOPWATCH_PAGE_INDEX = 2;
const int SETTINGS_PAGE_INDEX  = 5;

int currentPage = 0;
unsigned long lastDataUpdate = 0;
unsigned long lastClockUpdate = 0;

bool wifiConnected = false;
bool backbenchLive = false;

int weatherTemp = 0;
String weatherCondition = "Loading...";
String weatherLocation = "Mumbai";

const char* days[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};

// ---------------------------------------------------------------------
// Forecast strip data (index 2 = today).
// ---------------------------------------------------------------------
struct ForecastDay { String dayLabel; String tempStr; bool available; };
ForecastDay forecast[5] = {
  {"", "N/A", false},
  {"", "N/A", false},
  {"", "N/A", false},
  {"", "N/A", false},
  {"", "N/A", false},
};

// ---------------------------------------------------------------------
// Face
// ---------------------------------------------------------------------
// ---- Bot face animation ----
enum BotAnim { BOT_IDLE, BOT_BLINK, BOT_LOOK };
BotAnim botAnim = BOT_IDLE;

unsigned long botLastTick = 0;
unsigned long botNextAction = 0;

int botBlinkPhase = 0;     // 0 open → 1 closing → 2 closed → 3 opening
int botLookOffsetX = 0;    // pupil shift (-4..4)
int botLookTargetX = 0;

const int BOT_CX = 240;
const int BOT_CY = 125;

// ---------------------------------------------------------------------
// url
// ---------------------------------------------------------------------

#define URL_PREFIX     "url>"
#define URLDEL_PREFIX  "urldel>"
#define MAX_URLS 6

struct UrlEntry {
  String url;
  bool live;      // last check result
  bool checked;   // has it been checked at least once
};
UrlEntry urlList[MAX_URLS];
int urlCount = 0;

unsigned long lastUrlCheckTime = 0;
const unsigned long URL_CHECK_INTERVAL = 15000; // check one url every 15s, round-robin
int urlCheckCursor = 0;


// =======================================================================
// INPUT LAYER
// =======================================================================
enum InputEvent { INPUT_NONE, INPUT_PREV, INPUT_MAIN, INPUT_NEXT, INPUT_POWER_TEST };

void fullRedraw();    // forward declare
void addUrl(const String &u);
void removeUrl(const String &u);
void checkUrlStatus(int idx); 

// ===================== NOTIFICATIONS (T-Rex slide-in) =====================
enum NotifState { NOTIF_IDLE, NOTIF_SLIDE_IN, NOTIF_HOLD, NOTIF_SLIDE_OUT };
NotifState notifState = NOTIF_IDLE;

String notifText = "";
const char* NOTIF_PREFIX = "rex>";

int notifDinoX = 480;                       // current left-edge x of the dino sprite
const int NOTIF_DINO_RESTX = 385;           // resting x once fully slid in
const int NOTIF_BAND_Y = 218;               // top of the overlay band (just above footer)
const int NOTIF_BAND_H = 78;

unsigned long notifLastStepTime = 0;
const unsigned long NOTIF_STEP_INTERVAL = 15;   // ms between slide frames
const int NOTIF_STEP_PX = 8;                    // px moved per frame

unsigned long notifHoldStart = 0;
const unsigned long NOTIF_HOLD_MS = 4000;       // how long it stays fully in view

unsigned long notifLastRedraw = 0;
const unsigned long NOTIF_REDRAW_INTERVAL = 40; // throttle so we don't hammer the bus

// Blocky Chrome-dino-style T-Rex, drawn at (x, NOTIF_BAND_Y..). x = left edge.
// Blocky Chrome-style T-Rex skull. x = left edge.
// Exact Chrome-style T-Rex (blocky pixel art). x = left edge.
// Big blocky REX. x = left edge of the text.
void drawTRex(int x)
{
  int y = NOTIF_BAND_Y + 12;
  uint16_t c = ORANGE;

  // ===== R =====
  // Vertical bar
  tft.fillRect(x + 0,  y, 6, 28, c);
  // Top bar
  tft.fillRect(x + 6,  y, 14, 6, c);
  // Middle bar
  tft.fillRect(x + 6,  y + 11, 12, 6, c);
  // Upper curve
  tft.fillRect(x + 16, y + 6, 6, 6, c);
  // Lower leg
  tft.fillRect(x + 12, y + 17, 6, 11, c);
  tft.fillRect(x + 16, y + 22, 6, 6, c);

  // ===== E =====
  // Vertical bar
  tft.fillRect(x + 28, y, 6, 28, c);
  // Top bar
  tft.fillRect(x + 34, y, 16, 6, c);
  // Middle bar
  tft.fillRect(x + 34, y + 11, 14, 6, c);
  // Bottom bar
  tft.fillRect(x + 34, y + 22, 16, 6, c);

  // ===== X =====
  // Left-top to right-bottom
  tft.fillRect(x + 56, y, 6, 8, c);
  tft.fillRect(x + 60, y + 6, 6, 8, c);
  tft.fillRect(x + 64, y + 12, 6, 8, c);
  tft.fillRect(x + 68, y + 18, 6, 10, c);

  // Right-top to left-bottom
  tft.fillRect(x + 68, y, 6, 8, c);
  tft.fillRect(x + 64, y + 6, 6, 8, c);
  tft.fillRect(x + 60, y + 12, 6, 8, c);
  tft.fillRect(x + 56, y + 18, 6, 10, c);
}

void drawSpeechBubble(int dinoX)
{
  int bubbleRight = dinoX - 10;
  int bubbleW = bubbleRight - 20;
  if (bubbleW < 60) return;   // not enough room yet, still sliding in

  int bubbleX = bubbleRight - bubbleW;
  int bubbleY = NOTIF_BAND_Y + 6;
  int bubbleH = 56;

  tft.fillRoundRect(bubbleX, bubbleY, bubbleW, bubbleH, 8, PANEL);
  tft.drawRoundRect(bubbleX, bubbleY, bubbleW, bubbleH, 8, ORANGE);

  // little tail pointing toward the dino's mouth
  tft.fillTriangle(bubbleRight, bubbleY + bubbleH - 18,
                    bubbleRight, bubbleY + bubbleH - 6,
                    bubbleRight + 10, bubbleY + bubbleH - 12,
                    PANEL);
  tft.drawLine(bubbleRight, bubbleY + bubbleH - 18, bubbleRight + 10, bubbleY + bubbleH - 12, ORANGE);
  tft.drawLine(bubbleRight, bubbleY + bubbleH - 6, bubbleRight + 10, bubbleY + bubbleH - 12, ORANGE);

  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  const int maxCharsPerLine = (bubbleW - 16) / 6;
  if (maxCharsPerLine < 5) return;

  int lineY = bubbleY + 10;
  int start = 0, linesDrawn = 0;
  while (start < (int)notifText.length() && linesDrawn < 4)
  {
    int end = start + maxCharsPerLine;
    if (end >= (int)notifText.length())
    {
      end = notifText.length();
    }
    else
    {
      int lastSpace = notifText.lastIndexOf(' ', end);
      if (lastSpace > start) end = lastSpace;
    }
    String lineStr = notifText.substring(start, end);
    lineStr.trim();
    tft.setCursor(bubbleX + 8, lineY);
    tft.print(lineStr);
    lineY += 12;
    start = end;
    linesDrawn++;
  }
}

void drawNotifFrame()
{
  tft.fillRect(0, NOTIF_BAND_Y, 480, NOTIF_BAND_H, BLACK);
  drawSpeechBubble(notifDinoX);
  drawTRex(notifDinoX);
}

void showNotification(const String &text)
{
  notifText = text;
  notifDinoX = 480;
  notifState = NOTIF_SLIDE_IN;
  notifLastStepTime = millis();
}

// Call this every loop(). Redraws the band on top of whatever page is
// underneath, so page navigation or periodic refreshes never erase it
// mid-animation.
void updateNotification()
{
  if (notifState == NOTIF_IDLE) return;
  unsigned long now = millis();

  if (notifState == NOTIF_SLIDE_IN && now - notifLastStepTime >= NOTIF_STEP_INTERVAL)
  {
    notifLastStepTime = now;
    notifDinoX -= NOTIF_STEP_PX;
    if (notifDinoX <= NOTIF_DINO_RESTX)
    {
      notifDinoX = NOTIF_DINO_RESTX;
      notifState = NOTIF_HOLD;
      notifHoldStart = now;
    }
  }
  else if (notifState == NOTIF_HOLD && now - notifHoldStart > NOTIF_HOLD_MS)
  {
    notifState = NOTIF_SLIDE_OUT;
    notifLastStepTime = now;
  }
  else if (notifState == NOTIF_SLIDE_OUT && now - notifLastStepTime >= NOTIF_STEP_INTERVAL)
  {
    notifLastStepTime = now;
    notifDinoX += NOTIF_STEP_PX;
    if (notifDinoX >= 480)
    {
      notifState = NOTIF_IDLE;
      fullRedraw();   // clean restore of whatever's underneath
      return;
    }
  }

  if (now - notifLastRedraw >= NOTIF_REDRAW_INTERVAL)
  {
    notifLastRedraw = now;
    drawNotifFrame();
  }
}

String serialLineBuffer = "";

// ===================== INPUT QUEUE =====================
// Needed because a single serial line like "mm" should produce TWO
// separate INPUT_MAIN events (for double-press detection), but
// pollInput() can only return one event per call.
#define INPUT_QUEUE_SIZE 32
InputEvent inputQueue[INPUT_QUEUE_SIZE];
int inputQueueHead = 0;
int inputQueueTail = 0;

void queuePush(InputEvent ev)
{
  int nextTail = (inputQueueTail + 1) % INPUT_QUEUE_SIZE;
  if (nextTail == inputQueueHead) return;  // queue full, drop it
  inputQueue[inputQueueTail] = ev;
  inputQueueTail = nextTail;
}

bool queuePop(InputEvent &ev)
{
  if (inputQueueHead == inputQueueTail) return false;  // empty
  ev = inputQueue[inputQueueHead];
  inputQueueHead = (inputQueueHead + 1) % INPUT_QUEUE_SIZE;
  return true;
}

void processCommandLine(const String &line)
{
  // Each character in the line is treated as its own command,
  // in order - so "mm" queues two INPUT_MAIN events, "np" queues
  // INPUT_NEXT then INPUT_PREV, etc.
  for (int i = 0; i < (int)line.length(); i++)
  {
    char c = line.charAt(i);
    if (c == 'p' || c == 'P') queuePush(INPUT_PREV);
    else if (c == 'm' || c == 'M') queuePush(INPUT_MAIN);
    else if (c == 'n' || c == 'N') queuePush(INPUT_NEXT);
    else if (c == 'o' || c == 'O') queuePush(INPUT_POWER_TEST);
    // any other character is silently ignored
  }
}

InputEvent pollInput()
{
  InputEvent queued;
  if (queuePop(queued)) return queued;

  while (Serial.available())
  {
    char c = Serial.read();

    if (c == '\n' || c == '\r')
    {
      if (serialLineBuffer.length() > 0)
      {
        String line = serialLineBuffer;
        serialLineBuffer = "";

        Serial.println("[debug] line received: " + line); 

        if (line.startsWith(NOTIF_PREFIX))
        {
          String msg = line.substring(strlen(NOTIF_PREFIX));
          msg.trim();
          showNotification(msg);
        }
        else if (line.startsWith(URLDEL_PREFIX))
        {
          String u = line.substring(strlen(URLDEL_PREFIX));
          u.trim();
          removeUrl(u);
        }
        else if (line.startsWith(URL_PREFIX))
        {
          String u = line.substring(strlen(URL_PREFIX));
          u.trim();
          addUrl(u);
        }
        else
        {
          processCommandLine(line);
          if (queuePop(queued)) return queued;
        }
      }
    }
    else
    {
      if (serialLineBuffer.length() < 200) serialLineBuffer += c;
    }
  }
  return INPUT_NONE;
}

// =======================================================================
// UI CONTROLLER
// =======================================================================
enum UIMode { MODE_PAGE, MODE_OPTION };
UIMode uiMode = MODE_PAGE;

unsigned long lastMainPressTime = 0;
const unsigned long DOUBLE_PRESS_WINDOW_MS = 350;

bool poweredOff = false;

// ---- Settings model: WiFi (real toggle) + Clock (12H/24H, real effect) ----
bool use24Hour = true;

struct SettingOption
{
  const char* label;
  bool isToggle;
  bool boolValue;   // used by WiFi
  int  intValue;    // used by Clock (24 or 12)
};

SettingOption settings[] = {
  { "WiFi",  true,  false, 0  },  // boolValue synced from wifiConnected
  { "Clock", false, false, 24 },
};
const int settingsCount = sizeof(settings) / sizeof(settings[0]);
int selectedOption = 0;

void connectWiFi();   // forward declare

void toggleSetting(int idx)
{
  SettingOption &s = settings[idx];

  if (strcmp(s.label, "WiFi") == 0)
  {
    if (wifiConnected)
    {
      WiFi.disconnect(true);
      wifiConnected = false;
      Serial.println("WiFi turned OFF by user.");
    }
    else
    {
      Serial.println("WiFi turned ON by user - connecting...");
      connectWiFi();   // brief blocking call - acceptable for a deliberate menu action
    }
    s.boolValue = wifiConnected;
  }
  else if (strcmp(s.label, "Clock") == 0)
  {
    s.intValue = (s.intValue == 24) ? 12 : 24;
    use24Hour = (s.intValue == 24);
  }
}

// ---- Stopwatch state ----
bool stopwatchRunning = false;
unsigned long stopwatchStartMillis = 0;
unsigned long stopwatchElapsedMillis = 0;   // frozen value while paused
unsigned long lastStopwatchDraw = 0;

void handlePrev()
{
  if (uiMode == MODE_OPTION && currentPage == SETTINGS_PAGE_INDEX)
  {
    selectedOption--;
    if (selectedOption < 0) selectedOption = settingsCount - 1;
  }
  else
  {
    currentPage--;
    if (currentPage < 0) currentPage = totalPages - 1;
  }
  fullRedraw();
}

void handleNext()
{
  if (uiMode == MODE_OPTION && currentPage == SETTINGS_PAGE_INDEX)
  {
    selectedOption++;
    if (selectedOption >= settingsCount) selectedOption = 0;
  }
  else
  {
    currentPage++;
    if (currentPage >= totalPages) currentPage = 0;
  }
  fullRedraw();
}

// NOTE: same known trade-off as before - the first press of a double
// press can also register as a single-press action a split second
// before it's recognized as a double press. Flag if it feels wrong.
void handleMainPress()
{
  unsigned long now = millis();
  bool isDoublePress = (now - lastMainPressTime) < DOUBLE_PRESS_WINDOW_MS;
  lastMainPressTime = now;

  // ---- Stopwatch page has its own MAIN behavior ----
  if (currentPage == STOPWATCH_PAGE_INDEX)
  {
    if (isDoublePress)
    {
      stopwatchRunning = false;
      stopwatchElapsedMillis = 0;
      lastMainPressTime = 0;
      fullRedraw();
    }
    else
    {
      if (stopwatchRunning)
      {
        stopwatchElapsedMillis = now - stopwatchStartMillis;
        stopwatchRunning = false;
      }
      else
      {
        stopwatchStartMillis = now - stopwatchElapsedMillis;
        stopwatchRunning = true;
      }
      fullRedraw();
    }
    return;
  }

  // ---- Settings page: double-MAIN toggles PAGE_MODE <-> OPTION_MODE ----
  if (isDoublePress && currentPage == SETTINGS_PAGE_INDEX)
  {
    uiMode = (uiMode == MODE_PAGE) ? MODE_OPTION : MODE_PAGE;
    selectedOption = 0;
    lastMainPressTime = 0;
    fullRedraw();
    return;
  }

  if (uiMode == MODE_OPTION && currentPage == SETTINGS_PAGE_INDEX)
  {
    toggleSetting(selectedOption);
    fullRedraw();
  }
}

void handlePowerCombo()
{
  Serial.println("POWER combo triggered (placeholder) - going to OFF state");
  poweredOff = true;

  tft.fillScreen(BLACK);
  tft.setTextColor(GRAY);
  tft.setTextSize(2);
  tft.setCursor(150, 150);
  tft.print("OFF (test)");
  tft.setTextSize(1);
  tft.setCursor(120, 190);
  tft.print("press any key to wake");
}

// =======================================================================
// CLOCK FORMATTING (respects the Clock setting: 24H or 12H)
// =======================================================================
void formatClock(struct tm &ti, char *buf, size_t bufSize, bool &showAmPm, bool &isPm)
{
  isPm = ti.tm_hour >= 12;
  if (use24Hour)
  {
    snprintf(buf, bufSize, "%02d:%02d", ti.tm_hour, ti.tm_min);
    showAmPm = false;
  }
  else
  {
    int h12 = ti.tm_hour % 12;
    if (h12 == 0) h12 = 12;
    snprintf(buf, bufSize, "%02d:%02d", h12, ti.tm_min);
    showAmPm = true;
  }
}

// =======================================================================
// WEATHER + FORECAST FETCHING
// =======================================================================
const char* wdayShort(int wday) { return days[wday]; }

// Returns short weekday label for "today + offsetDays"
String labelForOffset(int offsetDays)
{
  time_t now = time(nullptr);
  time_t target = now + (time_t)offsetDays * 86400;
  struct tm *ti = localtime(&target);
  if (!ti) return "?";
  return String(wdayShort(ti->tm_wday));
}

// ---------------------------------------------------------------------
// Open-Meteo coordinates for Mumbai (no API key needed)
// ---------------------------------------------------------------------
const float LAT = 19.0760;
const float LON = 72.8777;

// Minimal WMO weather-code -> text mapping (Open-Meteo uses these codes
// instead of plain-English conditions like wttr.in did)
String weatherCodeToText(int code)
{
  if (code == 0) return "CLEAR";
  if (code <= 3) return "PARTLY CLOUDY";
  if (code <= 48) return "FOGGY";
  if (code <= 57) return "DRIZZLE";
  if (code <= 67) return "RAIN";
  if (code <= 77) return "SNOW";
  if (code <= 82) return "RAIN SHOWERS";
  if (code <= 86) return "SNOW SHOWERS";
  if (code <= 99) return "THUNDERSTORM";
  return "UNKNOWN";
}

// Fetches TODAY's current conditions + a real 5-day strip
// (-2, -1, today, +1, +2) in one request via Open-Meteo's past_days
// parameter, which actually returns historical daily data - not "N/A".
void fetchAllWeather()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[weather] Skipped - WiFi not connected.");
    return;
  }

  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LAT, 4)
             + "&longitude=" + String(LON, 4)
             + "&current=temperature_2m,weather_code"
             + "&daily=temperature_2m_max,weather_code"
             + "&past_days=2&forecast_days=3&timezone=auto";

  Serial.print("[weather] Requesting: ");
  Serial.println(url);

  http.begin(url);
  http.setTimeout(10000);

  int code = http.GET();
  Serial.printf("[weather] HTTP response code: %d\n", code);

  if (code != 200)
  {
    Serial.printf("[weather] Fetch failed (code %d) - not a 200 OK.\n", code);
    weatherCondition = "UNAVAILABLE";
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  Serial.printf("[weather] Payload length: %d bytes\n", payload.length());
  if (payload.length() == 0)
  {
    Serial.println("[weather] Payload was EMPTY despite HTTP 200.");
    weatherCondition = "UNAVAILABLE";
    return;
  }
  Serial.println("[weather] Payload preview:");
  Serial.println(payload.substring(0, 300));

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, payload);

  if (err)
  {
    Serial.print("[weather] JSON parse FAILED: ");
    Serial.println(err.c_str());
    weatherCondition = "UNAVAILABLE";
    return;
  }

  if (!doc.containsKey("current") || !doc.containsKey("daily"))
  {
    Serial.println("[weather] Parsed OK but missing 'current'/'daily' keys.");
    Serial.println("[weather] Top-level keys received:");
    for (JsonPair kv : doc.as<JsonObject>())
    {
      Serial.println(String("  - ") + kv.key().c_str());
    }
    weatherCondition = "UNAVAILABLE";
    return;
  }

  weatherTemp = (int)round((double)doc["current"]["temperature_2m"]);
  int curCode = doc["current"]["weather_code"];
  weatherCondition = weatherCodeToText(curCode);

  Serial.printf("[weather] Current: %dC, code=%d (%s)\n",
                 weatherTemp, curCode, weatherCondition.c_str());

  JsonArray dailyMax = doc["daily"]["temperature_2m_max"];
  JsonArray dailyTime = doc["daily"]["time"];

  Serial.printf("[weather] Daily array size: %d\n", dailyMax.size());

  for (int i = 0; i < 5 && i < (int)dailyMax.size(); i++)
  {
    float maxTemp = dailyMax[i];
    forecast[i].tempStr = String((int)round(maxTemp)) + "C";
    forecast[i].available = true;

    if (i == 2)
    {
      forecast[i].dayLabel = "TODAY";
    }
    else
    {
      const char* dateStr = dailyTime[i];
      int y, mo, d;
      sscanf(dateStr, "%d-%d-%d", &y, &mo, &d);
      struct tm t = {0};
      t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
      t.tm_hour = 12;
      time_t tt = mktime(&t);
      struct tm *resolved = localtime(&tt);
      forecast[i].dayLabel = resolved ? String(days[resolved->tm_wday]) : "?";
    }

    Serial.printf("[weather] forecast[%d] = %s %s\n",
                   i, forecast[i].dayLabel.c_str(), forecast[i].tempStr.c_str());
  }
}

void addUrl(const String &u)
{
  for (int i = 0; i < urlCount; i++)
    if (urlList[i].url == u) { Serial.println("[url] Already tracking: " + u); return; }

  if (urlCount >= MAX_URLS) { Serial.println("[url] List full, cannot add: " + u); return; }

  urlList[urlCount].url = u;
  urlList[urlCount].live = false;
  urlList[urlCount].checked = false;
  urlCount++;
  Serial.println("[url] Added: " + u);
  saveUrlsToFlash();   // <-- persist immediately
  if (currentPage == 3) fullRedraw();
}

void removeUrl(const String &u)
{
  for (int i = 0; i < urlCount; i++)
  {
    if (urlList[i].url == u)
    {
      for (int j = i; j < urlCount - 1; j++) urlList[j] = urlList[j + 1];
      urlCount--;
      Serial.println("[url] Removed: " + u);
      saveUrlsToFlash();   // <-- persist immediately
      if (currentPage == 3) fullRedraw();
      return;
    }
  }
  Serial.println("[url] Not found: " + u);
}

void checkUrlStatus(int idx)
{
  if (idx < 0 || idx >= urlCount) return;
  if (WiFi.status() != WL_CONNECTED) { urlList[idx].live = false; return; }

  String target = urlList[idx].url;
  if (!target.startsWith("http://") && !target.startsWith("https://"))
    target = "https://" + target;

  HTTPClient http;
  http.begin(target);
  http.setTimeout(6000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int code = http.GET();
  urlList[idx].live = (code > 0 && code < 400);
  urlList[idx].checked = true;
  Serial.printf("[url] %s -> %d (%s)\n", urlList[idx].url.c_str(), code, urlList[idx].live ? "LIVE" : "DOWN");
  http.end();

  if (currentPage == 3) fullRedraw();
}

void updateUrlChecks()
{
  if (poweredOff || urlCount == 0) return;
  unsigned long now = millis();
  if (now - lastUrlCheckTime < URL_CHECK_INTERVAL) return;
  lastUrlCheckTime = now;

  checkUrlStatus(urlCheckCursor);
  urlCheckCursor = (urlCheckCursor + 1) % urlCount;
}

void saveUrlsToFlash()
{
  String joined = "";
  for (int i = 0; i < urlCount; i++)
  {
    joined += urlList[i].url;
    if (i < urlCount - 1) joined += "|";
  }

  prefs.begin("rex-urls", false);   // false = read/write mode
  prefs.putString("list", joined);
  prefs.end();

  Serial.println("[url] Saved to flash: " + joined);
}

void loadUrlsFromFlash()
{
  prefs.begin("rex-urls", true);    // true = read-only mode
  String joined = prefs.getString("list", "");
  prefs.end();

  urlCount = 0;
  if (joined.length() == 0)
  {
    Serial.println("[url] No saved URLs found in flash.");
    return;
  }

  int start = 0;
  while (start < (int)joined.length() && urlCount < MAX_URLS)
  {
    int sep = joined.indexOf('|', start);
    String piece = (sep == -1) ? joined.substring(start) : joined.substring(start, sep);
    piece.trim();

    if (piece.length() > 0)
    {
      urlList[urlCount].url = piece;
      urlList[urlCount].live = false;
      urlList[urlCount].checked = false;
      urlCount++;
    }

    if (sep == -1) break;
    start = sep + 1;
  }

  Serial.println("[url] Loaded " + String(urlCount) + " URL(s) from flash.");
}


// =======================================================================
// RENDERING
// =======================================================================
void drawHeader(const char* subtitle)
{
  tft.fillRect(0, 0, 480, 46, BLACK);
  tft.fillCircle(22, 23, 15, ORANGE);
  tft.setTextColor(BLACK); tft.setTextSize(2);
  tft.setCursor(15, 16); tft.print("R");

  tft.setTextColor(WHITE); tft.setTextSize(3);
  tft.setCursor(48, 7); tft.print("REX");

  tft.setTextColor(ORANGE); tft.setTextSize(1);
  tft.setCursor(50, 32); tft.print(subtitle);

  tft.fillCircle(450, 18, 5, wifiConnected ? GREEN : RED);
}

void drawFooter()
{
  tft.fillRect(0, 297, 480, 23, BLACK);
  tft.drawFastHLine(0, 297, 480, DARKGRAY);

  int startX = 175;
  for (int i = 0; i < totalPages; i++)
  {
    int x = startX + i * 30;
    if (i == currentPage) tft.fillCircle(x, 309, 4, ORANGE);
    else tft.drawCircle(x, 309, 4, GRAY);
  }
}

void card(int x, int y, int w, int h)
{
  tft.fillRoundRect(x, y, w, h, 7, PANEL);
  tft.drawRoundRect(x, y, w, h, 7, DARKGRAY);
}

// ---- Page 1: Home (bot face + time only) ----
// Add these near your other color defines if you don't have them yet
#define ORANGE_HL  tft.color565(255, 220, 150)  // warm highlight, near-white orange
#define BLUSH      tft.color565(120, 40, 10)    // dim warm blush, subtle against black

void drawBotFaceFrame()
{
  const int cx = BOT_CX;
  const int cy = BOT_CY + 10;

  // Clear only the face area
  tft.fillRect(cx - 90, cy - 70, 180, 140, BLACK);

  // ---- Eye geometry ----
  int eyeW = 46;
  int eyeH = (botBlinkPhase == 2) ? 6 : 58;

  int eyeY = cy + 6;
  if (botBlinkPhase == 1 || botBlinkPhase == 3)
    eyeY += 3;

  int leftX  = cx - 48;
  int rightX = cx + 48;

  // ---- Blush (draw first, sits behind everything) ----
  tft.fillEllipse(leftX,  eyeY + 34, 20, 10, BLUSH);
  tft.fillEllipse(rightX, eyeY + 34, 20, 10, BLUSH);

  // ---- Eye sockets ----
  tft.fillEllipse(leftX,  eyeY, eyeW / 2, eyeH / 2, ORANGE);
  tft.fillEllipse(rightX, eyeY, eyeW / 2, eyeH / 2, ORANGE);

  // ---- Pupils + sparkle highlight ----
  if (botBlinkPhase != 2)
  {
    int px = botLookOffsetX;
    int pupilW = 16;
    int pupilH = 20;
    int pupilY = eyeY + 8;

    // Pupil
    tft.fillEllipse(leftX  + px, pupilY, pupilW / 2, pupilH / 2, BLACK);
    tft.fillEllipse(rightX + px, pupilY, pupilW / 2, pupilH / 2, BLACK);

    // Sparkle: small bright dot, upper-left of each pupil.
    // This one detail reads as "cute" more than anything else here —
    // it's what makes the eyes look wet/alive instead of flat dots.
    int hlX = -3, hlY = -5;
    tft.fillCircle(leftX  + px + hlX, pupilY + hlY, 3, ORANGE_HL);
    tft.fillCircle(rightX + px + hlX, pupilY + hlY, 3, ORANGE_HL);

    // Tiny secondary sparkle, smaller and lower-right — adds depth
    tft.fillCircle(leftX  + px - hlX + 2, pupilY - hlY + 3, 1, ORANGE_HL);
    tft.fillCircle(rightX + px - hlX + 2, pupilY - hlY + 3, 1, ORANGE_HL);
  }
}

void updateBotFace()
{
  if (currentPage != 0 || poweredOff) return;

  unsigned long now = millis();

  // Schedule next idle action
  if (botAnim == BOT_IDLE && now >= botNextAction)
  {
    if (random(0, 100) < 55)
    {
      botAnim = BOT_BLINK;
      botBlinkPhase = 1;
      botLastTick = now;
    }
    else
    {
      botAnim = BOT_LOOK;
      botLookTargetX = random(-4, 5);
      botLastTick = now;
    }
    botNextAction = now + random(1800, 4200);
  }

  // Blink animation
  if (botAnim == BOT_BLINK && now - botLastTick >= 55)
  {
    botLastTick = now;
    botBlinkPhase++;
    if (botBlinkPhase > 3)
    {
      botBlinkPhase = 0;
      botAnim = BOT_IDLE;
    }
    drawBotFaceFrame();
  }

  // Look left/right (pupils ease toward target)
  if (botAnim == BOT_LOOK && now - botLastTick >= 40)
  {
    botLastTick = now;
    if (botLookOffsetX < botLookTargetX) botLookOffsetX++;
    else if (botLookOffsetX > botLookTargetX) botLookOffsetX--;
    else
    {
      static unsigned long holdStart = 0;
      if (holdStart == 0) holdStart = now;
      if (now - holdStart > 500)
      {
        holdStart = 0;
        if (botLookTargetX != 0)
        {
          botLookTargetX = 0;
        }
        else
        {
          botAnim = BOT_IDLE;
        }
      }
    }
    drawBotFaceFrame();
  }
}

void pageHome()
{
  drawHeader("REX");

  botBlinkPhase = 0;
  botLookOffsetX = 0;
  botAnim = BOT_IDLE;
  botNextAction = millis() + 1000;
  drawBotFaceFrame();

  struct tm ti;
  bool ok = getLocalTime(&ti, 50);

  tft.setTextColor(WHITE);
  tft.setTextSize(4);

  if (ok)
  {
    char t[8];
    bool showAmPm, isPm;
    formatClock(ti, t, sizeof(t), showAmPm, isPm);

    int textW = strlen(t) * 24;
    tft.setCursor((480 - textW) / 2, 220);
    tft.print(t);

    if (showAmPm)
    {
      tft.setTextColor(ORANGE);
      tft.setTextSize(2);
      tft.setCursor((480 - textW) / 2 + textW + 8, 230);
      tft.print(isPm ? "PM" : "AM");
    }
  }
  else
  {
    tft.setCursor(180, 220);
    tft.print("--:--");
  }

  tft.setTextColor(GRAY);
  tft.setTextSize(1);
  tft.setCursor(210, 265);
  tft.print(ok ? days[ti.tm_wday] : "---");

  drawFooter();
}

void updateClock()
{
  if (currentPage != 0 || poweredOff) return;
  if (millis() - lastClockUpdate < 1000) return;
  lastClockUpdate = millis();

  struct tm ti;
  if (!getLocalTime(&ti, 10)) return;

  // Only clear the time strip (under the face)
  tft.fillRect(60, 215, 360, 60, BLACK);

  tft.setTextColor(WHITE);
  tft.setTextSize(4);

  char t[8];
  bool showAmPm, isPm;
  formatClock(ti, t, sizeof(t), showAmPm, isPm);

  int textW = strlen(t) * 24;
  tft.setCursor((480 - textW) / 2, 220);
  tft.print(t);

  if (showAmPm)
  {
    tft.setTextColor(ORANGE);
    tft.setTextSize(2);
    tft.setCursor((480 - textW) / 2 + textW + 8, 230);
    tft.print(isPm ? "PM" : "AM");
  }

  tft.setTextColor(GRAY);
  tft.setTextSize(1);
  tft.setCursor(210, 265);
  tft.print(days[ti.tm_wday]);
}

// ---- Page 2: Weather (with 5-day forecast strip) ----
void pageWeather()
{
  drawHeader("WEATHER");
  card(12, 55, 456, 165);

  tft.setTextColor(ORANGE); tft.setTextSize(5);
  tft.setCursor(170, 80);
  tft.printf("%dC", weatherTemp);

  tft.setTextColor(WHITE); tft.setTextSize(2);
  tft.setCursor(140, 145);
  tft.print(weatherCondition);

  tft.setTextColor(GRAY); tft.setTextSize(1);
  tft.setCursor(170, 180); tft.print(weatherLocation);

  // Forecast strip
  card(12, 228, 456, 60);
  int cellW = 456 / 5;
  for (int i = 0; i < 5; i++)
  {
    int cx = 12 + i * cellW + cellW / 2;

    tft.setTextColor(i == 2 ? ORANGE : GRAY);
    tft.setTextSize(1);
    tft.setCursor(cx - 14, 235);
    tft.print(forecast[i].dayLabel);

    tft.setTextColor(forecast[i].available ? WHITE : DARKGRAY);
    tft.setCursor(cx - 14, 258);
    tft.print(forecast[i].tempStr);
  }

  drawFooter();
}

// ---- Page 3: Stopwatch ----
void formatStopwatch(unsigned long ms, char* buf, size_t bufSize)
{
  unsigned long totalHundredths = ms / 10;
  unsigned long hundredths = totalHundredths % 100;
  unsigned long totalSeconds = totalHundredths / 100;
  unsigned long seconds = totalSeconds % 60;
  unsigned long minutes = totalSeconds / 60;
  snprintf(buf, bufSize, "%02lu:%02lu.%02lu", minutes, seconds, hundredths);
}

void pageStopwatch()
{
  drawHeader("STOPWATCH");
  card(12, 55, 456, 225);

  unsigned long elapsed = stopwatchRunning
                            ? (millis() - stopwatchStartMillis)
                            : stopwatchElapsedMillis;

  char buf[16];
  formatStopwatch(elapsed, buf, sizeof(buf));

  tft.setTextColor(ORANGE); tft.setTextSize(4);
  int textW = strlen(buf) * 24;
  tft.setCursor((480 - textW) / 2, 110);
  tft.print(buf);

  tft.setTextColor(stopwatchRunning ? GREEN : GRAY);
  tft.setTextSize(2);
  tft.setCursor(190, 175);
  tft.print(stopwatchRunning ? "RUNNING" : "STOPPED");

  tft.setTextColor(GRAY); tft.setTextSize(1);
  tft.setCursor(70, 230);
  tft.print("MAIN: start/stop   double-MAIN: reset");

  drawFooter();
}

// Live-updates ONLY the time readout while running - no full redraw.
void updateStopwatch()
{
  if (currentPage != STOPWATCH_PAGE_INDEX || poweredOff) return;
  if (!stopwatchRunning) return;
  if (millis() - lastStopwatchDraw < 50) return;
  lastStopwatchDraw = millis();

  unsigned long elapsed = millis() - stopwatchStartMillis;
  char buf[16];
  formatStopwatch(elapsed, buf, sizeof(buf));

  tft.fillRect(60, 100, 360, 45, PANEL);
  tft.setTextColor(ORANGE); tft.setTextSize(4);
  int textW = strlen(buf) * 24;
  tft.setCursor((480 - textW) / 2, 110);
  tft.print(buf);
}

// ---- Page 4: Rex Status ----
void pageUrlList()
{
  drawHeader("URL CHECKER");
  card(12, 55, 456, 225);

  if (urlCount == 0)
  {
    tft.setTextColor(GRAY);
    tft.setTextSize(2);
    tft.setCursor(90, 150);
    tft.print("No URLs tracked");
    tft.setTextSize(1);
    tft.setCursor(50, 190);
    tft.print("Send: url>www.example.com");
  }
  else
  {
    int rowH = 225 / MAX_URLS;
    int y = 65;
    for (int i = 0; i < urlCount; i++)
    {
      uint16_t dotColor = !urlList[i].checked ? GRAY : (urlList[i].live ? GREEN : RED);
      tft.fillCircle(30, y + 8, 5, dotColor);

      tft.setTextColor(WHITE);
      tft.setTextSize(1);
      tft.setCursor(46, y + 3);

      String display = urlList[i].url;
      if (display.length() > 58) display = display.substring(0, 55) + "...";
      tft.print(display);

      y += rowH;
    }
  }

  drawFooter();
}

// ---- Page 5: Device ----
void pageDevice()
{
  drawHeader("DEVICE");
  card(12, 55, 456, 225);

  tft.setTextColor(GRAY); tft.setTextSize(1);
  tft.setCursor(40, 80); tft.print("WIFI STATUS");
  tft.setTextColor(wifiConnected ? GREEN : RED);
  tft.setCursor(280, 80);
  tft.print(wifiConnected ? "CONNECTED" : "OFFLINE");

  tft.setTextColor(GRAY); tft.setCursor(40, 115); tft.print("SSID");
  tft.setTextColor(WHITE);
  tft.setCursor(280, 115);
  tft.print(wifiConnected ? WiFi.SSID() : "N/A");

  tft.setTextColor(GRAY); tft.setCursor(40, 150); tft.print("IP ADDRESS");
  tft.setTextColor(WHITE);
  tft.setCursor(280, 150);
  tft.print(wifiConnected ? WiFi.localIP().toString() : "N/A");

  tft.setTextColor(GRAY); tft.setCursor(40, 185); tft.print("SIGNAL (RSSI)");
  tft.setTextColor(WHITE);
  tft.setCursor(280, 185);
  if (wifiConnected) tft.printf("%d dBm", WiFi.RSSI());
  else tft.print("N/A");

  tft.setTextColor(GRAY); tft.setCursor(40, 220); tft.print("FREE RAM");
  char ram[16];
  sprintf(ram, "%d KB", ESP.getFreeHeap()/1024);
  tft.setTextColor(ORANGE); tft.setCursor(280, 220); tft.print(ram);

  drawFooter();
}

// ---- Page 6: Settings - WiFi (real toggle) + Clock (real effect) ----
void pageSettings()
{
  drawHeader(uiMode == MODE_OPTION ? "SETTINGS (OPTION MODE)" : "SETTINGS");
  card(12, 55, 456, 225);

  int y = 90;
  for (int i = 0; i < settingsCount; i++)
  {
    bool isSelected = (uiMode == MODE_OPTION && i == selectedOption);

    tft.setTextColor(isSelected ? ORANGE : GRAY);
    tft.setTextSize(2);
    tft.setCursor(30, y);
    tft.print(isSelected ? "> " : "  ");
    tft.print(settings[i].label);

    tft.setTextColor(isSelected ? ORANGE : WHITE);
    tft.setCursor(320, y);
    if (settings[i].isToggle)
    {
      tft.print(settings[i].boolValue ? "ON" : "OFF");
    }
    else
    {
      tft.printf("%dH", settings[i].intValue);
    }

    y += 45;
  }

  tft.setTextColor(GRAY);
  tft.setTextSize(1);
  tft.setCursor(30, y + 15);
  tft.print(uiMode == MODE_OPTION
             ? "PREV/NEXT: move  MAIN: toggle  dbl-MAIN: exit"
             : "dbl-MAIN: enter option mode");

  drawFooter();
}

void fullRedraw()
{
  tft.fillScreen(BLACK);

  switch (currentPage)
  {
    case 0: pageHome(); break;
    case 1: pageWeather(); break;
    case 2: pageStopwatch(); break;
    case 3: pageUrlList(); break;
    case 4: pageDevice(); break;
    case 5: pageSettings(); break;
  }
}

void connectWiFi()
{
  Serial.println("\nScanning WiFi networks...");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();

  if (n <= 0)
  {
    Serial.println("No WiFi networks found.");
    wifiConnected = false;
    return;
  }

  for (int i = 0; i < n; i++)
  {
    String foundSSID = WiFi.SSID(i);
    wifi_auth_mode_t auth = WiFi.encryptionType(i);

    if (auth == WIFI_AUTH_OPEN)
    {
      WiFi.begin(foundSSID.c_str());

      int tries = 0;
      while (WiFi.status() != WL_CONNECTED && tries < 20)
      {
        delay(500);
        tries++;
      }

      if (WiFi.status() == WL_CONNECTED)
      {
        wifiConnected = true;
        Serial.println("WiFi Connected: " + foundSSID);
        configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
        WiFi.scanDelete();
        return;
      }
      WiFi.disconnect();
    }
  }

  WiFi.scanDelete();
  wifiConnected = false;
  Serial.println("No usable open WiFi network found.");
}

void setup()
{
  void fullRedraw();
  void addUrl(const String &u);
  void removeUrl(const String &u);
  void checkUrlStatus(int idx);
  void saveUrlsToFlash();
  void loadUrlsFromFlash();

  Serial.begin(115200);
  Serial.println("Rex UI ready. Serial commands: p=PREV  m=MAIN  n=NEXT  o=power-combo test  rex>msg=notification");

  loadUrlsFromFlash();

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(BLACK);
  tft.setTextColor(ORANGE); tft.setTextSize(5);
  tft.setCursor(160, 115); tft.print("REX");
  tft.setTextColor(GRAY); tft.setTextSize(2);
  tft.setCursor(175, 175); tft.print("by mars");
  delay(1200);

  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE); tft.setTextSize(2);
  tft.setCursor(90, 140); tft.print("Connecting WiFi...");
  connectWiFi();
  settings[0].boolValue = wifiConnected;

  if (wifiConnected) {
    tft.setCursor(90, 180); tft.print("Fetching data...");
    fetchAllWeather();
  }

  fullRedraw();
  lastDataUpdate = millis();
}

void loop()
{
  updateClock();
  updateStopwatch();
  updateBotFace();
  updateUrlChecks();

  if (!poweredOff && wifiConnected && millis() - lastDataUpdate > 300000)
  {
    lastDataUpdate = millis();
    fetchAllWeather();
    if (currentPage == 0 || currentPage == 1 || currentPage == 3 || currentPage == 4)
      fullRedraw();
  }

  InputEvent ev = pollInput();

  if (poweredOff)
  {
    if (ev != INPUT_NONE)
    {
      poweredOff = false;
      fullRedraw();
    }
    updateNotification();
    return;
  }

  switch (ev)
  {
    case INPUT_PREV:       handlePrev(); break;
    case INPUT_NEXT:       handleNext(); break;
    case INPUT_MAIN:       handleMainPress(); break;
    case INPUT_POWER_TEST: handlePowerCombo(); break;
    default: break;
  }

  updateNotification();
}