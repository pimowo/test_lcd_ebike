#include <Wire.h>
#include <U8g2lib.h>

// Definicje pinów
#define I2C_SDA 8
#define I2C_SCL 9
#define BTN_UP 4
#define BTN_DOWN 5
#define BTN_SET 6

// Inicjalizacja wyświetlacza
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

// Stałe czasowe dla przycisków
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long BUTTON_DELAY = 200;
const unsigned long LONG_PRESS_TIME = 1000;

// Typ wyświetlanego parametru
enum DisplayMode {
    SPEED,      // Prędkość km/h
    TRIP,       // Licznik dzienny km
    ODOMETER,   // Przebieg całkowity km
    TEMP,       // Temperatura °C
    POWER,      // Moc W
    ENERGY,     // Zużycie energii Wh
    BATT_CAP    // Pojemność baterii Ah
};

// Zmienne globalne dla wyświetlania
DisplayMode currentDisplay = SPEED;
int assistLevel = 3;
bool assistLevelAsText = false;

// Zmienne dla obsługi przycisków
unsigned long lastButtonPress = 0;
unsigned long lastDebounceTime = 0;
unsigned long upPressStartTime = 0;
unsigned long downPressStartTime = 0;
unsigned long setPressStartTime = 0;
bool upLongPressExecuted = false;
bool downLongPressExecuted = false;
bool setLongPressExecuted = false;

// Symulowane dane pomiarowe
float speed = 25.7;
float tripDistance = 1.5;    // km
float totalDistance = 123.4; // km
float temperature = 25.3;    // °C
int power = 250;            // W
float energyConsumption = 12.4; // Wh
float batteryCapacity = 14.5;   // Ah
int batteryPercent = 75;
float batteryVoltage = 47.8;
int hour = 12;
int minute = 35;
int lightMode = 0; // 0=brak, 1=Dzień, 2=Noc
int assistMode = 0; // 0=PAS, 1=STOP, 2=GAZ, 3=P+G

void drawHorizontalLine() {
  display.drawHLine(4, 17, 122);
}

void drawVerticalLine() {
  display.drawVLine(45, 22, 48);
}

void drawTopBar() {
  display.setFont(u8g2_font_pxplusibmvga9_mf);
  
  // Czas
  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", hour, minute);
  display.drawStr(0, 13, timeStr);
  
  // Bateria
  char battStr[5];
  sprintf(battStr, "%d%%", batteryPercent);
  display.drawStr(65, 13, battStr);
  
  // Napięcie
  char voltStr[6];
  sprintf(voltStr, "%.0fV", batteryVoltage);
  display.drawStr(100, 13, voltStr);
}

void drawLightStatus() {
  display.setFont(u8g2_font_profont11_tr);
  
  switch(lightMode) {
    case 1:
      display.drawStr(10, 63, "Dzien");
      break;
    case 2:
      display.drawStr(10, 63, "Noc");
      break;
  }
}

void drawAssistLevel() {
  display.setFont(u8g2_font_logisoso20_tf);
  
  if (assistLevelAsText) {
    display.drawStr(12, 43, "T"); // Tryb tekstowy - tylko litera T
  } else {
    char levelStr[2];
    sprintf(levelStr, "%d", assistLevel);
    display.drawStr(12, 43, levelStr); // Tryb numeryczny - cyfry 0-5
  }
  
  display.setFont(u8g2_font_profont11_tr);
  const char* modeText;
  switch(assistMode) {
    case 0:
      modeText = "PAS";
      break;
    case 1:
      modeText = "STOP";
      break;
    case 2:
      modeText = "GAZ";
      break;
    case 3:
      modeText = "P+G";
      break;
  }
  display.drawStr(10, 54, modeText);
}

void drawMainDisplay() {
  display.setFont(u8g2_font_logisoso20_tf);
  char valueStr[10];
  const char* unitStr;
  
  switch(currentDisplay) {
    case SPEED:
      sprintf(valueStr, "%4.1f", speed);
      unitStr = "km/h";
      break;
    case TRIP:
      sprintf(valueStr, "%4.1f", tripDistance);
      unitStr = "km";
      break;
    case ODOMETER:
      //sprintf(valueStr, "%4.0f", totalDistance);
      sprintf(valueStr, "12345", totalDistance);
      unitStr = "km";
      break;
    case TEMP:
      sprintf(valueStr, "%4.1f", temperature);
      unitStr = "°C";
      break;
    case POWER:
      sprintf(valueStr, "%4d", power);
      unitStr = "W";
      break;
    case ENERGY:
      sprintf(valueStr, "%4.1f", energyConsumption);
      unitStr = "Wh";
      break;
    case BATT_CAP:
      sprintf(valueStr, "%4.1f", batteryCapacity);
      unitStr = "Ah";
      break;
  }
  
  //display.drawStr(53, 43, valueStr);

  int valueWidth = display.getStrWidth(valueStr);
  display.drawStr(128 - valueWidth, 43, valueStr); // Bez dodatkowego marginesu
  
  // Jednostka
  display.setFont(u8g2_font_profont11_tr);
  int unitWidth = display.getStrWidth(unitStr);
  display.drawStr(128 - unitWidth, 53, unitStr);
  
  // Opis - wyrównany do prawej
  const char* descText;
  switch(currentDisplay) {
    case SPEED:
      descText = "Predkosc";
      break;
    case TRIP:
      descText = "Dystans";
      break;
    case ODOMETER:
      descText = "Przebieg";
      break;
    case TEMP:
      descText = "Temperatura";
      break;
    case POWER:
      descText = "Moc";
      break;
    case ENERGY:
      descText = "Energia";
      break;
    case BATT_CAP:
      descText = "Bateria";
      break;
  }
  
  //int16_t width = display.getStrWidth(descText);
  //display.drawStr(100 - width, 63, descText);
  display.drawStr(52, 62, descText);
}

void handleButtons() {
    unsigned long currentTime = millis();
    
    // Odczyt stanu przycisków
    bool upState = digitalRead(BTN_UP);
    bool downState = digitalRead(BTN_DOWN);
    bool setState = digitalRead(BTN_SET);
    
    // Obsługa przycisku UP
    if (!upState && (currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
        if (!upPressStartTime) {
            upPressStartTime = currentTime;
        } else if (!upLongPressExecuted && (currentTime - upPressStartTime) > LONG_PRESS_TIME) {
            // Długie naciśnięcie UP - przełączanie trybu wyświetlania assist level
            assistLevelAsText = !assistLevelAsText;
            upLongPressExecuted = true;
        }
    } else if (upState && upPressStartTime) {
        if (!upLongPressExecuted && (currentTime - upPressStartTime) < LONG_PRESS_TIME) {
            // Krótkie naciśnięcie UP - zwiększenie poziomu wspomagania
            if (assistLevel < 5) assistLevel++;
        }
        upPressStartTime = 0;
        upLongPressExecuted = false;
        lastDebounceTime = currentTime;
    }
    
    // Obsługa przycisku DOWN
    if (!downState && (currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
        if (!downPressStartTime) {
            downPressStartTime = currentTime;
        } else if (!downLongPressExecuted && (currentTime - downPressStartTime) > LONG_PRESS_TIME) {
            // Długie naciśnięcie DOWN - zmiana trybu oświetlenia
            lightMode = (lightMode + 1) % 3;  // 0=brak, 1=Dzień, 2=Noc
            downLongPressExecuted = true;
        }
    } else if (downState && downPressStartTime) {
        if (!downLongPressExecuted && (currentTime - downPressStartTime) < LONG_PRESS_TIME) {
            // Krótkie naciśnięcie DOWN - zmniejszenie poziomu wspomagania
            if (assistLevel > 0) assistLevel--;
        }
        downPressStartTime = 0;
        downLongPressExecuted = false;
        lastDebounceTime = currentTime;
    }
    
    // Obsługa przycisku SET
    if (!setState && (currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
        if (!setPressStartTime) {
            setPressStartTime = currentTime;
        }
    } else if (setState && setPressStartTime) {
        if ((currentTime - setPressStartTime) < LONG_PRESS_TIME) {
            // Krótkie naciśnięcie SET - zmiana trybu wyświetlania
            currentDisplay = (DisplayMode)((currentDisplay + 1) % 7);
        }
        setPressStartTime = 0;
        lastDebounceTime = currentTime;
    }
}

void setup() {
    Serial.begin(115200);
    
    // Konfiguracja pinów
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_SET, INPUT_PULLUP);
    
    // Inicjalizacja I2C i wyświetlacza
    Wire.begin(I2C_SDA, I2C_SCL);
    display.begin();
    display.enableUTF8Print();
    display.setFontDirection(0);
}

void loop() {
    static unsigned long lastButtonCheck = 0;
    static unsigned long lastUpdate = 0;
    const unsigned long buttonInterval = 10; // Interwał sprawdzania przycisków (10 ms)
    const unsigned long updateInterval = 2000; // Interwał aktualizacji danych (2000 ms)

    unsigned long currentTime = millis();

    // Obsługa przycisków
    if (currentTime - lastButtonCheck >= buttonInterval) {
        handleButtons();
        lastButtonCheck = currentTime;
    }

    // Rysowanie interfejsu
    display.clearBuffer();
    drawTopBar();
    drawHorizontalLine();
    drawVerticalLine();
    drawAssistLevel();
    drawMainDisplay();
    drawLightStatus();
    display.sendBuffer();

    // Symulacja zmiany danych
    if (currentTime - lastUpdate >= updateInterval) {
        speed = (speed >= 35.0) ? 0.0 : speed + 0.1;
        tripDistance += 0.1;
        totalDistance += 0.1;
        temperature = 20.0 + random(100) / 10.0;
        power = 100 + random(300);
        energyConsumption += 0.2;
        batteryCapacity = 14.5 - (random(20) / 10.0);

        minute = (minute >= 59) ? 0 : minute + 1;
        hour = (minute == 0) ? (hour >= 23 ? 0 : hour + 1) : hour;
        batteryPercent = (batteryPercent <= 0) ? 100 : batteryPercent - 1;
        batteryVoltage = (batteryVoltage <= 42.0) ? 50.0 : batteryVoltage - 0.1;
        assistMode = (assistMode + 1) % 4;
        lastUpdate = currentTime;
    }
}

/**************************************
        przełaczanie ekranów A-H przez krótkie naciśnięcia < 1s
        wejście do ekranu A-H przez długie naciśnięcie <= 1s
        przełaczanie ekranów 1-x przez krótkie naciśnięcia < 1s
        wyjscie do ekranu A-H przez długie naciśnięcie >= 1s
        przełączenie USB przez krótkie naciśnięcie dwóch przycisków UP i DOWN
        kasowanie liczników przez krótkie naciśnięcie dwóch przycisków UP i DOWN
            
        A 
        1 predkosc km/h
        2 predkosc AVG km/h
        3 predkosc MAX km/h
        4 kadencja RPM
        5 kadencja AVG RPM
        B
        1 zasięg km
        2 dystans km
        3 przebieg km
        C
        1 temperatura °C
        2 sterownik °C
        3 silnik °C
        D
        1 moc W
        2 moc AVG W
        3 moc MAX W
        E
        1 bateria V
        2 natężenie A
        3 energia Ah
        4 energia Wh
        5 pojemność %
        F
        1 Ciśnienie bar
        2 Temperatura C
        3 napięcie V
        G
        1 USB on/off
        H
        1 info
        2 kasowanie liczników
          - predkosc AVG km/h
          - predkosc MAX km/h
          - kadencja AVG RPM
          - zasięg km
          - dystans km
          - moc AVG W
          - moc MAX W 
**************************************/
