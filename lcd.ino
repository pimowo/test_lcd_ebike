#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Definicje pinów
// Przyciski
#define BTN_UP 13
#define BTN_DOWN 14
#define BTN_SET 12
// Światła
#define FrontDayPin 5  // światła dzienne
#define FrontPin 18    // światła zwykłe
#define RealPin 19     // tylne światło
// Ładowarka USB
#define UsbPin 32      // ładowarka USB
// Czujnik temperatury powietrza
#define PIN_ONE_WIRE_BUS 15  // Pin do którego podłączony jest DS18B20

OneWire oneWire(PIN_ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
unsigned long ds18b20RequestTime = 0;
const unsigned long DS18B20_CONVERSION_DELAY_MS = 750;
float currentTemp = 0.0;

// Dodaj obiekt RTC
RTC_DS3231 rtc;

// Inicjalizacja wyświetlacza
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

// Stałe czasowe dla przycisków
const unsigned long DEBOUNCE_DELAY = 25;
const unsigned long BUTTON_DELAY = 200;
const unsigned long LONG_PRESS_TIME = 1000;

bool displayActive = false;
const unsigned long GOODBYE_DELAY = 5000; // 5 sekund na komunikaty
const unsigned long SET_LONG_PRESS = 3000; // 3 sekundy na długie naciśnięcie SET
unsigned long messageStartTime = 0; // Używane zarówno dla powitania jak i pożegnania
bool showingWelcome = false; // Flaga dla wyświetlania powitania

// Dodaj na początku pliku z innymi zmiennymi globalnymi
bool temperatureReady = false;
unsigned long lastTempRequest = 0;
const unsigned long TEMP_REQUEST_INTERVAL = 1000; // 1 sekunda między pomiarami

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

class TimeoutHandler {
private:
    uint32_t startTime;
    uint32_t timeoutPeriod;
    bool isRunning;

public:
    TimeoutHandler(uint32_t timeout_ms = 0) : 
        startTime(0), 
        timeoutPeriod(timeout_ms), 
        isRunning(false) {}

    void start(uint32_t timeout_ms = 0) {
        if (timeout_ms > 0) timeoutPeriod = timeout_ms;
        startTime = millis();
        isRunning = true;
    }

    bool isExpired() {
        if (!isRunning) return false;
        return (millis() - startTime) >= timeoutPeriod;
    }

    void stop() {
        isRunning = false;
    }

    uint32_t getElapsed() {
        if (!isRunning) return 0;
        return (millis() - startTime);
    }
};

bool isValidTemperature(float temp) {
    return (temp >= -50.0f && temp <= 100.0f);
}

class TemperatureSensor {
private:
    TimeoutHandler conversionTimeout;
    TimeoutHandler readTimeout;
    bool conversionInProgress;

public:
    TemperatureSensor() : 
        conversionTimeout(DS18B20_CONVERSION_DELAY_MS),
        readTimeout(1000),  // 1 sekunda na odczyt
        conversionInProgress(false) {}

    void requestTemperature() {
        if (conversionInProgress) return;
        
        sensors.requestTemperatures();
        conversionTimeout.start();
        conversionInProgress = true;
    }

    bool isReady() {
        if (!conversionInProgress) return false;
        if (conversionTimeout.isExpired()) {
            conversionInProgress = false;
            return true;
        }
        return false;
    }

    float readTemperature() {
        if (!conversionInProgress) return -999.0;

        readTimeout.start();
        float temp = sensors.getTempCByIndex(0);

        if (readTimeout.isExpired()) {
            return -999.0;
        }

        conversionInProgress = false;
        return isValidTemperature(temp) ? temp : -999.0;
    }
};

TemperatureSensor tempSensor;

void temperatureTask(void * parameter) {
    for(;;) {
        if (!tempSensor.isReady()) {
            tempSensor.requestTemperature();
        } else {
            float temp = tempSensor.readTemperature();
            if (temp != -999.0) {
                currentTemp = temp;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Odczyt co 1 sekundę
    }
}

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
        if (currentTemp != -999.0) {
            sprintf(valueStr, "%4.1f", currentTemp);
        } else {
            strcpy(valueStr, "Blad");  // Pokazuj kreski jeśli błąd odczytu
        }
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
    digitalWrite(UsbPin, LOW);
    
    delay(50);

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

    Wire.begin();

    // Inicjalizacja DS18B20
    sensors.begin();
    Serial.println("Inicjalizacja DS18B20...");
    Serial.print("Znaleziono czujników: ");
    Serial.println(sensors.getDeviceCount());
    
    if (sensors.getDeviceCount() == 0) {
        Serial.println("Nie znaleziono czujnika DS18B20!");
    }
    
    sensors.setResolution(12);  // Ustaw najwyższą rozdzielczość
    tempSensor.requestTemperature();  // Pierwsze żądanie pomiaru
    
    
    // Uruchom task temperatury na rdzeniu 0
    xTaskCreatePinnedToCore(
        temperatureTask,    // Funkcja zadania
        "TempTask",        // Nazwa
        2048,              // Rozmiar stosu
        NULL,              // Parametry
        1,                 // Priorytet
        NULL,             // Uchwyt do zadania
        0                  // Rdzeń (0)
    );

    // Inicjalizacja RTC
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        while (1);
    }

    // Jeśli chcesz ustawić czas, odkomentuj poniższą linię
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    // Ustaw aktualny czas (rok, miesiąc, dzień, godzina, minuta, sekunda)
    //rtc.adjust(DateTime(2024, 12, 14, 13, 31, 0)); // Pamiętaj o strefie czasowej (UTC+1)

    
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

    // Ładowarka USB
    pinMode(UsbPin, OUTPUT);
    digitalWrite(UsbPin, LOW);
    
    // Inicjalizacja I2C i wyświetlacza
    //Wire.begin(I2C_SDA, I2C_SCL);
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
    const unsigned long buttonInterval = 5;
    const unsigned long updateInterval = 2000;
    static unsigned long lastTempUpdate = 0;

    unsigned long currentTime = millis();

    if (currentTime - lastButtonCheck >= buttonInterval) {
        handleButtons();
        lastButtonCheck = currentTime;
    }

    // Obsługa czujnika temperatury
    if (!tempSensor.isReady()) {
        tempSensor.requestTemperature();
    } else {
        float temp = tempSensor.readTemperature();
        if (temp != -999.0) {
            currentTemp = temp;
            Serial.print("Temperatura: ");
            Serial.println(currentTemp);
        }
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

/*
GPIO | Input	| Output     | Notes
-----|--------|------------|---------
0	   | PULLUP | OK	       | wysyła sygnał PWM przy rozruchu, musi być NISKI, aby przejść do trybu migania
1	   | TX ESP	| OK	       | debugowanie danych wyjściowych podczas rozruchu
2	   | OK	    | OK	       | podłączony do wbudowanej diody LED, musi pozostać pływający lub NISKI, aby przejść do trybu migania
3	   | OK	    | RX         | pin WYSOKI przy rozruchu
4	   | OK	    | OK	       |
5	   | OK   	| Dzień      | wyprowadza sygnał PWM przy rozruchu, pin spinający
6	   | x	    | x	         | connected to the integrated SPI flash
7	   | x	    | x	         | connected to the integrated SPI flash
8	   | x      | x	         | connected to the integrated SPI flash
9	   | x      | x	         | connected to the integrated SPI flash
10	 | x	    | x	         | connected to the integrated SPI flash
11	 | x	    | x	         | connected to the integrated SPI flash
12	 | SET    | OK	       | boot fails if pulled high, strapping pin
13	 | UP	    | OK	       |
14	 | DOWN   | OK	       | outputs PWM signal at boot
15	 | TEMP   | OK	       | outputs PWM signal at boot, strapping pin
16	 | OK	    | OK         |	
17	 | OK	    | OK         |	
18	 | OK	    | Przód      |
19	 | OK	    | Tył        |	
21	 | SDA    | OK         |	
22	 | OK	    | SCL        |	
23	 | OK	    | OK         |	
25	 | OK	    | OK	       |
26	 | OK	    | OK	       |
27	 | OK	    | OK	       |
32	 | OK	    | USB        |	
33	 | OK	    | OK         |	
34	 | OK		  | input only |  
35	 | OK		  | input only |
36	 | OK		  | input only |
39	 | OK		  | input only |
*/
