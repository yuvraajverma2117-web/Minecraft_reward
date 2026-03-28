// ============================================================
//  MINECRAFT REWARD SYSTEM v5.0
//  GitHub: minecraft-reward-system
//
//  HARDWARE:
//    - Arduino Uno / Mega
//    - 16x2 I2C LCD (address 0x27)
//    - Joystick module (X=A0, Y=A1, BTN=D2)
//
//  FEATURES:
//    - 50 questions across 5 categories (100,000 combos)
//    - Swipe navigation between 3 pages
//    - EEPROM persistent stats (survives power off)
//    - Bordered selection UI
//    - Triangle navigation indicators
//    - Earn Minecraft time by answering honestly
//    - Free timer with adjustable minutes
//    - Stats reset by holding button 5 seconds
// ============================================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// ============================================================
// HARDWARE PINS
// ============================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);
const int JOY_X   = A0;
const int JOY_Y   = A1;
const int JOY_BTN = 2;

// ============================================================
// EEPROM ADDRESSES
// ============================================================
const int ADDR_LIFETIME_SECS = 0;   // long = 4 bytes (addr 0-3)
const int ADDR_LIFETIME_SESS = 4;   // int  = 2 bytes (addr 4-5)
const int ADDR_MAGIC         = 6;   // byte magic number to detect first boot

// ============================================================
// GLOBAL STATE
// ============================================================
int  currentPage    = 0;   // 0=Homework 1=Stats 2=FreeTimer
int  sessionsDone   = 0;   // sessions this power cycle
long sessionEarned  = 0;   // seconds earned this cycle (for countdown)
long lifetimeSecs   = 0;   // total seconds earned ever (EEPROM)
int  lifetimeSess   = 0;   // total sessions ever (EEPROM)
char qbuf[18];             // question text buffer

// ============================================================
// QUESTION BANK
// 5 categories x 10 questions = 50 total
// Arduino picks 1 random from each category = 100,000 combos
// ============================================================
void getQuestion(int category, int index) {
  const char* bank[5][10] = {
    // Category 1: Did you START?
    {
      "Did you start?",
      "Opened the book?",
      "Got your pen?",
      "Sat at a table?",
      "Phone put away?",
      "No distractions?",
      "Started on time?",
      "Bag was ready?",
      "Notebook open?",
      "Timer was set?"
    },
    // Category 2: Did you UNDERSTAND?
    {
      "Understood it?",
      "Read it fully?",
      "No confusions?",
      "All made sense?",
      "Can explain it?",
      "Got the concept?",
      "Reread if lost?",
      "Knew the steps?",
      "Knew the topic?",
      "No guessing?"
    },
    // Category 3: Did you WORK HARD?
    {
      "Took 10+ mins?",
      "Did it yourself?",
      "No copy paste?",
      "Wrote it neatly?",
      "All steps shown?",
      "No shortcuts?",
      "Tried hard parts?",
      "No help taken?",
      "Full effort?",
      "Did extra too?"
    },
    // Category 4: Did you CHECK?
    {
      "Checked answers?",
      "Reread your work?",
      "Fixed mistakes?",
      "Spellings ok?",
      "All questions done?",
      "Nothing skipped?",
      "Verified the maths?",
      "Neat and tidy?",
      "No blank spaces?",
      "Double checked?"
    },
    // Category 5: Are you SURE? (lie detector!)
    {
      "100% honest?",
      "Not guessing?",
      "Really done it?",
      "No lies at all?",
      "Can prove it?",
      "Teacher will pass?",
      "Worth full marks?",
      "No missing parts?",
      "Fully complete?",
      "Swear its done?"
    }
  };
  strncpy(qbuf, bank[category][index], 17);
  qbuf[17] = '\0';
}

// ============================================================
// EEPROM SAVE / LOAD
// ============================================================
void saveStats() {
  EEPROM.put(ADDR_LIFETIME_SECS, lifetimeSecs);
  EEPROM.put(ADDR_LIFETIME_SESS, lifetimeSess);
  EEPROM.write(ADDR_MAGIC, 42); // magic number = initialized
}

void loadStats() {
  byte magic = EEPROM.read(ADDR_MAGIC);
  if (magic != 42) {
    // First boot ever — initialize clean
    lifetimeSecs = 0;
    lifetimeSess = 0;
    saveStats();
    return;
  }
  EEPROM.get(ADDR_LIFETIME_SECS, lifetimeSecs);
  EEPROM.get(ADDR_LIFETIME_SESS, lifetimeSess);
  // Sanity check
  if (lifetimeSecs < 0 || lifetimeSecs > 999999) lifetimeSecs = 0;
  if (lifetimeSess < 0 || lifetimeSess > 99999)  lifetimeSess = 0;
}

void resetStats() {
  lifetimeSecs = 0;
  lifetimeSess = 0;
  saveStats();
}

// ============================================================
// LCD HELPERS
// ============================================================
void showMsg(const char* line1, const char* line2, int waitMs) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1);
  lcd.setCursor(0, 1); lcd.print(line2);
  delay(waitMs);
}

void printPadded(String s, int totalLen) {
  lcd.print(s.substring(0, totalLen));
  for (int i = s.length(); i < totalLen; i++) lcd.print(" ");
}

// ============================================================
// HOME SCREEN
// Shows bordered selection + triangle navigation
// ============================================================
void showHome() {
  lcd.clear();

  // Top row: page name with border brackets
  lcd.setCursor(0, 0);
  if (currentPage == 0) lcd.print("[Homework       ]");
  if (currentPage == 1) lcd.print("[Stats          ]");
  if (currentPage == 2) lcd.print("[Free Timer     ]");

  // Bottom row: triangle navigation + button hint
  lcd.setCursor(0, 1);
  if (currentPage == 0) lcd.print("         >> OK  ");
  if (currentPage == 1) lcd.print(" <<      >> OK  ");
  if (currentPage == 2) lcd.print(" <<         OK  ");
}

// ============================================================
// JOYSTICK READING
// ============================================================
bool waitForButtonOrSwipe() {
  while (true) {
    // Button press = select
    if (digitalRead(JOY_BTN) == LOW) {
      delay(300);
      return true;
    }
    // Joystick left = previous page
    int x = analogRead(JOY_X);
    if (x < 300) {
      currentPage = (currentPage - 1 + 3) % 3;
      delay(300);
      return false;
    }
    // Joystick right = next page
    if (x > 700) {
      currentPage = (currentPage + 1) % 3;
      delay(300);
      return false;
    }
  }
}

bool waitYesNo() {
  lcd.setCursor(0, 1);
  lcd.print("^^=Yes   vv=No  ");
  while (true) {
    int y = analogRead(JOY_Y);
    if (y < 300) { delay(300); return true;  }
    if (y > 700) { delay(300); return false; }
  }
}

// ============================================================
// QUIZ SESSION
// 5 random questions (1 per category) -> score -> time earned
// ============================================================
void runQuizSession() {
  int score = 0;

  for (int cat = 0; cat < 5; cat++) {
    int qi = random(0, 10);
    getQuestion(cat, qi);

    lcd.clear();
    lcd.setCursor(0, 0);
    // Show "Q1: Did you start?" trimmed to 16 chars
    String header = "Q";
    header += (cat + 1);
    header += ": ";
    header += String(qbuf);
    if (header.length() > 16) header = header.substring(0, 16);
    lcd.print(header);

    bool answer = waitYesNo();
    if (answer) score++;

    // Brief feedback flash
    lcd.setCursor(0, 1);
    if (answer) lcd.print("  >> Noted! <<  ");
    else        lcd.print("  >> Hmm...  << ");
    delay(600);
  }

  // Calculate time earned
  long earned = 0;
  if      (score == 5) { earned = 120; showMsg("** PERFECT! **  ", "+2 min MC time! ", 2500); }
  else if (score == 4) { earned = 90;  showMsg("Great job! 4/5  ", "+1m 30s MC time!", 2500); }
  else if (score == 3) { earned = 60;  showMsg("Good effort 3/5 ", "+1 min MC time! ", 2500); }
  else if (score == 2) { earned = 30;  showMsg("Hmm... 2/5      ", "+30s. Be honest!", 2500); }
  else if (score == 1) { earned = 0;   showMsg("Only 1/5...     ", "No MC time! :(  ", 2500); }
  else                 { earned = 0;   showMsg("Lying detected! ", "No MC time! >:( ", 2500); }

  sessionEarned += earned;
  lifetimeSecs  += earned;
  saveStats();

  // Show score summary
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Score: ");
  lcd.print(score);
  lcd.print("/5    ");
  lcd.setCursor(0, 1);
  lcd.print("+");
  lcd.print(earned / 60);
  lcd.print("m ");
  lcd.print(earned % 60);
  lcd.print("s earned     ");
  delay(2000);
}

// ============================================================
// MINECRAFT COUNTDOWN TIMER
// Runs after all 5 sessions are complete
// ============================================================
void startMinecraftCountdown() {
  // Show total earned
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("You earned:     ");
  lcd.setCursor(0, 1);
  lcd.print(sessionEarned / 60);
  lcd.print("m ");
  lcd.print(sessionEarned % 60);
  lcd.print("s of MC!        ");
  delay(3000);

  showMsg("Get ready...    ", "Minecraft time! ", 2000);

  // Countdown loop
  for (long s = sessionEarned; s >= 0; s--) {
    int m  = s / 60;
    int sc = s % 60;
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("** MINECRAFT ** ");
    lcd.setCursor(0, 1);
    lcd.print(m); lcd.print("m ");
    if (sc < 10) lcd.print("0");
    lcd.print(sc); lcd.print("s remaining  ");
    delay(1000);
  }

  showMsg("** TIME IS UP **", "Stop playing!!  ", 3000);
  showMsg("Good job today! ", "Do homework 2mo!", 3000);
  sessionEarned = 0;
}

// ============================================================
// STATS PAGE
// Shows lifetime stats, hold button 5s to reset
// ============================================================
void showStatsPage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Total:");
  lcd.print(lifetimeSecs / 60);
  lcd.print("m");
  lcd.print(lifetimeSecs % 60);
  lcd.print("s      ");
  lcd.setCursor(0, 1);
  lcd.print("Sess:");
  lcd.print(lifetimeSess);
  lcd.print(" Hld=Rst");

  // Wait up to 5 seconds — hold button to reset
  unsigned long start = millis();
  while (millis() - start < 5000) {
    if (digitalRead(JOY_BTN) == LOW) {
      delay(100);
      // Check they're still holding
      unsigned long holdStart = millis();
      while (digitalRead(JOY_BTN) == LOW) {
        lcd.setCursor(0, 1);
        int held = (millis() - holdStart) / 1000;
        lcd.print("Resetting in ");
        lcd.print(3 - held);
        lcd.print("s ");
        delay(100);
        if (millis() - holdStart >= 3000) {
          resetStats();
          showMsg("Stats RESET!    ", "All cleared.    ", 2000);
          return;
        }
      }
    }
  }
}

// ============================================================
// FREE TIMER PAGE
// Set any duration with joystick, then countdown
// ============================================================
void freeTimerPage() {
  int freeMinutes = 5;

  // Adjust time loop
  while (true) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Set free timer: ");
    lcd.setCursor(0, 1);
    lcd.print("[");
    lcd.print(freeMinutes);
    lcd.print("m]  Btn=Start   ");

    // Short polling loop
    unsigned long t = millis();
    while (millis() - t < 300) {
      if (digitalRead(JOY_BTN) == LOW) { delay(300); goto startFreeTimer; }
    }

    int y = analogRead(JOY_Y);
    if (y < 300 && freeMinutes < 60) freeMinutes++;
    if (y > 700 && freeMinutes > 1)  freeMinutes--;
  }

  startFreeTimer:
  long freeSecs = (long)freeMinutes * 60;
  showMsg("Free timer start", "Enjoy your time!", 1500);

  for (long s = freeSecs; s >= 0; s--) {
    int m  = s / 60;
    int sc = s % 60;
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Free Timer:     ");
    lcd.setCursor(0, 1);
    lcd.print(m); lcd.print("m ");
    if (sc < 10) lcd.print("0");
    lcd.print(sc); lcd.print("s left      ");
    delay(1000);
  }

  showMsg("Free time done! ", "Back to work!   ", 3000);
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  randomSeed(analogRead(A2)); // Unconnected pin = true random seed
  lcd.init();
  lcd.backlight();
  pinMode(JOY_BTN, INPUT_PULLUP);

  loadStats(); // Load saved stats from EEPROM

  // Splash screen
  showMsg("Minecraft Reward", "   System v5.0  ", 1500);
  showMsg("<< >> navigate  ", "Btn to select   ", 1500);
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  showHome();
  bool selected = waitForButtonOrSwipe();

  if (!selected) return; // Just swiped, redraw home

  // ---- HOMEWORK PAGE ----
  if (currentPage == 0) {
    runQuizSession();
    sessionsDone++;
    lifetimeSess++;
    saveStats();

    if (sessionsDone >= 5) {
      showMsg("All 5 done!     ", "MC time starts! ", 2000);
      startMinecraftCountdown();
      sessionsDone  = 0;
      sessionEarned = 0;
    } else {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Progress:       ");
      lcd.setCursor(0, 1);
      lcd.print(sessionsDone);
      lcd.print(" / 5 done       ");
      delay(2000);
    }
  }

  // ---- STATS PAGE ----
  else if (currentPage == 1) {
    showStatsPage();
  }

  // ---- FREE TIMER PAGE ----
  else if (currentPage == 2) {
    freeTimerPage();
  }
}
