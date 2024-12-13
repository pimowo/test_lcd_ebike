#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>

// Definicje pinów
#define I2C_SDA 21
#define I2C_SCL 22

#define BTN_UP 13
#define BTN_DOWN 14
#define BTN_SET 12

#define FrontDayPin 2  // światła dzienne
#define FrontPin 4     // światła zwykłe
#define RealPin 15     // tylne światło

// Dodaj obiekt RTC
RTC_DS3231 rtc;

// Inicjalizacja wyświetlacza
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

// Stałe czasowe dla przycisków
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long BUTTON_DELAY = 200;
const unsigned long LONG_PRESS_TIME = 1000;

bool displayActive = false;
const unsigned long GOODBYE_DELAY = 5000; // 5 sekund na komunikaty
const unsigned long SET_LONG_PRESS = 3000; // 3 sekundy na długie naciśnięcie SET
unsigned long messageStartTime = 0; // Używane zarówno dla powitania jak i pożegnania
bool showingWelcome = false; // Flaga dla wyświetlania powitania

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
int lightMode = 0; // 0=brak, 1=Dzień, 2=Noc
int assistMode = 0; // 0=PAS, 1=STOP, 2=GAZ, 3=P+G

void drawHorizontalLine() {
  display.drawHLine(4, 17, 122);
}

void drawVerticalLine() {
  display.drawVLine(45, 22, 48);
}

void drawTopBar() {
  static bool colonVisible = true;
  static unsigned long lastColonToggle = 0;
  const unsigned long COLON_TOGGLE_INTERVAL = 500; // Miganie co 500ms (pół sekundy)
    
  display.setFont(u8g2_font_pxplusibmvga9_mf);
  
  // Pobierz aktualny czas z RTC
  DateTime now = rtc.now();

  // Czas z migającym dwukropkiem
  char timeStr[6];
  if (colonVisible) {
      sprintf(timeStr, "%02d:%02d", now.hour(), now.minute());
  } else {
      sprintf(timeStr, "%02d %02d", now.hour(), now.minute());
  }
  display.drawStr(0, 13, timeStr);
  
  // Przełącz stan dwukropka co COLON_TOGGLE_INTERVAL
  if (millis() - lastColonToggle >= COLON_TOGGLE_INTERVAL) {
      colonVisible = !colonVisible;
      lastColonToggle = millis();
  }

  // Bateria
  char battStr[5];
  sprintf(battStr, "%d%%", batteryPercent);
  display.drawStr(60, 13, battStr);
  
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
    
    // Obsługa przycisku SET dla wyłączonego wyświetlacza
    if (!displayActive) {
        if (!setState && (currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
            if (!setPressStartTime) {
                setPressStartTime = currentTime;
            } else if (!setLongPressExecuted && (currentTime - setPressStartTime) > SET_LONG_PRESS) {
                // Włączanie wyświetlacza
                display.clearBuffer();
                display.setFont(u8g2_font_pxplusibmvga9_mf);
                display.drawStr(40, 32, "Witaj!");
                display.sendBuffer();
                messageStartTime = currentTime;
                setLongPressExecuted = true;
                showingWelcome = true;
                displayActive = true;
            }
        } else if (setState && setPressStartTime) {
            setPressStartTime = 0;
            setLongPressExecuted = false;
            lastDebounceTime = currentTime;
        }
        return; // Wyjdź z funkcji jeśli wyświetlacz jest wyłączony
    }
    
    // Reszta obsługi przycisków gdy wyświetlacz jest włączony
    if (!showingWelcome) { // Nie obsługuj innych przycisków podczas powitania
        // Obsługa przycisku UP
        if (!upState && (currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
            if (!upPressStartTime) {
                upPressStartTime = currentTime;
            } else if (!upLongPressExecuted && (currentTime - upPressStartTime) > LONG_PRESS_TIME) {
                lightMode = (lightMode + 1) % 3;
                upLongPressExecuted = true;
            }
        } else if (upState && upPressStartTime) {
            if (!upLongPressExecuted && (currentTime - upPressStartTime) < LONG_PRESS_TIME) {
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
                assistLevelAsText = !assistLevelAsText;
                downLongPressExecuted = true;
            }
        } else if (downState && downPressStartTime) {
            if (!downLongPressExecuted && (currentTime - downPressStartTime) < LONG_PRESS_TIME) {
                if (assistLevel > 0) assistLevel--;
            }
            downPressStartTime = 0;
            downLongPressExecuted = false;
            lastDebounceTime = currentTime;
        }
        
        // Obsługa przycisku SET (dla włączonego wyświetlacza)
        if (!setState && (currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
            if (!setPressStartTime) {
                setPressStartTime = currentTime;
            } else if (!setLongPressExecuted && (currentTime - setPressStartTime) > SET_LONG_PRESS) {
                // Wyłączanie wyświetlacza
                display.clearBuffer();
                display.setFont(u8g2_font_pxplusibmvga9_mf);
                display.drawStr(20, 32, "Do widzenia :)");
                display.sendBuffer();
                messageStartTime = currentTime;
                setLongPressExecuted = true;
                showingWelcome = false;
            }
        } else if (setState && setPressStartTime) {
            if (!setLongPressExecuted && (currentTime - setPressStartTime) < SET_LONG_PRESS) {
                currentDisplay = (DisplayMode)((currentDisplay + 1) % 7);
            }
            setPressStartTime = 0;
            setLongPressExecuted = false;
            lastDebounceTime = currentTime;
        }
    }
    
    // Sprawdzanie czasu wyświetlania komunikatów
    if (messageStartTime > 0 && (currentTime - messageStartTime) >= GOODBYE_DELAY) {
        if (!showingWelcome) {
            // Koniec wyświetlania "Do widzenia" - przejdź do deep sleep
            goToSleep();  // To wywoła funkcję deep sleep zamiast tylko wyłączać wyświetlacz
        }
        // Koniec wyświetlania "Witaj"
        messageStartTime = 0;
        showingWelcome = false;
    }
}

void goToSleep() {
    // Wyłącz wszystkie LEDy
    digitalWrite(FrontDayPin, LOW);
    digitalWrite(FrontPin, LOW);
    digitalWrite(RealPin, LOW);
    
    // Wyłącz OLED
    display.clearBuffer();
    display.sendBuffer();
    display.setPowerSave(1);  // Wprowadź OLED w tryb oszczędzania energii
    
    // Konfiguracja wybudzania przez przycisk SET
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_12, 0);  // GPIO12 (BTN_SET) stan niski
    
    // Wejście w deep sleep
    esp_deep_sleep_start();
}

void setup() {
    // Sprawdź przyczynę wybudzenia
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    Serial.begin(115200);
    
    // Inicjalizacja RTC
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        while (1);
    }

    // Jeśli chcesz ustawić czas, odkomentuj poniższą linię
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    
    // Jeśli RTC stracił zasilanie, ustaw go
    if (rtc.lostPower()) {
        Serial.println("RTC lost power, lets set the time!");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // Konfiguracja pinów
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_SET, INPUT_PULLUP);

    // Konfiguracja pinów LED
    pinMode(FrontDayPin, OUTPUT);
    pinMode(FrontPin, OUTPUT);
    pinMode(RealPin, OUTPUT);
    digitalWrite(FrontDayPin, LOW);
    digitalWrite(FrontPin, LOW);
    digitalWrite(RealPin, LOW);
    
    // Inicjalizacja I2C i wyświetlacza
    Wire.begin(I2C_SDA, I2C_SCL);
    display.begin();
    display.enableUTF8Print();
    display.setFontDirection(0);

    // Wyczyść wyświetlacz na starcie
    display.clearBuffer();
    display.sendBuffer();

    // Jeśli wybudzenie przez przycisk SET, poczekaj na długie naciśnięcie
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        unsigned long startTime = millis();
        while(!digitalRead(BTN_SET)) {  // Czekaj na puszczenie przycisku
            if((millis() - startTime) > SET_LONG_PRESS) {
                displayActive = true;
                showingWelcome = true;
                messageStartTime = millis();
                
                display.clearBuffer();
                display.setFont(u8g2_font_pxplusibmvga9_mf);
                display.drawStr(40, 32, "Witaj!");
                display.sendBuffer();
                
                while(!digitalRead(BTN_SET)) {  // Czekaj na puszczenie przycisku
                    delay(10);
                }
                break;
            }
            delay(10);
        }
    }
}

void loop() {
    static unsigned long lastButtonCheck = 0;
    static unsigned long lastUpdate = 0;
    const unsigned long buttonInterval = 10;
    const unsigned long updateInterval = 2000;

    unsigned long currentTime = millis();

    if (currentTime - lastButtonCheck >= buttonInterval) {
        handleButtons();
        lastButtonCheck = currentTime;
    }

    // Aktualizuj wyświetlacz tylko jeśli jest aktywny i nie wyświetla komunikatów
    if (displayActive && messageStartTime == 0) {
        display.clearBuffer();
        drawTopBar();
        drawHorizontalLine();
        drawVerticalLine();
        drawAssistLevel();
        drawMainDisplay();
        drawLightStatus();
        display.sendBuffer();

        if (currentTime - lastUpdate >= updateInterval) {  
            speed = (speed >= 35.0) ? 0.0 : speed + 0.1;
            tripDistance += 0.1;
            totalDistance += 0.1;
            temperature = 20.0 + random(100) / 10.0;
            power = 100 + random(300);
            energyConsumption += 0.2;
            batteryCapacity = 14.5 - (random(20) / 10.0);
            batteryPercent = (batteryPercent <= 0) ? 100 : batteryPercent - 1;
            batteryVoltage = (batteryVoltage <= 42.0) ? 50.0 : batteryVoltage - 0.1;
            assistMode = (assistMode + 1) % 4;
            lastUpdate = currentTime;
        }
    }
}
