#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <Adafruit_NeoPixel.h>

// DIN of the WS2812B strip is connected to D4 through about 330 Ohm.
// D4 is GPIO2 on the Wemos D1 mini.
const uint8_t DATA_PIN = D4;
const uint16_t MAX_LEDS = 50;

const char* DEFAULT_DEVICE_NAME = "RGB-Sequencer";
const char* DEFAULT_AP_PASSWORD = "12345678";
const char* MDNS_NAME = "rgb-sequencer";
const uint16_t DNS_PORT = 53;
const uint8_t AP_CHANNEL = 6;
const uint8_t AP_MAX_CLIENTS = 8;
const unsigned long WIFI_CHECK_INTERVAL_MS = 15000;
IPAddress AP_IP(192, 168, 178, 99);
IPAddress AP_GATEWAY(192, 168, 178, 99);
IPAddress AP_SUBNET(255, 255, 255, 0);

ESP8266WebServer server(80);
DNSServer dnsServer;
Adafruit_NeoPixel strip(MAX_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

uint16_t ledCount = 50;
String deviceName = DEFAULT_DEVICE_NAME;
String apPassword = DEFAULT_AP_PASSWORD;
String playlistText =
  "#NAME RGB-Sequencer\n"
  "#PASS 12345678\n"
  "#COUNT 50\n"
  "#SEQ Demo_Sequenz|150000|1\n"
  "5000|FFFFFF:1\n"
  "5000|000000:1\n"
  "5000|FF0000:1\n"
  "5000|00FF00:1\n"
  "5000|0000FF:1\n"
  "5000|FX:fire:50:FFFFFF:1\n"
  "5000|FX:storm:50:FFFFFF:1\n"
  "5000|FX:rainbow:50:FFFFFF:1\n"
  "5000|FX:welder:50:FFFFFF:1\n"
  "5000|FX:camera:50:FFFFFF:1\n"
  "5000|FX:police:50:FFFFFF:1\n"
  "5000|FX:sparkle:50:FFFFFF:1\n"
  "5000|FX:comet:50:FFFFFF:1\n"
  "5000|FX:theater:50:FFFFFF:1\n"
  "5000|FX:pulse:50:FFFFFF:1\n"
  "5000|FX:breathe:50:FFFFFF:1\n"
  "5000|FX:lava:50:FFFFFF:1\n"
  "5000|FX:randomOnOff:50:FFFFFF:1\n"
  "5000|FX:randomColor:50:FFFFFF:1\n"
  "5000|FX:randomOnOffColor:50:FFFFFF:1\n"
  "5000|FX:meteor:50:FFFFFF:1\n"
  "5000|FX:twinkle:50:FFFFFF:1\n"
  "5000|FX:candle:50:FFFFFF:1\n"
  "5000|FX:sunrise:50:FFFFFF:1\n"
  "5000|FX:sunset:50:FFFFFF:1\n"
  "5000|FX:scanner:50:FFFFFF:1\n"
  "5000|FX:confetti:50:FFFFFF:1\n"
  "5000|FX:aurora:50:FFFFFF:1\n"
  "5000|FX:toxic:50:FFFFFF:1\n"
  "5000|FX:heartbeat:50:FFFFFF:1\n";

struct Step {
  uint16_t durationMs;
  uint16_t randomMaxMs;
  String groups;
};

struct Sequence {
  String name;
  uint32_t durationMs;
  uint16_t firstStep;
  uint16_t stepCount;
  bool enabled;
};

const uint8_t MAX_SEQUENCES = 30;
const uint16_t MAX_STEPS = 240;

Sequence sequences[MAX_SEQUENCES];
Step steps[MAX_STEPS];
uint8_t sequenceCount = 0;
uint16_t totalStepCount = 0;
uint8_t currentSequence = 0;
uint16_t currentStepOffset = 0;
unsigned long lastStepAt = 0;
unsigned long activeStepDurationMs = 0;
unsigned long sequenceStartedAt = 0;
unsigned long liveUntil = 0;
unsigned long lastEffectAt = 0;
unsigned long lastWifiCheckAt = 0;
unsigned long apRestartAt = 0;
String liveGroups = "";
bool playbackRunning = true;

const float DEFAULT_EFFECT_SPEED = 50.0;

void applyLedCount(uint16_t count) {
  ledCount = constrain(count, 1, MAX_LEDS);
  strip.updateLength(ledCount);
}

void showStrip() {
  strip.show();
  yield();
}

uint32_t parseColor(String value) {
  value.trim();
  value.replace("#", "");
  return strtoul(value.c_str(), nullptr, 16) & 0xFFFFFF;
}

uint32_t scaleColor(uint32_t color, float factor) {
  factor = constrain(factor, 0.0, 1.0);
  uint8_t r = ((color >> 16) & 0xFF) * factor;
  uint8_t g = ((color >> 8) & 0xFF) * factor;
  uint8_t b = (color & 0xFF) * factor;
  return strip.Color(r, g, b);
}

float effectSpeedFactor(float setting) {
  setting = constrain(setting, 0.0, 100.0);
  if (setting <= 50.0) {
    return pow(300.0, (setting - 50.0) / 50.0);
  }
  return pow(10.0, (setting - 50.0) / 50.0);
}

unsigned long effectUpdateInterval(float setting) {
  setting = constrain(setting, 0.0, 100.0);
  if (setting <= 50.0) {
    return (unsigned long)(90.0 * pow(300.0, (50.0 - setting) / 50.0));
  }
  return (unsigned long)(90.0 / pow(4.5, (setting - 50.0) / 50.0));
}

unsigned long effectUpdateIntervalForGroups(const String& groups) {
  unsigned long interval = 60000;
  int cursor = 0;
  while (cursor < groups.length()) {
    int end = groups.indexOf(';', cursor);
    if (end < 0) end = groups.length();
    String group = groups.substring(cursor, end);
    group.trim();
    if (group.startsWith("FX:")) {
      int sep = group.indexOf(':', 3);
      int sep2 = sep > 3 ? group.indexOf(':', sep + 1) : -1;
      float setting = sep2 > sep ? group.substring(sep + 1, sep2).toFloat() : DEFAULT_EFFECT_SPEED;
      interval = min(interval, effectUpdateInterval(setting));
    }
    cursor = end + 1;
  }
  return constrain(interval, 20UL, 300000UL);
}

void clearStrip() {
  strip.clear();
  showStrip();
}

void applyLedSelection(const String& selection, uint32_t color) {
  String text = selection;
  text.replace(" ", "");
  text.toUpperCase();

  if (text == "ALLE" || text == "ALL") {
    for (uint16_t i = 0; i < ledCount; i++) strip.setPixelColor(i, color);
    return;
  }

  int cursor = 0;
  while (cursor < text.length()) {
    int comma = text.indexOf(',', cursor);
    if (comma < 0) comma = text.length();

    String part = text.substring(cursor, comma);
    int dash = part.indexOf('-');

    int first = 0;
    int last = 0;
    if (dash >= 0) {
      first = part.substring(0, dash).toInt();
      last = part.substring(dash + 1).toInt();
    } else {
      first = part.toInt();
      last = first;
    }

    if (first > last) {
      int tmp = first;
      first = last;
      last = tmp;
    }

    for (int led = first; led <= last; led++) {
      if (led >= 1 && led <= ledCount) strip.setPixelColor(led - 1, color);
    }

    cursor = comma + 1;
  }
}

uint32_t effectColorBase(const String& type, uint16_t led, float speed, uint32_t chosenColor) {
  unsigned long t = (unsigned long)(millis() * speed);
  if (type == "fire") {
    return strip.Color(random(120, 256), random(18, 112), random(0, 9));
  }
  if (type == "storm") {
    if (random(100) > 92) return random(100) > 35 ? strip.Color(255, 255, 255) : strip.Color(159, 216, 255);
    uint8_t pulse = random(18, 61);
    return strip.Color(pulse / 3, pulse / 2, pulse);
  }
  if (type == "rainbow") {
    uint16_t hue = (uint16_t)((t / 20 + led * 850) % 65535);
    return strip.gamma32(strip.ColorHSV(hue, 240, 255));
  }
  if (type == "welder") {
    bool burst = ((t / 70) % 9) < 4 || random(100) > 78;
    if (!burst) return strip.Color(2, 6, 17);
    return random(100) > 28 ? strip.Color(random(120, 201), random(180, 256), random(180, 256)) : strip.Color(255, 255, 255);
  }
  if (type == "camera") {
    uint8_t phase = (t / 70) % 42;
    if (phase < 2) return strip.Color(255, 255, 255);
    if (phase < 4) return strip.Color(185, 215, 255);
    if (phase < 7) return strip.Color(27, 38, 58);
    return 0;
  }
  if (type == "police") {
    return ((t / 180) % 4) < 2 ? strip.Color(255, 0, 40) : strip.Color(0, 109, 255);
  }
  if (type == "sparkle") {
    return random(100) > 82 ? chosenColor : strip.Color(6, 6, 6);
  }
  if (type == "comet") {
    int head = (t / 80) % max((uint16_t)1, ledCount);
    int d = abs((int)led - head);
    uint8_t glow = d > 7 ? 0 : 255 - d * 36;
    return scaleColor(chosenColor, glow / 255.0);
  }
  if (type == "theater") {
    return ((led + t / 180) % 3) == 0 ? chosenColor : 0;
  }
  if (type == "pulse") {
    uint8_t v = (sin(t / 480.0) + 1.0) * 100 + 35;
    return scaleColor(chosenColor, v / 255.0);
  }
  if (type == "breathe") {
    float wave = (sin(t / 780.0) + 1.0) / 2.0;
    uint8_t v = (uint8_t)(255.0 * wave * wave);
    return scaleColor(chosenColor, v / 255.0);
  }
  if (type == "lava") {
    uint8_t v = (sin(t / 360.0 + led * 0.55) + 1.0) * 127;
    return strip.Color(120 + v / 2, 18 + v / 4, 0);
  }
  if (type == "randomOnOff") {
    return random(100) > 58 ? chosenColor : 0;
  }
  if (type == "randomColor") {
    return strip.gamma32(strip.ColorHSV(random(65535), 230, 255));
  }
  if (type == "randomOnOffColor") {
    return random(100) > 58 ? strip.gamma32(strip.ColorHSV(random(65535), 230, 255)) : 0;
  }
  if (type == "meteor") {
    int head = (t / 55) % max((uint16_t)1, ledCount);
    int d = abs((int)led - head);
    uint8_t glow = d > 9 ? 0 : 255 - d * 28;
    return scaleColor(chosenColor, glow / 255.0);
  }
  if (type == "twinkle") {
    uint8_t flash = random(100) > 96 ? 255 : (uint8_t)((sin(t / 210.0 + led * 2.17) + 1.0) * 35);
    return scaleColor(chosenColor, flash / 255.0);
  }
  if (type == "candle") {
    uint8_t v = random(140, 256);
    return strip.Color(v, random(105, 161) * v / 255, 18 * v / 255);
  }
  if (type == "sunrise") {
    uint8_t v = (sin(t / 1400.0) + 1.0) * 127;
    return strip.Color(110 + v / 2, 35 + v / 2, 55 + v / 6);
  }
  if (type == "sunset") {
    uint8_t v = (sin(t / 1300.0 + 1.6) + 1.0) * 127;
    return strip.Color(v, v * 95 / 255, 12 + (255 - v) / 4);
  }
  if (type == "scanner") {
    int span = max(1, (int)ledCount - 1);
    int pos = abs((int)((t / 55) % (span * 2)) - span) + 1;
    int d = abs((int)led - pos);
    uint8_t glow = d > 5 ? 0 : 255 - d * 51;
    return scaleColor(chosenColor, glow / 255.0);
  }
  if (type == "confetti") {
    return random(100) > 72 ? strip.gamma32(strip.ColorHSV(random(65535), 230, 255)) : 0;
  }
  if (type == "aurora") {
    uint16_t hue = 24000 + (uint16_t)((sin(t / 900.0 + led * 0.31) + 1.0) * 6800);
    uint8_t v = 90 + (uint8_t)((sin(t / 600.0 + led * 0.13) + 1.0) * 80);
    return strip.gamma32(strip.ColorHSV(hue, 215, v));
  }
  if (type == "toxic") {
    uint8_t v = random(100) > 90 ? 255 : 115 + (uint8_t)((sin(t / 260.0 + led) + 1.0) * 55);
    return strip.Color(v / 3, v, 0);
  }
  if (type == "heartbeat") {
    uint8_t phase = (t / 70) % 24;
    uint8_t beat = (phase < 2 || (phase > 5 && phase < 8)) ? 255 : (phase > 8 ? max(0, 255 - (phase - 8) * 18) : 0);
    return scaleColor(chosenColor, beat / 255.0);
  }
  return 0;
}

uint32_t effectColor(const String& type, uint16_t led, float speed, uint32_t chosenColor, float brightness) {
  return scaleColor(effectColorBase(type, led, speed, chosenColor), constrain(brightness, 0.0, 1.0));
}

void applyEffectSelection(const String& selection, const String& type, float speed, uint32_t chosenColor, float brightness) {
  String text = selection;
  text.replace(" ", "");
  text.toUpperCase();

  int cursor = 0;
  while (cursor < text.length()) {
    int comma = text.indexOf(',', cursor);
    if (comma < 0) comma = text.length();

    String part = text.substring(cursor, comma);
    int dash = part.indexOf('-');
    int first = dash >= 0 ? part.substring(0, dash).toInt() : part.toInt();
    int last = dash >= 0 ? part.substring(dash + 1).toInt() : first;

    if (first > last) {
      int tmp = first;
      first = last;
      last = tmp;
    }

    for (int led = first; led <= last; led++) {
      if (led >= 1 && led <= ledCount) strip.setPixelColor(led - 1, effectColor(type, led, speed, chosenColor, brightness));
    }

    cursor = comma + 1;
  }
}

void applyStepGroups(const String& groups) {
  strip.clear();

  int cursor = 0;
  while (cursor < groups.length()) {
    int end = groups.indexOf(';', cursor);
    if (end < 0) end = groups.length();

    String group = groups.substring(cursor, end);
    group.trim();
    if (group.startsWith("FX:")) {
      int sep = group.indexOf(':', 3);
      if (sep > 3) {
        String type = group.substring(3, sep);
        int sep2 = group.indexOf(':', sep + 1);
        int sep3 = sep2 > sep ? group.indexOf(':', sep2 + 1) : -1;
        float speedSetting = DEFAULT_EFFECT_SPEED;
        uint32_t color = 0xFFFFFF;
        float brightness = 1.0;
        String leds;
        if (sep3 > sep2) {
          int sep4 = group.indexOf(':', sep3 + 1);
          speedSetting = constrain(group.substring(sep + 1, sep2).toFloat(), 0.0, 100.0);
          color = parseColor(group.substring(sep2 + 1, sep3));
          if (sep4 > sep3) {
            brightness = constrain(group.substring(sep3 + 1, sep4).toFloat(), 0.0, 1.0);
            leds = group.substring(sep4 + 1);
          } else {
            leds = group.substring(sep3 + 1);
          }
        } else if (sep2 > sep) {
          speedSetting = constrain(group.substring(sep + 1, sep2).toFloat(), 0.0, 100.0);
          leds = group.substring(sep2 + 1);
        } else {
          leds = group.substring(sep + 1);
        }
        applyEffectSelection(leds, type, effectSpeedFactor(speedSetting), color, brightness);
      }
    } else {
      int sep = group.indexOf(':');
      if (sep > 0) {
      uint32_t color = parseColor(group.substring(0, sep));
      String leds = group.substring(sep + 1);
      applyLedSelection(leds, color);
      }
    }

    cursor = end + 1;
  }

  showStrip();
}

void addSequenceHeader(String line) {
  if (sequenceCount >= MAX_SEQUENCES) return;
  line.replace("#SEQ", "");
  line.trim();

  int sep = line.indexOf('|');
  int sep2 = sep >= 0 ? line.indexOf('|', sep + 1) : -1;
  String name = sep >= 0 ? line.substring(0, sep) : line;
  name.trim();
  if (name.length() == 0) name = "Sequenz " + String(sequenceCount + 1);

  uint32_t duration = sep >= 0 ? line.substring(sep + 1, sep2 >= 0 ? sep2 : line.length()).toInt() : 10000;
  duration = constrain(duration, 100, 3600000);
  bool enabled = sep2 < 0 || line.substring(sep2 + 1).toInt() != 0;
  sequences[sequenceCount++] = {name, duration, totalStepCount, 0, enabled};
}

void ensureSequenceExists() {
  if (sequenceCount > 0 || sequenceCount >= MAX_SEQUENCES) return;
  sequences[0] = {"Sequenz 1", 10000, totalStepCount, 0, true};
  sequenceCount = 1;
}

void addStep(String line) {
  ensureSequenceExists();
  if (sequenceCount == 0 || totalStepCount >= MAX_STEPS) return;

  int sep1 = line.indexOf('|');
  if (sep1 <= 0) return;

  int sep2 = line.indexOf('|', sep1 + 1);
  uint16_t duration = constrain(line.substring(0, sep1).toInt(), 20, 60000);
  uint16_t randomMaxMs = 0;
  String option = sep2 > sep1 ? line.substring(sep2 + 1) : "";
  option.trim();
  String groups = (sep2 > sep1 && option.startsWith("RANDOM:")) ? line.substring(sep1 + 1, sep2) : line.substring(sep1 + 1);
  groups.trim();
  if (option.startsWith("RANDOM:")) {
    option.trim();
    option.replace("RANDOM:", "");
    randomMaxMs = constrain(option.toInt(), 0, 60000);
  }

  int oldSep = groups.indexOf('|');
  if (oldSep > 0) {
    String color = groups.substring(0, oldSep);
    String leds = groups.substring(oldSep + 1);
    groups = color + ":" + leds;
  }

  steps[totalStepCount++] = {duration, randomMaxMs, groups};
  sequences[sequenceCount - 1].stepCount++;
}

bool selectNextEnabledSequence(uint8_t preferred) {
  if (sequenceCount == 0) return false;
  for (uint8_t i = 0; i < sequenceCount; i++) {
    uint8_t index = (preferred + i) % sequenceCount;
    if (sequences[index].enabled && sequences[index].stepCount > 0) {
      currentSequence = index;
      return true;
    }
  }
  return false;
}

unsigned long chooseStepDuration(const Step& step) {
  if (step.randomMaxMs == 0) return step.durationMs;
  return random(0, (long)step.randomMaxMs + 1);
}

void startStep(Step& step) {
  applyStepGroups(step.groups);
  activeStepDurationMs = chooseStepDuration(step);
  lastEffectAt = millis();
  lastStepAt = millis();
}

void startCurrentSequence() {
  if (!selectNextEnabledSequence(currentSequence)) {
    clearStrip();
    return;
  }
  currentStepOffset = 0;
  sequenceStartedAt = millis();

  const Sequence& seq = sequences[currentSequence];
  if (seq.stepCount > 0) {
    Step& step = steps[seq.firstStep];
    startStep(step);
  }
}

void nextSequence() {
  if (sequenceCount == 0) return;
  currentSequence = (currentSequence + 1) % sequenceCount;
  if (!selectNextEnabledSequence(currentSequence)) {
    clearStrip();
    return;
  }
  startCurrentSequence();
}

String normalizeDeviceName(String name) {
  name.trim();
  if (name.length() == 0) name = DEFAULT_DEVICE_NAME;
  if (name.length() > 31) name = name.substring(0, 31);
  return name;
}

String normalizePassword(String password) {
  password.trim();
  if (password.length() < 8) password = DEFAULT_AP_PASSWORD;
  if (password.length() > 63) password = password.substring(0, 63);
  return password;
}

void parsePlaylist(const String& text) {
  sequenceCount = 0;
  totalStepCount = 0;
  deviceName = DEFAULT_DEVICE_NAME;
  apPassword = DEFAULT_AP_PASSWORD;

  int start = 0;
  while (start < text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();

    String line = text.substring(start, end);
    line.trim();

    if (line.startsWith("#NAME")) {
      String nameText = line;
      nameText.replace("#NAME", "");
      deviceName = normalizeDeviceName(nameText);
    } else if (line.startsWith("#PASS")) {
      String passwordText = line;
      passwordText.replace("#PASS", "");
      apPassword = normalizePassword(passwordText);
    } else if (line.startsWith("#COUNT")) {
      String countText = line;
      countText.replace("#COUNT", "");
      countText.trim();
      applyLedCount(countText.toInt());
    } else if (line.startsWith("#SEQ")) {
      addSequenceHeader(line);
    } else if (line.length() > 0 && !line.startsWith("#")) {
      addStep(line);
    }

    start = end + 1;
  }

  while (sequenceCount > 0 && sequences[sequenceCount - 1].stepCount == 0) {
    sequenceCount--;
  }

  if (sequenceCount == 0 || totalStepCount == 0) {
    sequences[0] = {"Aus", 1000, 0, 1, true};
    steps[0] = {1000, 0, ""};
    sequenceCount = 1;
    totalStepCount = 1;
  }

  startCurrentSequence();
}

void savePlaylist(const String& text) {
  File file = LittleFS.open("/playlist.txt", "w");
  if (!file) return;
  file.print(text);
  file.close();
}

String demoSequenceBlock() {
  return String(
    "#SEQ Demo_Sequenz|150000|1\n"
    "5000|FFFFFF:1\n"
    "5000|000000:1\n"
    "5000|FF0000:1\n"
    "5000|00FF00:1\n"
    "5000|0000FF:1\n"
    "5000|FX:fire:50:FFFFFF:1\n"
    "5000|FX:storm:50:FFFFFF:1\n"
    "5000|FX:rainbow:50:FFFFFF:1\n"
    "5000|FX:welder:50:FFFFFF:1\n"
    "5000|FX:camera:50:FFFFFF:1\n"
    "5000|FX:police:50:FFFFFF:1\n"
    "5000|FX:sparkle:50:FFFFFF:1\n"
    "5000|FX:comet:50:FFFFFF:1\n"
    "5000|FX:theater:50:FFFFFF:1\n"
    "5000|FX:pulse:50:FFFFFF:1\n"
    "5000|FX:breathe:50:FFFFFF:1\n"
    "5000|FX:lava:50:FFFFFF:1\n"
    "5000|FX:randomOnOff:50:FFFFFF:1\n"
    "5000|FX:randomColor:50:FFFFFF:1\n"
    "5000|FX:randomOnOffColor:50:FFFFFF:1\n"
    "5000|FX:meteor:50:FFFFFF:1\n"
    "5000|FX:twinkle:50:FFFFFF:1\n"
    "5000|FX:candle:50:FFFFFF:1\n"
    "5000|FX:sunrise:50:FFFFFF:1\n"
    "5000|FX:sunset:50:FFFFFF:1\n"
    "5000|FX:scanner:50:FFFFFF:1\n"
    "5000|FX:confetti:50:FFFFFF:1\n"
    "5000|FX:aurora:50:FFFFFF:1\n"
    "5000|FX:toxic:50:FFFFFF:1\n"
    "5000|FX:heartbeat:50:FFFFFF:1\n"
  );
}

void ensureDemoSequence() {
  if (playlistText.indexOf("#SEQ Demo_Sequenz|") >= 0) return;

  String demo = demoSequenceBlock();
  int firstSequence = playlistText.indexOf("#SEQ ");
  if (firstSequence >= 0) {
    playlistText = playlistText.substring(0, firstSequence) + demo + "\n" + playlistText.substring(firstSequence);
  } else {
    if (!playlistText.endsWith("\n")) playlistText += "\n";
    playlistText += demo;
  }
  savePlaylist(playlistText);
}

void loadPlaylist() {
  if (LittleFS.exists("/playlist.txt")) {
    File file = LittleFS.open("/playlist.txt", "r");
    if (file) {
      playlistText = file.readString();
      file.close();
    }
  }
  ensureDemoSequence();
  parsePlaylist(playlistText);
}

const char INDEX_HTML[] PROGMEM = R"LEDSEQPAGE(
<!doctype html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>RGB Sequencer</title>
  <style>
    :root { font-family: system-ui, sans-serif; color: #172033; background: #eef2f7; }
    * { box-sizing: border-box; }
    body { margin: 0; background: #eef2f7; color: #172033; }
    main { display: grid; grid-template-columns: 380px 1fr; min-height: 100vh; }
    aside, section { padding: 18px; }
    aside { background: #182235; color: #f8fafc; }
    h1 { font-size: 22px; margin: 0 0 18px; }
    h2 { font-size: 18px; margin: 0 0 12px; }
    h3 { font-size: 15px; margin: 18px 0 8px; }
    button, input, select { font: inherit; }
    button { border: 0; border-radius: 9px; min-height: 44px; padding: 10px 12px; font-weight: 700; cursor: pointer; background: #dbe4f0; color: #172033; touch-action: manipulation; }
    button.primary { background: #22c55e; color: #052e16; }
    button.danger { background: #f97316; color: #431407; }
    button.ghost { background: #2a3650; color: #f8fafc; }
    button.icon { width: 44px; height: 44px; padding: 0; }
    input, select { width: 100%; min-height: 44px; border: 1px solid #cbd5e1; border-radius: 9px; padding: 10px; background: #fff; color: #172033; }
    aside input, aside select { border-color: #475569; }
    .seq-list { display: grid; gap: 8px; }
    .seq-item { display: grid; grid-template-columns: 1fr auto; gap: 6px; align-items: center; padding: 10px; border-radius: 8px; background: #26334b; color: #f8fafc; cursor: pointer; }
    .seq-item.active { outline: 3px solid #38bdf8; background: #31415d; }
    .seq-meta { color: #cbd5e1; font-size: 13px; margin-top: 3px; }
    .row { display: flex; gap: 8px; align-items: center; flex-wrap: wrap; }
    .sidebar-row { display: flex; gap: 8px; align-items: center; flex-wrap: nowrap; }
    .sidebar-row button { flex: 1 1 0; padding-left: 8px; padding-right: 8px; }
    .sidebar-panel { margin-bottom: 18px; }
    .sidebar-panel label { display: block; margin-bottom: 8px; }
    .panel { background: #fff; border: 1px solid #d7dee8; border-radius: 10px; padding: 16px; margin-bottom: 16px; }
    .led-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(52px, 1fr)); gap: 8px; max-height: 45vh; overflow: auto; padding: 2px; -webkit-overflow-scrolling: touch; }
    .led-btn { min-height: 48px; border: 2px solid #cbd5e1; background: #f8fafc; }
    .led-btn.on { color: #fff; border-color: #111827; }
    .steps { display: grid; gap: 8px; }
    .step { display: grid; grid-template-columns: 1fr auto repeat(5, auto); gap: 8px; align-items: center; padding: 10px; background: #f8fafc; border: 1px solid #d7dee8; border-radius: 8px; }
    .step.active { outline: 3px solid #38bdf8; }
    .effects { display: grid; gap: 8px; }
    .effect { display: grid; grid-template-columns: 1fr auto; gap: 8px; align-items: center; padding: 9px; background: #f8fafc; border: 1px solid #d7dee8; border-radius: 8px; }
    .status { min-height: 24px; color: #166534; font-weight: 800; }
    .muted { color: #64748b; font-size: 14px; }
    .swatch { display: inline-block; width: 18px; height: 18px; border-radius: 4px; border: 1px solid #475569; vertical-align: middle; margin-right: 6px; }
    @media (max-width: 1050px) { main { grid-template-columns: 1fr; } aside { order: 1; } section { order: 2; } }
    @media (max-width: 640px) {
      body { font-size: 16px; }
      aside, section { padding: 12px; }
      h1 { font-size: 21px; margin-bottom: 12px; }
      h2 { font-size: 17px; }
      .panel { padding: 12px; margin-bottom: 12px; }
      .row, .sidebar-row { display: grid; grid-template-columns: 1fr; gap: 10px; }
      .row label, .sidebar-row button { width: 100% !important; }
      .led-grid { grid-template-columns: repeat(auto-fill, minmax(56px, 1fr)); max-height: 38vh; }
      .led-btn { min-height: 52px; }
      .step { grid-template-columns: 1fr repeat(5, 44px); gap: 6px; }
      .step > div:nth-child(2) { grid-column: 1 / -1; }
      .effect { grid-template-columns: 1fr; }
      .effect input, .effect button { width: 100%; }
    }
  </style>
</head>
<body>
<main>
  <aside>
    <h1>RGB Sequencer</h1>

    <div class="sidebar-panel">
      <h3>1. Geraet und LEDs</h3>
      <label>Name / WLAN
        <input id="deviceNameInput" maxlength="31" value="RGB-Sequencer" oninput="updateDeviceName()" />
      </label>
      <label>Kennwort
        <input id="apPasswordInput" minlength="8" maxlength="63" value="12345678" oninput="updatePassword()" />
      </label>
      <label>Anzahl LEDs
        <input id="ledCountInput" type="number" min="1" max="50" value="50" onchange="changeLedCount()" />
      </label>
      <div class="sidebar-row">
        <button class="ghost" onclick="paintAll()">Alle setzen</button>
        <button class="ghost" onclick="clearPaint()">Alle aus</button>
      </div>
    </div>

    <div class="seq-list" id="sequenceList"></div>
    <h3>2. Sequenzen</h3>
    <div class="sidebar-row">
      <button class="ghost" onclick="addSequence()">Neu</button>
      <button class="ghost" onclick="moveSequence(-1)">Hoch</button>
      <button class="ghost" onclick="moveSequence(1)">Runter</button>
      <button class="danger" onclick="deleteSequence()">Loeschen</button>
    </div>
  </aside>

  <section>
    <div class="panel">
      <h2>3. Steuerung</h2>
      <div class="row">
        <button class="primary" onclick="playPlaylist()">Abspielen</button>
        <button onclick="stopPlaylist()">Stop</button>
        <button class="primary" onclick="saveAll()">Alle Sequenzen speichern</button>
      </div>
      <p id="status" class="status"></p>
      <p class="muted">Ohne Abspielen wird der aktuell bearbeitete Schritt live auf den LEDs angezeigt.</p>
    </div>

    <div class="panel">
      <h2>4. Sequenz bearbeiten</h2>
      <div class="row">
        <label style="flex: 1 1 260px">Name
          <input id="seqName" oninput="updateSequenceMeta()" />
        </label>
        <label style="width: 170px">Dauer Sequenz
          <input id="seqDuration" type="number" min="0.1" step="0.1" oninput="updateSequenceMeta()" />
        </label>
        <label style="width: 130px">Aktiv
          <input id="seqEnabled" type="checkbox" onchange="updateSequenceMeta()" />
        </label>
      </div>
    </div>

    <div class="panel">
      <h2>5. Aktuellen Schritt bearbeiten</h2>
      <div class="row">
        <label style="width: 120px">Farbe
          <input id="stepColor" type="color" value="#ff0000" oninput="liveCurrentStep()" />
        </label>
        <label style="width: 170px">Schrittdauer Sekunden
          <input id="stepSeconds" type="number" min="0.02" step="0.1" value="1" oninput="liveCurrentStep()" />
        </label>
        <label style="width: 210px">Zufallsdauer bis Sekunden
          <input id="stepRandomSeconds" type="number" min="0" max="60" step="0.1" value="0" oninput="liveCurrentStep()" />
        </label>
        <button class="primary" onclick="saveStep()">Schritt speichern</button>
        <button onclick="newStep()">Neuer Schritt</button>
        <button onclick="pasteStep()">Kopierten Schritt einfuegen</button>
      </div>
      <h3>LEDs fuer diesen Schritt</h3>
      <div class="led-grid" id="ledGrid"></div>
    </div>

    <div class="panel">
      <h2>6. Effekte im aktuellen Schritt</h2>
      <div class="row">
        <label style="width: 180px">LEDs
          <input id="effectLeds" value="1-20" />
        </label>
        <label style="width: 180px">Effekt
          <select id="effectType">
            <option value="fire">Feuerflackern</option>
            <option value="storm">Gewitter</option>
            <option value="rainbow">Regenbogen</option>
            <option value="welder">Schweisserlicht</option>
            <option value="camera">Fotografen-Blitz</option>
            <option value="police">Polizei-Blitzer</option>
            <option value="sparkle">Funkeln</option>
            <option value="comet">Komet</option>
            <option value="theater">Theater-Lauflicht</option>
            <option value="pulse">Pulsieren</option>
            <option value="breathe">Atmen</option>
            <option value="lava">Lava</option>
            <option value="randomOnOff">Zufall An/Aus</option>
            <option value="randomColor">Zufall Farbe</option>
            <option value="randomOnOffColor">Zufall An/Aus + Farbe</option>
            <option value="meteor">Meteor</option>
            <option value="twinkle">Sternfunkeln</option>
            <option value="candle">Kerze</option>
            <option value="sunrise">Sonnenaufgang</option>
            <option value="sunset">Sonnenuntergang</option>
            <option value="scanner">Scanner</option>
            <option value="confetti">Konfetti</option>
            <option value="aurora">Aurora</option>
            <option value="toxic">Giftgruen</option>
            <option value="heartbeat">Herzschlag</option>
          </select>
        </label>
        <label style="width: 150px">Geschwindigkeit 0-100
          <input id="effectSpeed" type="number" min="0" max="100" step="1" value="50" oninput="liveCurrentStep()" />
        </label>
        <label style="width: 120px">Effektfarbe
          <input id="effectColor" type="color" value="#ffffff" oninput="liveCurrentStep()" />
        </label>
        <label style="width: 160px">Helligkeit %
          <input id="effectBrightness" type="number" min="0" max="100" step="1" value="100" oninput="liveCurrentStep()" />
        </label>
        <button onclick="addEffect()">Effekt hinzufuegen</button>
      </div>
      <div class="effects" id="effects"></div>
    </div>

    <div class="panel">
      <h2>7. Schritte dieser Sequenz</h2>
      <div class="steps" id="steps"></div>
    </div>
  </section>
</main>

<script>
const MAX_LEDS = 50;
let deviceName = 'RGB-Sequencer';
let ledCount = 50;
let playlist = [];
let selectedSequence = 0;
let selectedStep = -1;
let paintedColors = new Map();
let currentEffects = [];
let copiedStep = null;
let liveTimer = null;
const API_BASE = (location.hostname === '127.0.0.1' || location.protocol === 'file:')
  ? 'http://192.168.178.99'
  : '';

function apiFetch(path, options = {}) {
  return fetch(API_BASE + path, { cache: 'no-store', ...options });
}

const sequenceList = document.getElementById('sequenceList');
const ledGrid = document.getElementById('ledGrid');
const stepsEl = document.getElementById('steps');
const effectsEl = document.getElementById('effects');
const seqName = document.getElementById('seqName');
const seqDuration = document.getElementById('seqDuration');
const seqEnabled = document.getElementById('seqEnabled');
const stepSeconds = document.getElementById('stepSeconds');
const stepRandomSeconds = document.getElementById('stepRandomSeconds');
const stepColor = document.getElementById('stepColor');
const effectLeds = document.getElementById('effectLeds');
const effectType = document.getElementById('effectType');
const effectSpeed = document.getElementById('effectSpeed');
const effectColor = document.getElementById('effectColor');
const effectBrightness = document.getElementById('effectBrightness');
const statusEl = document.getElementById('status');
const deviceNameInput = document.getElementById('deviceNameInput');
const apPasswordInput = document.getElementById('apPasswordInput');
const ledCountInput = document.getElementById('ledCountInput');
let apPassword = '12345678';

const EFFECT_TYPES = ['fire', 'storm', 'rainbow', 'welder', 'camera', 'police', 'sparkle', 'comet', 'theater', 'pulse', 'breathe', 'lava', 'randomOnOff', 'randomColor', 'randomOnOffColor', 'meteor', 'twinkle', 'candle', 'sunrise', 'sunset', 'scanner', 'confetti', 'aurora', 'toxic', 'heartbeat'];

function defaultPlaylist() {
  return [
    { name: 'Demo_Sequenz', durationMs: 150000, enabled: true, steps: [
      { durationMs: 5000, randomMaxMs: 0, groups: [{ leds: '1', color: '#ffffff' }], effects: [] },
      { durationMs: 5000, randomMaxMs: 0, groups: [{ leds: '1', color: '#000000' }], effects: [] },
      { durationMs: 5000, randomMaxMs: 0, groups: [{ leds: '1', color: '#ff0000' }], effects: [] },
      { durationMs: 5000, randomMaxMs: 0, groups: [{ leds: '1', color: '#00ff00' }], effects: [] },
      { durationMs: 5000, randomMaxMs: 0, groups: [{ leds: '1', color: '#0000ff' }], effects: [] },
      ...EFFECT_TYPES.map(type => ({ durationMs: 5000, randomMaxMs: 0, groups: [], effects: [{ leds: '1', type, speed: 50, brightness: 1, color: '#ffffff' }] }))
    ] }
  ];
}

function normalizeStep(step) {
  const normalized = Array.isArray(step.groups)
    ? { ...step }
    : { durationMs: step.durationMs || 1000, groups: [{ leds: step.leds || '', color: step.color || '#ff0000' }] };
  normalized.randomMaxMs = Number(normalized.randomMaxMs || 0);
  if (!Array.isArray(normalized.effects)) normalized.effects = [];
  normalized.effects = normalized.effects.map(effect => ({ ...effect, speed: clamp(Number(effect.speed ?? 50), 0, 100), brightness: clamp(Number(effect.brightness ?? 1), 0, 1) }));
  return normalized;
}

function stepText(step) {
  step = normalizeStep(step);
  const colors = step.groups.map(group => `${group.color} LEDs ${group.leds || 'keine'}`);
  const effects = step.effects.map(effect => `${effectName(effect.type)} LEDs ${effect.leds} Speed ${effect.speed ?? 50}, ${Math.round((effect.brightness ?? 1) * 100)}%${usesEffectColor(effect.type) ? ` ${effect.color || '#ffffff'}` : ''}`);
  const randomText = step.randomMaxMs > 0 ? `Zufallsdauer bis ${msToSeconds(step.randomMaxMs)} s` : '';
  return [...colors, ...effects, randomText].filter(Boolean).join(' / ');
}

function effectName(type) {
  return ({
    fire: 'Feuerflackern',
    storm: 'Gewitter',
    rainbow: 'Regenbogen',
    welder: 'Schweisserlicht',
    camera: 'Fotografen-Blitz',
    police: 'Polizei-Blitzer',
    sparkle: 'Funkeln',
    comet: 'Komet',
    theater: 'Theater-Lauflicht',
    pulse: 'Pulsieren',
    breathe: 'Atmen',
    lava: 'Lava',
    randomOnOff: 'Zufall An/Aus',
    randomColor: 'Zufall Farbe',
    randomOnOffColor: 'Zufall An/Aus + Farbe',
    meteor: 'Meteor',
    twinkle: 'Sternfunkeln',
    candle: 'Kerze',
    sunrise: 'Sonnenaufgang',
    sunset: 'Sonnenuntergang',
    scanner: 'Scanner',
    confetti: 'Konfetti',
    aurora: 'Aurora',
    toxic: 'Giftgruen',
    heartbeat: 'Herzschlag'
  })[type] || type;
}

function usesEffectColor(type) {
  return ['sparkle', 'comet', 'theater', 'pulse', 'breathe', 'randomOnOff', 'meteor', 'twinkle', 'scanner', 'heartbeat'].includes(type);
}

function mapFromGroups(groups) {
  const colors = new Map();
  for (const group of groups) {
    for (const led of expandSelection(group.leds)) colors.set(led, group.color.toLowerCase());
  }
  return colors;
}

function groupsFromMap(colors) {
  const byColor = new Map();
  for (const [led, color] of colors) {
    if (!byColor.has(color)) byColor.set(color, []);
    byColor.get(color).push(led);
  }
  return [...byColor.entries()].map(([color, leds]) => ({ color, leds: compactNumbers(leds) }));
}

function parseStored(text) {
  const list = [];
  let current = null;
  deviceName = 'RGB-Sequencer';
  apPassword = '12345678';
  for (const raw of text.split('\n')) {
    const line = raw.trim();
    if (!line) continue;
    if (line.startsWith('#NAME')) {
      deviceName = normalizeDeviceName(line.replace('#NAME', '').trim());
      continue;
    }
    if (line.startsWith('#PASS')) {
      apPassword = normalizePassword(line.replace('#PASS', '').trim());
      continue;
    }
    if (line.startsWith('#COUNT')) {
      ledCount = clamp(parseInt(line.replace('#COUNT', '').trim(), 10) || 50, 1, MAX_LEDS);
      continue;
    }
    if (line.startsWith('#SEQ')) {
      const header = line.replace('#SEQ', '').trim();
      const fields = header.split('|');
      current = {
        name: fields[0]?.trim() || 'Sequenz',
        durationMs: parseInt(fields[1], 10) || 10000,
        enabled: fields.length < 3 || fields[2] !== '0',
        steps: []
      };
      list.push(current);
      continue;
    }
    const parts = line.split('|');
    if (parts.length >= 2 && current) {
      if (parts.length === 3 && !parts[2].startsWith('RANDOM:') && !parts[1].includes(':')) {
        current.steps.push({ durationMs: parseInt(parts[0], 10) || 1000, randomMaxMs: 0, color: `#${parts[1]}`, leds: parts[2] });
        continue;
      }
      const option = parts[2] || '';
      current.steps.push({
        durationMs: parseInt(parts[0], 10) || 1000,
        randomMaxMs: option.startsWith('RANDOM:') ? clamp(parseInt(option.replace('RANDOM:', ''), 10) || 0, 0, 60000) : 0,
        groups: parts[1].split(';').filter(Boolean).filter(group => !group.startsWith('FX:')).map(group => {
          const sep = group.indexOf(':');
          return { color: `#${group.slice(0, sep)}`, leds: group.slice(sep + 1) };
        }),
        effects: parts[1].split(';').filter(group => group.startsWith('FX:')).map(group => {
          const parts = group.split(':');
          if (parts.length >= 6) return { type: parts[1] || 'fire', speed: clamp(parseFloat(parts[2]) || 50, 0, 100), color: `#${parts[3] || 'ffffff'}`, brightness: clamp(parseFloat(parts[4]) || 1, 0, 1), leds: parts.slice(5).join(':') || '' };
          if (parts.length >= 5) return { type: parts[1] || 'fire', speed: clamp(parseFloat(parts[2]) || 50, 0, 100), color: `#${parts[3] || 'ffffff'}`, brightness: 1, leds: parts.slice(4).join(':') || '' };
          if (parts.length >= 4) return { type: parts[1] || 'fire', speed: clamp(parseFloat(parts[2]) || 50, 0, 100), color: '#ffffff', brightness: 1, leds: parts.slice(3).join(':') || '' };
          return { type: parts[1] || 'fire', speed: 50, color: '#ffffff', brightness: 1, leds: parts[2] || '' };
        })
      });
    }
  }
  list.forEach(seq => seq.steps = (seq.steps || []).map(normalizeStep));
  return list.length ? list : defaultPlaylist();
}

function serialize() {
  const body = playlist.map(seq => {
    const header = `#SEQ ${seq.name || 'Sequenz'}|${Math.max(100, Math.round(seq.durationMs || 10000))}|${seq.enabled === false ? 0 : 1}`;
    const lines = seq.steps.map(step => {
      step = normalizeStep(step);
      const colorGroups = step.groups.map(group => `${group.color.replace('#', '').toUpperCase()}:${group.leds || ''}`);
      const effectGroups = (step.effects || []).map(effect => `FX:${effect.type}:${clamp(Number(effect.speed ?? 50), 0, 100)}:${String(effect.color || '#ffffff').replace('#', '').toUpperCase()}:${clamp(Number(effect.brightness ?? 1), 0, 1)}:${effect.leds || ''}`);
      const groups = [...colorGroups, ...effectGroups].join(';');
      const randomPart = step.randomMaxMs > 0 ? `|RANDOM:${Math.max(0, Math.round(step.randomMaxMs))}` : '';
      return `${Math.max(20, Math.round(step.durationMs))}|${groups}${randomPart}`;
    });
    return [header, ...lines].join('\n');
  }).join('\n\n');
  return `#NAME ${normalizeDeviceName(deviceName)}\n#PASS ${normalizePassword(apPassword)}\n#COUNT ${ledCount}\n${body}`;
}

function render() {
  deviceNameInput.value = normalizeDeviceName(deviceName);
  apPasswordInput.value = normalizePassword(apPassword);
  ledCountInput.value = ledCount;
  if (!playlist.length) playlist = defaultPlaylist();
  selectedSequence = clamp(selectedSequence, 0, playlist.length - 1);
  const seq = playlist[selectedSequence];
  if (selectedStep >= seq.steps.length) selectedStep = seq.steps.length - 1;

  sequenceList.innerHTML = '';
  playlist.forEach((item, index) => {
    const el = document.createElement('div');
    el.className = `seq-item ${index === selectedSequence ? 'active' : ''}`;
    el.onclick = () => selectSequence(index);
    const state = item.enabled === false ? 'inaktiv' : 'aktiv';
    el.innerHTML = `<div><strong>${escapeHtml(item.name || 'Sequenz')}</strong><div class="seq-meta">${state}, ${msToSeconds(item.durationMs)} s, ${item.steps.length} Schritte</div></div><div>${index + 1}</div>`;
    sequenceList.appendChild(el);
  });

  seqName.value = seq.name || '';
  seqDuration.value = msToSeconds(seq.durationMs);
  seqEnabled.checked = seq.enabled !== false;
  renderLedGrid();
  renderStrip();
  renderEffects();
  renderSteps();
}

function normalizeDeviceName(name) {
  name = String(name || 'RGB-Sequencer').trim();
  if (!name) name = 'RGB-Sequencer';
  return name.slice(0, 31);
}

function normalizePassword(password) {
  password = String(password || '12345678').trim();
  if (password.length < 8) password = '12345678';
  return password.slice(0, 63);
}

function updateDeviceName() {
  deviceName = normalizeDeviceName(deviceNameInput.value);
}

function updatePassword() {
  apPassword = normalizePassword(apPasswordInput.value);
}

function colorMapFromGroups(groups) {
  const colors = new Map();
  for (const group of groups || []) {
    for (const led of expandSelection(group.leds)) colors.set(led, group.color);
  }
  return colors;
}

function currentStepDraft() {
  return { durationMs: secondsToMs(stepSeconds.value, 1), randomMaxMs: secondsToOptionalMs(stepRandomSeconds.value), groups: groupsFromMap(paintedColors), effects: currentEffects };
}

function renderStrip(step = currentStepDraft()) {
  // Keine Browser-Simulation mehr. Die echten LEDs werden weiterhin per /live aktualisiert.
}

function renderLedGrid() {
  ledGrid.innerHTML = '';
  for (let i = 1; i <= ledCount; i++) {
    const button = document.createElement('button');
    const color = paintedColors.get(i);
    button.className = `led-btn ${color ? 'on' : ''}`;
    button.textContent = i;
    if (color) button.style.background = color;
    button.onclick = () => toggleLed(i);
    ledGrid.appendChild(button);
  }
}

function renderSteps() {
  const seq = playlist[selectedSequence];
  stepsEl.innerHTML = '';
  seq.steps.forEach((step, index) => {
    step = normalizeStep(step);
    const row = document.createElement('div');
    row.className = `step ${index === selectedStep ? 'active' : ''}`;
    row.onclick = () => loadStep(index);
    row.innerHTML = `
      <div><strong>Schritt ${index + 1}</strong><div class="muted">${escapeHtml(stepText(step))}</div></div>
      <div>${msToSeconds(step.durationMs)} s</div>
      <button class="icon" onclick="event.stopPropagation(); copyStep(${index})">⧉</button>
      <button class="icon" onclick="event.stopPropagation(); pasteStep(${index})">＋</button>
      <button class="icon" onclick="event.stopPropagation(); moveStep(${index}, -1)">↑</button>
      <button class="icon" onclick="event.stopPropagation(); moveStep(${index}, 1)">↓</button>
      <button class="danger" onclick="event.stopPropagation(); deleteStep(${index})">X</button>`;
    stepsEl.appendChild(row);
  });
}

function renderEffects() {
  effectsEl.innerHTML = '';
  if (!currentEffects.length) {
    effectsEl.innerHTML = '<div class="muted">Noch kein Effekt im aktuellen Schritt.</div>';
    return;
  }
  currentEffects.forEach((effect, index) => {
    const row = document.createElement('div');
    row.className = 'effect';
    const colorText = usesEffectColor(effect.type) ? `, Farbe ${effect.color || '#ffffff'}` : '';
    const colorInput = usesEffectColor(effect.type) ? `<input type="color" value="${effect.color || '#ffffff'}" oninput="updateEffectColor(${index}, this.value)">` : '';
    row.innerHTML = `<div><strong>${effectName(effect.type)}</strong><div class="muted">LEDs ${escapeHtml(effect.leds || 'keine')}${colorText}</div></div><label class="muted">Speed 0-100 <input type="number" min="0" max="100" step="1" value="${effect.speed ?? 50}" oninput="updateEffectSpeed(${index}, this.value)"></label><label class="muted">Helligkeit % <input type="number" min="0" max="100" step="1" value="${Math.round((effect.brightness ?? 1) * 100)}" oninput="updateEffectBrightness(${index}, this.value)"></label>${colorInput}<button class="danger" onclick="deleteEffect(${index})">X</button>`;
    effectsEl.appendChild(row);
  });
}

function selectSequence(index) {
  selectedSequence = index;
  selectedStep = -1;
  paintedColors.clear();
  currentEffects = [];
  render();
  liveCurrentStep();
}

function changeLedCount() {
  ledCount = clamp(parseInt(ledCountInput.value, 10) || 50, 1, MAX_LEDS);
  paintedColors = new Map([...paintedColors].filter(([led]) => led <= ledCount));
  currentEffects = currentEffects.map(effect => ({ ...effect, leds: compactNumbers([...expandSelection(effect.leds)].filter(led => led <= ledCount)) }));
  render();
  liveCurrentStep();
}

function updateSequenceMeta() {
  const seq = playlist[selectedSequence];
  seq.name = seqName.value;
  seq.durationMs = secondsToMs(seqDuration.value, 10);
  seq.enabled = seqEnabled.checked;
  render();
}

function toggleLed(index) {
  const color = stepColor.value.toLowerCase();
  if (paintedColors.get(index) === color) paintedColors.delete(index);
  else paintedColors.set(index, color);
  renderLedGrid();
  renderStrip();
  liveCurrentStep();
}

function paintAll() {
  const color = stepColor.value.toLowerCase();
  for (let i = 1; i <= ledCount; i++) paintedColors.set(i, color);
  renderLedGrid();
  renderStrip();
  liveCurrentStep();
}

function clearPaint() {
  paintedColors.clear();
  renderLedGrid();
  renderStrip();
  liveCurrentStep();
}

function newStep() {
  const seq = playlist[selectedSequence];
  const insertAt = selectedStep >= 0 ? selectedStep + 1 : seq.steps.length;
  seq.steps.splice(insertAt, 0, { durationMs: 1000, randomMaxMs: 0, groups: [], effects: [] });
  loadStep(insertAt);
  statusEl.textContent = `Neuer Schritt ${insertAt + 1} angelegt`;
}

function saveStep() {
  const seq = playlist[selectedSequence];
  const step = { durationMs: secondsToMs(stepSeconds.value, 1), randomMaxMs: secondsToOptionalMs(stepRandomSeconds.value), groups: groupsFromMap(paintedColors), effects: currentEffects.map(effect => ({ ...effect })) };
  if (selectedStep >= 0) seq.steps[selectedStep] = step;
  else {
    seq.steps.push(step);
    selectedStep = seq.steps.length - 1;
  }
  render();
}

function loadStep(index) {
  const step = normalizeStep(playlist[selectedSequence].steps[index]);
  selectedStep = index;
  paintedColors = mapFromGroups(step.groups);
  currentEffects = step.effects.map(effect => ({ ...effect, speed: clamp(Number(effect.speed ?? 50), 0, 100), brightness: clamp(Number(effect.brightness ?? 1), 0, 1), color: effect.color || '#ffffff' }));
  stepColor.value = step.groups[0]?.color || '#ff0000';
  stepSeconds.value = msToSeconds(step.durationMs);
  stepRandomSeconds.value = msToSeconds(step.randomMaxMs || 0);
  render();
  liveCurrentStep();
}

function addEffect() {
  const leds = compactNumbers([...expandSelection(effectLeds.value)]);
  if (!leds) return;
  currentEffects.push({ leds, type: effectType.value, speed: clamp(parseFloat(String(effectSpeed.value).replace(',', '.')) || 50, 0, 100), brightness: clamp((parseFloat(String(effectBrightness.value).replace(',', '.')) || 100) / 100, 0, 1), color: effectColor.value });
  render();
  renderStrip();
  liveCurrentStep();
}

function updateEffectSpeed(index, value) {
  if (!currentEffects[index]) return;
  currentEffects[index].speed = clamp(parseFloat(String(value).replace(',', '.')) || 0, 0, 100);
  renderStrip();
  liveCurrentStep();
}

function updateEffectBrightness(index, value) {
  if (!currentEffects[index]) return;
  currentEffects[index].brightness = clamp((parseFloat(String(value).replace(',', '.')) || 0) / 100, 0, 1);
  renderStrip();
  liveCurrentStep();
}

function updateEffectColor(index, value) {
  if (!currentEffects[index]) return;
  currentEffects[index].color = value || '#ffffff';
  renderStrip();
  liveCurrentStep();
}

function deleteEffect(index) {
  currentEffects.splice(index, 1);
  render();
  renderStrip();
  liveCurrentStep();
}

function deleteStep(index) {
  playlist[selectedSequence].steps.splice(index, 1);
  selectedStep = -1;
  render();
}

function moveStep(index, direction) {
  const steps = playlist[selectedSequence].steps;
  const target = index + direction;
  if (target < 0 || target >= steps.length) return;
  [steps[index], steps[target]] = [steps[target], steps[index]];
  selectedStep = target;
  render();
}

function cloneStep(step) {
  return JSON.parse(JSON.stringify(normalizeStep(step)));
}

function copyStep(index) {
  copiedStep = cloneStep(playlist[selectedSequence].steps[index]);
  statusEl.textContent = `Schritt ${index + 1} kopiert`;
}

function pasteStep(afterIndex = selectedStep) {
  if (!copiedStep) return;
  const steps = playlist[selectedSequence].steps;
  const insertAt = afterIndex >= 0 ? afterIndex + 1 : steps.length;
  steps.splice(insertAt, 0, cloneStep(copiedStep));
  selectedStep = insertAt;
  loadStep(insertAt);
  statusEl.textContent = `Schritt eingefuegt`;
}

function addSequence() {
  playlist.splice(selectedSequence + 1, 0, { name: 'Neue Sequenz', durationMs: 10000, enabled: true, steps: [] });
  selectedSequence += 1;
  selectedStep = -1;
  paintedColors.clear();
  currentEffects = [];
  render();
}

function deleteSequence() {
  if (playlist.length <= 1) return;
  playlist.splice(selectedSequence, 1);
  selectedSequence = Math.max(0, selectedSequence - 1);
  selectedStep = -1;
  paintedColors.clear();
  currentEffects = [];
  render();
}

function moveSequence(direction) {
  const target = selectedSequence + direction;
  if (target < 0 || target >= playlist.length) return;
  [playlist[selectedSequence], playlist[target]] = [playlist[target], playlist[selectedSequence]];
  selectedSequence = target;
  render();
}

function liveCurrentStep() {
  renderStrip();
  clearTimeout(liveTimer);
  liveTimer = setTimeout(() => {
    const groups = groupsFromMap(paintedColors).map(group => `${group.color.replace('#', '').toUpperCase()}:${group.leds || ''}`).join(';');
    const effects = currentEffects.map(effect => `FX:${effect.type}:${clamp(Number(effect.speed ?? 50), 0, 100)}:${String(effect.color || '#ffffff').replace('#', '').toUpperCase()}:${clamp(Number(effect.brightness ?? 1), 0, 1)}:${effect.leds || ''}`).join(';');
    const allGroups = [groups, effects].filter(Boolean).join(';');
    const params = new URLSearchParams({ groups: allGroups });
    apiFetch(`/live?${params}`).catch(() => { statusEl.textContent = 'Keine Verbindung zum Wemos'; });
  }, 80);
}

function saveAll() {
  apiFetch('/save', { method: 'POST', body: serialize() })
    .then(response => response.text())
    .then(text => { statusEl.textContent = text; liveCurrentStep(); })
    .catch(() => { statusEl.textContent = 'Speichern fehlgeschlagen'; });
}

function playPlaylist() {
  const firstStep = playlist[0]?.steps[0];
  if (firstStep) renderStrip(firstStep);
  apiFetch('/play')
    .then(response => response.text())
    .then(text => { statusEl.textContent = text; })
    .catch(() => { statusEl.textContent = 'Abspielen fehlgeschlagen'; });
}

function stopPlaylist() {
  renderStrip();
  apiFetch('/stop')
    .then(response => response.text())
    .then(text => { statusEl.textContent = text; liveCurrentStep(); })
    .catch(() => { statusEl.textContent = 'Stop fehlgeschlagen'; });
}

function compactSelection() {
  return compactNumbers([...paintedColors.keys()]);
}

function compactNumbers(numbers) {
  const nums = [...numbers].sort((a, b) => a - b);
  const ranges = [];
  for (let i = 0; i < nums.length; i++) {
    const start = nums[i];
    let end = start;
    while (nums[i + 1] === end + 1) end = nums[++i];
    ranges.push(start === end ? `${start}` : `${start}-${end}`);
  }
  return ranges.join(',');
}

function expandSelection(text) {
  const result = new Set();
  for (const part of String(text || '').split(',')) {
    const [a, b] = part.split('-').map(v => parseInt(v, 10));
    if (!Number.isFinite(a)) continue;
    const end = Number.isFinite(b) ? b : a;
    for (let i = Math.min(a, end); i <= Math.max(a, end); i++) {
      if (i >= 1 && i <= ledCount) result.add(i);
    }
  }
  return result;
}

function secondsToMs(value, fallback) {
  const number = parseFloat(String(value).replace(',', '.'));
  return Math.max(20, Math.round((Number.isFinite(number) ? number : fallback) * 1000));
}

function secondsToOptionalMs(value) {
  const number = parseFloat(String(value).replace(',', '.'));
  if (!Number.isFinite(number) || number <= 0) return 0;
  return Math.min(60000, Math.round(number * 1000));
}

function msToSeconds(ms) {
  if (!ms) return '0';
  return (Math.round(ms / 100) / 10).toString();
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, char => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[char]));
}

const loadPlaylist = () => {
  apiFetch('/playlist')
    .then(response => response.text())
    .then(text => {
      playlist = parseStored(text);
      ledCountInput.value = ledCount;
      if (!playlist.length) playlist = defaultPlaylist();
      if (playlist[0].steps.length) loadStep(0);
      render();
    });
};

loadPlaylist();
</script>
</body>
</html>
)LEDSEQPAGE";

void addDefaultHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
}

void handleOptions() {
  addDefaultHeaders();
  server.send(204, "text/plain", "");
}

void handleRoot() {
  addDefaultHeaders();
  server.send_P(200, "text/html", INDEX_HTML);
}

void handlePlaylist() {
  addDefaultHeaders();
  server.send(200, "text/plain", playlistText);
}

void handleLive() {
  String groups = server.arg("groups");
  liveGroups = groups;
  liveUntil = playbackRunning ? millis() + 3000 : 0;
  applyStepGroups(groups);
  lastEffectAt = millis();
  addDefaultHeaders();
  server.send(200, "text/plain", "OK");
}

void handleSave() {
  String oldDeviceName = deviceName;
  String oldPassword = apPassword;
  playlistText = server.arg("plain");
  playlistText.replace("\r", "");
  savePlaylist(playlistText);
  parsePlaylist(playlistText);
  liveUntil = 0;
  playbackRunning = true;
  currentSequence = 0;
  startCurrentSequence();
  addDefaultHeaders();
  server.send(200, "text/plain", "Gespeichert. Der RGB-Streifen laeuft jetzt eigenstaendig weiter.");
  if (oldDeviceName != deviceName || oldPassword != apPassword) apRestartAt = millis() + 1200;
}

void handlePlay() {
  playbackRunning = true;
  liveUntil = 0;
  liveGroups = "";
  currentSequence = 0;
  startCurrentSequence();
  addDefaultHeaders();
  server.send(200, "text/plain", "Abspielen gestartet.");
}

void handleStop() {
  playbackRunning = false;
  liveUntil = 0;
  if (liveGroups.length() > 0) {
    applyStepGroups(liveGroups);
  }
  addDefaultHeaders();
  server.send(200, "text/plain", "Gestoppt. Aktueller Schritt bleibt sichtbar.");
}

void handleCaptivePortal() {
  if (server.method() == HTTP_OPTIONS) {
    handleOptions();
    return;
  }
  handleRoot();
}

void startAccessPoint() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setOutputPower(17.5);
  WiFi.softAPdisconnect(true);
  delay(100);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(deviceName.c_str(), apPassword.c_str(), AP_CHANNEL, false, AP_MAX_CLIENTS);
  dnsServer.stop();
  dnsServer.start(DNS_PORT, "*", AP_IP);
  MDNS.begin(MDNS_NAME);
}

void keepAccessPointAlive() {
  unsigned long now = millis();
  if (apRestartAt != 0 && now >= apRestartAt) {
    apRestartAt = 0;
    startAccessPoint();
    return;
  }
  if (now - lastWifiCheckAt < WIFI_CHECK_INTERVAL_MS) return;
  lastWifiCheckAt = now;

  if ((WiFi.getMode() & WIFI_AP) == 0 || WiFi.softAPIP() != AP_IP) {
    startAccessPoint();
  }
}

void setup() {
  strip.begin();
  applyLedCount(ledCount);
  clearStrip();
  LittleFS.begin();
  loadPlaylist();

  startAccessPoint();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/", HTTP_OPTIONS, handleOptions);
  server.on("/playlist", HTTP_GET, handlePlaylist);
  server.on("/live", HTTP_GET, handleLive);
  server.on("/live", HTTP_OPTIONS, handleOptions);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/save", HTTP_OPTIONS, handleOptions);
  server.on("/play", HTTP_GET, handlePlay);
  server.on("/play", HTTP_OPTIONS, handleOptions);
  server.on("/stop", HTTP_GET, handleStop);
  server.on("/stop", HTTP_OPTIONS, handleOptions);
  server.onNotFound(handleCaptivePortal);
  server.begin();
}

void loop() {
  keepAccessPointAlive();
  dnsServer.processNextRequest();
  MDNS.update();
  server.handleClient();

  unsigned long now = millis();
  if (liveUntil != 0 && now < liveUntil) {
    if (liveGroups.indexOf("FX:") >= 0 && now - lastEffectAt >= effectUpdateIntervalForGroups(liveGroups)) {
      applyStepGroups(liveGroups);
      lastEffectAt = now;
    }
    return;
  }
  if (liveUntil != 0 && now >= liveUntil) {
    liveUntil = 0;
    liveGroups = "";
    if (playbackRunning) {
      startCurrentSequence();
    }
  }

  if (!playbackRunning) {
    if (liveGroups.indexOf("FX:") >= 0 && now - lastEffectAt >= effectUpdateIntervalForGroups(liveGroups)) {
      applyStepGroups(liveGroups);
      lastEffectAt = now;
    }
    return;
  }
  if (sequenceCount == 0) return;

  Sequence& seq = sequences[currentSequence];
  if (seq.stepCount == 0) {
    nextSequence();
    return;
  }

  if (now - sequenceStartedAt >= seq.durationMs) {
    nextSequence();
    return;
  }

  Step& step = steps[seq.firstStep + currentStepOffset];
  if (step.groups.indexOf("FX:") >= 0 && now - lastEffectAt >= effectUpdateIntervalForGroups(step.groups)) {
    applyStepGroups(step.groups);
    lastEffectAt = now;
  }

  if (now - lastStepAt >= activeStepDurationMs) {
    currentStepOffset = (currentStepOffset + 1) % seq.stepCount;
    Step& next = steps[seq.firstStep + currentStepOffset];
    startStep(next);
  }
}
