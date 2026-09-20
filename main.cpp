#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <ThreeWire.h>
#include <RtcDS1302.h>

// ============================================================
// MINIMALIST PRODUCT UI - ESP32 REMINDER
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C

Preferences prefs;

#define MAX_REMINDERS 50

struct Reminder {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    char label[32];
    bool repeating;
    uint8_t weekday;
};

Reminder reminders[MAX_REMINDERS];
uint8_t reminderCount = 0;

// Pin Definitions
#define BTN_UP       19
#define BTN_DOWN     18
#define BTN_SELECT   16
#define BUZZER_PIN   17

#define RTC_DAT      33
#define RTC_RST      32
#define RTC_CLK      25

#define SDA_PIN      21
#define SCL_PIN      22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ThreeWire myWire(RTC_DAT, RTC_CLK, RTC_RST);
RtcDS1302<ThreeWire> Rtc(myWire);

enum Screen {
    SCREEN_HOME,
    SCREEN_REMINDERS,
    SCREEN_CALENDAR,
    SCREEN_MENU,
    SCREEN_ADD,
    SCREEN_RTC_SETUP,
    SCREEN_ALARM,
    SCREEN_INFO,
    SCREEN_DELETE
};

Screen currentScreen = SCREEN_HOME;

const char* menuItems[] = {
    "Home",
    "Tasks",
    "Calendar",
    "Add Task",
    "Delete Task",
    "Clock Setup",
    "Info"
};
const uint8_t MENU_COUNT = 7;
uint8_t menuIndex = 0;
uint8_t menuScrollOffset = 0;

unsigned long lastScreenChangeTime = 0;
#define LONG_PRESS_TIME 600
#define DEBOUNCE_MS 25

struct Button {
    uint8_t pin;
    bool state;              // debounced logical state (true = pressed)
    bool lastReading;        // last raw pin reading
    unsigned long lastChangeTime;
    unsigned long pressStartTime;
};

Button btnUp    = { BTN_UP,     false, false, 0, 0 };
Button btnDown  = { BTN_DOWN,   false, false, 0, 0 };
Button btnSel   = { BTN_SELECT, false, false, 0, 0 };

uint8_t addHour = 8;
uint8_t addMinute = 0;
uint8_t addField = 0;
char addLabel[32] = "Task";
uint8_t addWeekday = 0;
bool addRepeating = false;

uint8_t rtcField = 0;
int rtcYear, rtcMonth, rtcDay, rtcHour, rtcMinute;

uint8_t deleteIndex = 0;

int activeReminder = -1;
const char* weekdayNames[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
const char* monthNames[] = { "", "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

// Forward Declarations
void printCentered(const char* str, int y, uint8_t textSize = 1);
void beep(uint16_t duration);
void drawHeader(const char* title);
void bootSequence();
void setScreen(Screen newScreen);

void drawHome();
void drawMenu();
void drawReminders();
void drawCalendar();
void drawAddReminder();
void drawAlarm();
void drawRTCSetup();
void drawInfo();
void drawDelete();
void prepareDelete();
void deleteReminder(uint8_t index);

int getDaysInMonth(int year, int month);

void initializeRTC();
void loadReminders();
void saveReminders();
void handleButtons();
bool updateButton(Button &b, bool &pressedEdge, bool &releasedEdge);
void handleUp();
void handleDown();
void handleSelect();
void handleLongSelect();
void selectMenuItem();
void prepareAddReminder();
void changeAddValue(int amount);
void saveNewReminder();
void prepareRTCSetup();
void changeRTCValue(int amount);
void saveRTC();
bool reminderAppliesToday(Reminder &r, RtcDateTime &now);
int countTodayReminders();
void checkReminders();
void stopAlarm();

// ============================================================
// SCREEN SWITCHER WITH TRANSITION GUARD
// ============================================================

void setScreen(Screen newScreen) {
    currentScreen = newScreen;
    lastScreenChangeTime = millis();
}

// ============================================================
// HOME SCREEN
// ============================================================

void drawHome() {
    RtcDateTime now = Rtc.GetDateTime();
    display.clearDisplay();

    // Top Left Status Badge
    display.drawRoundRect(0, 0, 58, 11, 2, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(5, 2);
    display.print("PULSE 32");

    // Top Right Badge
    display.drawRoundRect(94, 0, 34, 11, 2, SSD1306_WHITE);
    display.setCursor(98, 2);
    display.print("DESK");

    // Main Clock
    char timeBuf[16];
    uint8_t h12 = now.Hour() % 12;
    if (h12 == 0) h12 = 12;
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", h12, now.Minute());

    display.setTextSize(3);
    display.setCursor(6, 15);
    display.print(timeBuf);

    // AM/PM Indicator
    display.setTextSize(1);
    display.setCursor(98, 25);
    display.print(now.Hour() >= 12 ? "PM" : "AM");

    display.drawFastHLine(4, 41, 120, SSD1306_WHITE);

    // Date Line
    char dateBuf[32];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %02d/%02d", weekdayNames[now.DayOfWeek()], now.Day(), now.Month());
    display.setCursor(6, 48);
    display.print(dateBuf);

    // Active Task Pill
    int tasks = countTodayReminders();
    char taskBuf[16];
    snprintf(taskBuf, sizeof(taskBuf), "%d TASKS", tasks);

    display.drawRoundRect(74, 46, 48, 13, 3, SSD1306_WHITE);
    display.setCursor(78, 49);
    display.print(taskBuf);

    display.display();
}

// ============================================================
// UI HELPERS & BOOT
// ============================================================

void printCentered(const char* str, int y, uint8_t textSize) {
    display.setTextSize(textSize);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
    int x = (128 - w) / 2;
    if (x < 0) x = 0;
    display.setCursor(x, y);
    display.print(str);
}

void beep(uint16_t duration) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
}

void drawHeader(const char* title) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(title);
    display.drawFastHLine(0, 11, 128, SSD1306_WHITE);
}

void bootSequence() {
    display.clearDisplay();

    const int boxX = 14;
    const int boxY = 36;
    const int boxW = 100;
    const int boxH = 12;

    for (int percent = 0; percent <= 100; percent += 2) {
        display.clearDisplay();

        printCentered("PULSE 32", 10, 1);
        printCentered("LOADING...", 23, 1);

        display.drawRoundRect(boxX, boxY, boxW, boxH, 2, SSD1306_WHITE);

        int fillWidth = map(percent, 0, 100, 0, boxW - 4);
        if (fillWidth > 0) {
            display.fillRect(boxX + 2, boxY + 2, fillWidth, boxH - 4, SSD1306_WHITE);
        }

        display.display();
        delay(25);
    }

    beep(30);
    delay(200);
}

// ============================================================
// SCREENS
// ============================================================

void drawMenu() {
    display.clearDisplay();
    drawHeader("SETTINGS");

    if (menuIndex < menuScrollOffset) {
        menuScrollOffset = menuIndex;
    } else if (menuIndex >= menuScrollOffset + 4) {
        menuScrollOffset = menuIndex - 3;
    }

    for (int i = 0; i < 4; i++) {
        int itemIdx = menuScrollOffset + i;
        if (itemIdx >= MENU_COUNT) break;

        int y = 14 + (i * 12);
        if (itemIdx == menuIndex) {
            display.fillRoundRect(2, y - 1, 120, 11, 2, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
            display.setCursor(8, y);
            display.print(menuItems[itemIdx]);
            display.setTextColor(SSD1306_WHITE);
        } else {
            display.setCursor(8, y);
            display.print(menuItems[itemIdx]);
        }
    }

    display.drawFastVLine(125, 14, 48, SSD1306_WHITE);
    int indicatorY = 14 + (menuIndex * 40 / (MENU_COUNT - 1));
    display.fillRect(124, indicatorY, 3, 8, SSD1306_WHITE);

    display.display();
}

int getDaysInMonth(int year, int month) {
    if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return 31;
}

void drawCalendar() {
    display.clearDisplay();
    RtcDateTime now = Rtc.GetDateTime();

    char headerBuf[32];
    snprintf(headerBuf, sizeof(headerBuf), "%s %d", monthNames[now.Month()], now.Year());
    drawHeader(headerBuf);

    display.setCursor(2, 13);
    display.print("S  M  T  W  T  F  S");

    int totalDays = getDaysInMonth(now.Year(), now.Month());
    int currentDay = now.Day();
    int startCol = (now.DayOfWeek() - ((now.Day() - 1) % 7) + 7) % 7;

    int row = 0;
    int col = startCol;

    for (int d = 1; d <= totalDays; d++) {
        int x = 2 + (col * 18);
        int y = 23 + (row * 8);

        if (y > 56) break;

        if (d == currentDay) {
            display.fillRect(x - 1, y - 1, 14, 9, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
            display.setCursor(x, y);
            if (d < 10) display.print("0");
            display.print(d);
            display.setTextColor(SSD1306_WHITE);
        } else {
            display.setCursor(x, y);
            if (d < 10) display.print(" ");
            display.print(d);
        }

        col++;
        if (col > 6) {
            col = 0;
            row++;
        }
    }

    display.display();
}

void drawReminders() {
    display.clearDisplay();
    drawHeader("TODAY'S TASKS");

    int shown = 0;
    for (int i = 0; i < reminderCount; i++) {
        if (!reminders[i].enabled) continue;
        if (shown >= 3) break;

        int y = 15 + (shown * 15);
        display.drawRoundRect(0, y, 128, 13, 2, SSD1306_WHITE);

        char timeBuf[16];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", reminders[i].hour, reminders[i].minute);
        display.setCursor(4, y + 3);
        display.print(timeBuf);

        display.setCursor(45, y + 3);
        display.print(reminders[i].label);

        shown++;
    }

    if (shown == 0) {
        printCentered("No active tasks", 32, 1);
    }

    display.display();
}

void drawAddReminder() {
    display.clearDisplay();
    drawHeader("NEW TASK");

    char timeBuf[32];
    snprintf(timeBuf, sizeof(timeBuf), "%s Time   %02d:%02d", (addField == 0 || addField == 1) ? ">" : " ", addHour, addMinute);
    display.setCursor(2, 18);
    display.print(timeBuf);

    char labelBuf[48];
    snprintf(labelBuf, sizeof(labelBuf), "%s Tag    %s", (addField == 2) ? ">" : " ", addLabel);
    display.setCursor(2, 30);
    display.print(labelBuf);

    char repeatBuf[32];
    snprintf(repeatBuf, sizeof(repeatBuf), "%s Repeat %s", (addField == 3) ? ">" : " ", addRepeating ? weekdayNames[addWeekday] : "Once");
    display.setCursor(2, 42);
    display.print(repeatBuf);

    if (addField == 4) {
        display.fillRoundRect(20, 51, 88, 11, 2, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        printCentered("SAVE TASK", 53, 1);
        display.setTextColor(SSD1306_WHITE);
    } else {
        printCentered("[SELECT] Next Field", 53, 1);
    }

    display.display();
}

void drawRTCSetup() {
    display.clearDisplay();
    drawHeader("CLOCK SETUP");

    char dateBuf[32];
    snprintf(dateBuf, sizeof(dateBuf), "%s Date  %02d/%02d/%d", (rtcField <= 2) ? ">" : " ", rtcDay, rtcMonth, rtcYear);
    display.setCursor(2, 20);
    display.print(dateBuf);

    char timeBuf[32];
    snprintf(timeBuf, sizeof(timeBuf), "%s Time  %02d:%02d", (rtcField == 3 || rtcField == 4) ? ">" : " ", rtcHour, rtcMinute);
    display.setCursor(2, 34);
    display.print(timeBuf);

    if (rtcField == 5) {
        display.fillRoundRect(20, 50, 88, 11, 2, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        printCentered("SAVE TIME", 52, 1);
        display.setTextColor(SSD1306_WHITE);
    } else {
        printCentered("UP/DN: Adj  SEL: Next", 52, 1);
    }

    display.display();
}

void drawInfo() {
    display.clearDisplay();
    drawHeader("INFO");

    display.setCursor(2, 15);
    display.print("Device   PULSE 32");

    display.setCursor(2, 26);
    display.print("RTC      ");
    display.print(Rtc.IsDateTimeValid() ? "OK" : "ERROR");

    char taskBuf[32];
    snprintf(taskBuf, sizeof(taskBuf), "Tasks    %d/%d", reminderCount, MAX_REMINDERS);
    display.setCursor(2, 37);
    display.print(taskBuf);

    char heapBuf[32];
    snprintf(heapBuf, sizeof(heapBuf), "Free RAM %lu KB", (unsigned long)(ESP.getFreeHeap() / 1024));
    display.setCursor(2, 48);
    display.print(heapBuf);

    printCentered("[SELECT] Back", 56, 1);

    display.display();
}

void drawDelete() {
    display.clearDisplay();
    drawHeader("DELETE TASK");

    if (reminderCount == 0) {
        printCentered("No tasks to delete", 30, 1);
        printCentered("[SELECT] Back", 52, 1);
        display.display();
        return;
    }

    if (deleteIndex >= reminderCount) deleteIndex = reminderCount - 1;
    Reminder &r = reminders[deleteIndex];

    char idxBuf[16];
    snprintf(idxBuf, sizeof(idxBuf), "%d / %d", deleteIndex + 1, reminderCount);
    display.setCursor(2, 14);
    display.print(idxBuf);

    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", r.hour, r.minute);
    display.setTextSize(2);
    display.setCursor(20, 24);
    display.print(timeBuf);

    display.setTextSize(1);
    printCentered(r.label, 45, 1);

    display.fillRoundRect(14, 53, 100, 10, 2, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    printCentered("UP/DN Pick  SEL Delete", 55, 1);
    display.setTextColor(SSD1306_WHITE);

    display.display();
}

void drawAlarm() {
    display.clearDisplay();

    display.drawRoundRect(10, 6, 108, 52, 4, SSD1306_WHITE);
    printCentered("REMINDER", 14, 1);

    if (activeReminder >= 0 && activeReminder < reminderCount) {
        printCentered(reminders[activeReminder].label, 28, 2);
    }

    display.fillRoundRect(24, 46, 80, 9, 2, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    printCentered("DISMISS", 47, 1);
    display.setTextColor(SSD1306_WHITE);

    static unsigned long lastAlarmBeep = 0;
    if (millis() - lastAlarmBeep > 400) {
        lastAlarmBeep = millis();
        beep(80);
    }

    display.display();
}

// ============================================================
// MAIN SETUP & LOOP
// ============================================================

void setup() {
    pinMode(BTN_UP, INPUT);
    pinMode(BTN_DOWN, INPUT);
    pinMode(BTN_SELECT, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    Wire.begin(SDA_PIN, SCL_PIN);

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        while (true) delay(1000);
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    bootSequence();

    Rtc.Begin();
    initializeRTC();
    loadReminders();

    beep(30);
    setScreen(SCREEN_HOME);
}

void loop() {
    handleButtons();
    checkReminders();

    switch (currentScreen) {
        case SCREEN_HOME:        drawHome(); break;
        case SCREEN_REMINDERS:   drawReminders(); break;
        case SCREEN_CALENDAR:    drawCalendar(); break;
        case SCREEN_MENU:        drawMenu(); break;
        case SCREEN_ADD:         drawAddReminder(); break;
        case SCREEN_RTC_SETUP:   drawRTCSetup(); break;
        case SCREEN_ALARM:       drawAlarm(); break;
        case SCREEN_INFO:        drawInfo(); break;
        case SCREEN_DELETE:      drawDelete(); break;
    }

    delay(30);
}

// ============================================================
// NAVIGATION & SYSTEM LOGIC
// ============================================================

void initializeRTC() {
    if (Rtc.GetIsWriteProtected()) Rtc.SetIsWriteProtected(false);
    if (!Rtc.GetIsRunning()) Rtc.SetIsRunning(true);
    
    RtcDateTime now = Rtc.GetDateTime();
    
    // Check if RTC is invalid or register reset to year 2000 default (1/1 issue)
    if (!Rtc.IsDateTimeValid() || now.Year() < 2024 || now.Year() > 2029) {
        RtcDateTime compiled(__DATE__, __TIME__);
        Rtc.SetDateTime(compiled);
    }
}

bool updateButton(Button &b, bool &pressedEdge, bool &releasedEdge) {
    pressedEdge = false;
    releasedEdge = false;

    bool reading = digitalRead(b.pin) == HIGH;

    if (reading != b.lastReading) {
        b.lastChangeTime = millis();
        b.lastReading = reading;
    }

    if (millis() - b.lastChangeTime > DEBOUNCE_MS && reading != b.state) {
        b.state = reading;
        if (b.state) {
            pressedEdge = true;
            b.pressStartTime = millis();
        } else {
            releasedEdge = true;
        }
    }

    return b.state;
}

void handleButtons() {
    bool upPressed, upReleased;
    bool downPressed, downReleased;
    bool selPressed, selReleased;

    updateButton(btnUp, upPressed, upReleased);
    updateButton(btnDown, downPressed, downReleased);
    updateButton(btnSel, selPressed, selReleased);

    bool ignoreActions = (millis() - lastScreenChangeTime < 200);
    if (ignoreActions) return;

    if (upPressed)   { beep(12); handleUp(); }
    if (downPressed) { beep(12); handleDown(); }

    if (selReleased) {
        unsigned long duration = millis() - btnSel.pressStartTime;
        if (duration >= LONG_PRESS_TIME) handleLongSelect();
        else handleSelect();
    }
}

void handleUp() {
    switch (currentScreen) {
        case SCREEN_MENU:      menuIndex = (menuIndex == 0) ? MENU_COUNT - 1 : menuIndex - 1; break;
        case SCREEN_ADD:       changeAddValue(-1); break;
        case SCREEN_RTC_SETUP: changeRTCValue(-1); break;
        case SCREEN_DELETE:
            if (reminderCount > 0 && deleteIndex > 0) deleteIndex--;
            break;
        default: break;
    }
}

void handleDown() {
    switch (currentScreen) {
        case SCREEN_MENU:      menuIndex = (menuIndex + 1) % MENU_COUNT; break;
        case SCREEN_ADD:       changeAddValue(1); break;
        case SCREEN_RTC_SETUP: changeRTCValue(1); break;
        case SCREEN_DELETE:
            if (reminderCount > 0 && deleteIndex < reminderCount - 1) deleteIndex++;
            break;
        default: break;
    }
}

void handleSelect() {
    switch (currentScreen) {
        case SCREEN_HOME:
            setScreen(SCREEN_MENU);
            break;
        case SCREEN_REMINDERS:
        case SCREEN_CALENDAR:
        case SCREEN_INFO:
            setScreen(SCREEN_MENU);
            break;
        case SCREEN_MENU:
            selectMenuItem();
            break;
        case SCREEN_ADD:
            if (addField < 4) {
                addField++;
            } else {
                saveNewReminder();
                setScreen(SCREEN_MENU);
            }
            break;
        case SCREEN_RTC_SETUP:
            if (rtcField < 5) {
                rtcField++;
            } else {
                saveRTC();
                setScreen(SCREEN_MENU);
            }
            break;
        case SCREEN_ALARM:
            stopAlarm();
            break;
        case SCREEN_DELETE:
            if (reminderCount == 0) {
                setScreen(SCREEN_MENU);
            } else {
                deleteReminder(deleteIndex);
            }
            break;
    }
}

void selectMenuItem() {
    switch (menuIndex) {
        case 0: setScreen(SCREEN_HOME); break;
        case 1: setScreen(SCREEN_REMINDERS); break;
        case 2: setScreen(SCREEN_CALENDAR); break;
        case 3: prepareAddReminder(); setScreen(SCREEN_ADD); break;
        case 4: prepareDelete(); setScreen(SCREEN_DELETE); break;
        case 5: prepareRTCSetup(); setScreen(SCREEN_RTC_SETUP); break;
        case 6: setScreen(SCREEN_INFO); break;
    }
}

void handleLongSelect() {
    beep(30);
    if (currentScreen == SCREEN_ALARM) {
        stopAlarm();
    } else {
        setScreen(SCREEN_HOME);
    }
}

void prepareAddReminder() {
    addHour = 8;
    addMinute = 0;
    addField = 0;
    strcpy(addLabel, "Task");
    addRepeating = false;
    addWeekday = 0;
}

void changeAddValue(int amount) {
    if (addField == 0) {
        addHour = (addHour + amount + 24) % 24;
    } else if (addField == 1) {
        addMinute = (addMinute + amount + 60) % 60;
    } else if (addField == 2) {
        const char* labels[] = { "Task", "Study", "Project", "Water", "Meeting", "Homework" };
        static int idx = 0;
        idx = (idx + amount + 6) % 6;
        strncpy(addLabel, labels[idx], sizeof(addLabel) - 1);
    } else if (addField == 3) {
        addRepeating = !addRepeating;
    }
}

void saveNewReminder() {
    if (reminderCount >= MAX_REMINDERS) return;
    reminders[reminderCount].enabled = true;
    reminders[reminderCount].hour = addHour;
    reminders[reminderCount].minute = addMinute;
    strncpy(reminders[reminderCount].label, addLabel, sizeof(reminders[reminderCount].label) - 1);
    reminders[reminderCount].repeating = addRepeating;
    reminders[reminderCount].weekday = addWeekday;
    reminderCount++;
    saveReminders();
    beep(40);
}

void prepareDelete() {
    deleteIndex = 0;
}

void deleteReminder(uint8_t index) {
    if (index >= reminderCount) return;
    for (uint8_t i = index; i < reminderCount - 1; i++) {
        reminders[i] = reminders[i + 1];
    }
    reminderCount--;
    if (deleteIndex >= reminderCount && deleteIndex > 0) deleteIndex--;
    saveReminders();
    beep(60);
}

void prepareRTCSetup() {
    RtcDateTime now = Rtc.GetDateTime();
    rtcYear = now.Year();
    rtcMonth = now.Month();
    rtcDay = now.Day();
    rtcHour = now.Hour();
    rtcMinute = now.Minute();
    rtcField = 0;
}

void changeRTCValue(int amount) {
    if (rtcField == 0) {
        int maxDays = getDaysInMonth(rtcYear, rtcMonth);
        rtcDay = rtcDay + amount;
        if (rtcDay < 1) rtcDay = maxDays;
        if (rtcDay > maxDays) rtcDay = 1;
    }
    else if (rtcField == 1) rtcMonth = constrain(rtcMonth + amount, 1, 12);
    else if (rtcField == 2) rtcYear = constrain(rtcYear + amount, 2024, 2029);
    else if (rtcField == 3) rtcHour = (rtcHour + amount + 24) % 24;
    else if (rtcField == 4) rtcMinute = (rtcMinute + amount + 60) % 60;
}

void saveRTC() {
    if (Rtc.GetIsWriteProtected()) Rtc.SetIsWriteProtected(false);
    RtcDateTime dt(rtcYear, rtcMonth, rtcDay, rtcHour, rtcMinute, 0);
    Rtc.SetDateTime(dt);
    Rtc.SetIsRunning(true);
    beep(40);
}

void saveReminders() {
    prefs.begin("reminder32", false);
    prefs.putUChar("count", reminderCount);
    prefs.putBytes("data", reminders, sizeof(reminders));
    prefs.end();
}

void loadReminders() {
    prefs.begin("reminder32", true);
    reminderCount = prefs.getUChar("count", 0);
    if (reminderCount > MAX_REMINDERS) reminderCount = 0;
    prefs.getBytes("data", reminders, sizeof(reminders));
    prefs.end();
}

bool reminderAppliesToday(Reminder &r, RtcDateTime &now) {
    if (!r.enabled) return false;
    if (r.repeating) return r.weekday == now.DayOfWeek();
    return true;
}

int countTodayReminders() {
    RtcDateTime now = Rtc.GetDateTime();
    int count = 0;
    for (int i = 0; i < reminderCount; i++) {
        if (reminderAppliesToday(reminders[i], now)) count++;
    }
    return count;
}

void checkReminders() {
    static int lastMinute = -1;
    RtcDateTime now = Rtc.GetDateTime();
    if (now.Minute() == lastMinute) return;
    lastMinute = now.Minute();

    for (int i = 0; i < reminderCount; i++) {
        if (!reminders[i].enabled) continue;
        if (reminders[i].hour != now.Hour() || reminders[i].minute != now.Minute()) continue;
        if (!reminderAppliesToday(reminders[i], now)) continue;

        activeReminder = i;
        setScreen(SCREEN_ALARM);
        if (!reminders[i].repeating) {
            reminders[i].enabled = false;
            saveReminders();
        }
        break;
    }
}

void stopAlarm() {
    digitalWrite(BUZZER_PIN, LOW);
    activeReminder = -1;
    setScreen(SCREEN_HOME);
}