#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// Deklaracja brakujących zmiennych
struct Settings {
  int wheelCircumference;
  float batteryCapacity;
  int daySetting;
  int nightSetting;
  bool dayRearBlink;
  bool nightRearBlink;
  unsigned long blinkInterval;
};

// Typ głównych ekranów
enum MainScreen {
    SPEED_SCREEN,    // Ekran a
    TEMP_SCREEN,     // Ekran b
    RANGE_SCREEN,    // Ekran c
    BATTERY_SCREEN,  // Ekran d
    POWER_SCREEN,    // Ekran e
    PRESSURE_SCREEN, // Ekran f
    MAIN_SCREEN_COUNT
};

// Typy pod-ekranów dla każdego głównego ekranu
enum SpeedSubScreen {
    SPEED_KMH,
    CADENCE_RPM,
    SPEED_SUB_COUNT
};

enum TempSubScreen {
    TEMP_AIR,
    TEMP_CONTROLLER,
    TEMP_MOTOR,
    TEMP_SUB_COUNT
};

enum RangeSubScreen {
    RANGE_KM,
    DISTANCE_KM,
    ODOMETER_KM,
    RANGE_SUB_COUNT
};

enum BatterySubScreen {
    BATTERY_VOLTAGE,
    BATTERY_CURRENT,
    BATTERY_CAPACITY_WH,
    BATTERY_CAPACITY_AH,
    BATTERY_CAPACITY_PERCENT,
    BATTERY_SUB_COUNT
};

enum PowerSubScreen {
    POWER_W,
    POWER_AVG_W,
    POWER_MAX_W,
    POWER_SUB_COUNT
};

enum PressureSubScreen {
    PRESSURE_BAR,
    PRESSURE_VOLTAGE,
    PRESSURE_TEMP,
    PRESSURE_SUB_COUNT
};

MainScreen currentMainScreen = SPEED_SCREEN;
int currentSubScreen = 0;
bool inSubScreen = false;

// Zmienne do przechowywania danych pomiarowych
float speed_kmh = 0;
int cadence_rpm = 0;
float temp_air = 0;
float temp_controller = 0;
float temp_motor = 0;
float range_km = 0;
float distance_km = 0;
float odometer_km = 0;
float battery_voltage = 0;
float battery_current = 0;
float battery_capacity_wh = 0;
float battery_capacity_ah = 0;
int battery_capacity_percent = 0;
int power_w = 0;
int power_avg_w = 0;
int power_max_w = 0;
float pressure_bar = 0;
float pressure_voltage = 0;
float pressure_temp = 0;

// Zmienne dla obsługi podwójnego kliknięcia
const unsigned long DOUBLE_CLICK_TIME = 250;  // Maksymalny czas między kliknięciami (250ms)
unsigned long lastClickTime = 0;              // Czas ostatniego kliknięcia
bool firstClick = false;                      // Flaga pierwszego kliknięcia

Settings bikeSettings;
Settings storedSettings;

BLEClient* bleClient;
BLEAddress bmsMacAddress("a5:c2:37:05:8b:86");
BLERemoteService* bleService;
BLERemoteCharacteristic* bleCharacteristicTx;
BLERemoteCharacteristic* bleCharacteristicRx;

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
#define ONE_WIRE_BUS 15  // Pin do którego podłączony jest DS18B20

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
unsigned long lastTempRequest = 0;
const unsigned long TEMP_REQUEST_INTERVAL = 1000;
float currentTemp = DEVICE_DISCONNECTED_C; // Początkowa wartość
bool conversionRequested = false;
#define TEMP_ERROR -999.0
const unsigned long DS18B20_CONVERSION_DELAY_MS = 750;  // Czas konwersji DS18B20
unsigned long ds18b20RequestTime;

int subScreen = 0;
int currentScreen = 0;
bool longPressHandled = false;
const int NUM_SCREENS = 7; // Liczba głównych ekranów, dostosuj do swoich potrzeb

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

bool isValidTemperature(float temp);

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

// int getSubScreenCount(int screen) {
//     switch (screen) {
//         case 0: return 2; // Przykład: ekran 0 ma 2 pod-ekrany
//         case 1: return 3; // Przykład: ekran 1 ma 3 pod-ekrany
//         // Dodaj pozostałe ekrany i ich liczby pod-ekranów
//         default: return 0;
//     }
// }

void notificationCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  // Twoja funkcja obsługi powiadomień
}

// --- Wczytywanie ustawień z EEPROM ---
void loadSettingsFromEEPROM() {
  // Wczytanie ustawień z EEPROM
  EEPROM.get(0, bikeSettings);

  // Skopiowanie aktualnych ustawień do storedSettings do późniejszego porównania
  storedSettings = bikeSettings;

  // Możesz dodać weryfikację wczytanych danych
  if (bikeSettings.wheelCircumference == 0) {
    bikeSettings.wheelCircumference = 2210;  // Domyślny obwód koła
    bikeSettings.batteryCapacity = 10.0;     // Domyślna pojemność baterii
    bikeSettings.daySetting = 0;
    bikeSettings.nightSetting = 0;
    bikeSettings.dayRearBlink = false;
    bikeSettings.nightRearBlink = false;
    bikeSettings.blinkInterval = 500;
  }
}

// --- Funkcja zapisująca ustawienia do EEPROM ---
void saveSettingsToEEPROM() {
  // Porównaj aktualne ustawienia z poprzednio wczytanymi
  if (memcmp(&storedSettings, &bikeSettings, sizeof(bikeSettings)) != 0) {
    // Jeśli ustawienia się zmieniły, zapisz je do EEPROM
    EEPROM.put(0, bikeSettings);
    EEPROM.commit();

    // Zaktualizuj storedSettings po zapisie
    storedSettings = bikeSettings;
  }
}

// --- Połączenie z BMS ---
void connectToBms() {
  if (!bleClient->isConnected()) {
    #if DEBUG
    Serial.println("Próba połączenia z BMS...");
    #endif

    if (bleClient->connect(bmsMacAddress)) {
      #if DEBUG
      Serial.println("Połączono z BMS");
      #endif

      bleService = bleClient->getService("0000ff00-0000-1000-8000-00805f9b34fb");
      if (bleService == nullptr) {
        #if DEBUG
        Serial.println("Nie znaleziono usługi BMS");
        #endif
        bleClient->disconnect();
        return;
      }

      bleCharacteristicTx = bleService->getCharacteristic("0000ff02-0000-1000-8000-00805f9b34fb");
      if (bleCharacteristicTx == nullptr) {
        #if DEBUG
        Serial.println("Nie znaleziono charakterystyki Tx");
        #endif
        bleClient->disconnect();
        return;
      }

      bleCharacteristicRx = bleService->getCharacteristic("0000ff01-0000-1000-8000-00805f9b34fb");
      if (bleCharacteristicRx == nullptr) {
        #if DEBUG
        Serial.println("Nie znaleziono charakterystyki Rx");
        #endif
        bleClient->disconnect();
        return;
      }

      // Rejestracja funkcji obsługi powiadomień BLE
      if (bleCharacteristicRx->canNotify()) {
        bleCharacteristicRx->registerForNotify(notificationCallback);
        #if DEBUG
        Serial.println("Zarejestrowano powiadomienia dla Rx");
        #endif
      } else {
        #if DEBUG
        Serial.println("Charakterystyka Rx nie obsługuje powiadomień");
        #endif
        bleClient->disconnect();
        return;
      }

    } else {
      #if DEBUG
      Serial.println("Nie udało się połączyć z BMS");
      #endif
    }
  }
}

void setLights() {
    if (lightMode == 0) {
        digitalWrite(FrontDayPin, LOW);
        digitalWrite(FrontPin, LOW);
        digitalWrite(RealPin, LOW);
    } else if (lightMode == 1) {
        digitalWrite(FrontDayPin, HIGH);
        digitalWrite(FrontPin, LOW);
        digitalWrite(RealPin, HIGH);
    } else if (lightMode == 2) {
        digitalWrite(FrontDayPin, LOW);
        digitalWrite(FrontPin, HIGH);
        digitalWrite(RealPin, HIGH);
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

void drawValueAndUnit(const char* valueStr, const char* unitStr) {
    int valueWidth = display.getStrWidth(valueStr);
    display.drawStr(128 - valueWidth, 43, valueStr); // Bez dodatkowego marginesu

    display.setFont(u8g2_font_profont11_tr);
    int unitWidth = display.getStrWidth(unitStr);
    display.drawStr(128 - unitWidth, 53, unitStr);
}

void drawMainDisplay() {
    display.setFont(u8g2_font_logisoso20_tf);
    char valueStr[10];
    const char* unitStr;
    const char* descText;

    if (inSubScreen) {
        // Wyświetlanie pod-ekranów
        switch (currentMainScreen) {
            case SPEED_SCREEN:
                switch (currentSubScreen) {
                    case SPEED_KMH:
                        sprintf(valueStr, "%4.1f", speed_kmh);
                        unitStr = "km/h";
                        descText = "Predkosc";
                        break;
                    case CADENCE_RPM:
                        sprintf(valueStr, "%4d", cadence_rpm);
                        unitStr = "RPM";
                        descText = "Kadencja";
                        break;
                }
                break;

            case TEMP_SCREEN:
                switch (currentSubScreen) {
                    case TEMP_AIR:
                        if (currentTemp != TEMP_ERROR && currentTemp != DEVICE_DISCONNECTED_C) {
                            sprintf(valueStr, "%4.1f", currentTemp);
                        } else {
                            strcpy(valueStr, "---");
                        }
                        unitStr = "°C";
                        descText = "Powietrze";
                        break;
                    case TEMP_CONTROLLER:
                        sprintf(valueStr, "%4.1f", temp_controller);
                        unitStr = "°C";
                        descText = "Sterownik";
                        break;
                    case TEMP_MOTOR:
                        sprintf(valueStr, "%4.1f", temp_motor);
                        unitStr = "°C";
                        descText = "Silnik";
                        break;
                }
                break;

            case RANGE_SCREEN:
                switch (currentSubScreen) {
                    case RANGE_KM:
                        sprintf(valueStr, "%4.1f", range_km);
                        unitStr = "km";
                        descText = "Zasieg";
                        break;
                    case DISTANCE_KM:
                        sprintf(valueStr, "%4.1f", tripDistance);
                        unitStr = "km";
                        descText = "Dystans";
                        break;
                    case ODOMETER_KM:
                        sprintf(valueStr, "%4.0f", totalDistance);
                        unitStr = "km";
                        descText = "Przebieg";
                        break;
                }
                break;

            case BATTERY_SCREEN:
                switch (currentSubScreen) {
                    case BATTERY_VOLTAGE:
                        sprintf(valueStr, "%4.1f", batteryVoltage);
                        unitStr = "V";
                        descText = "Napiecie";
                        break;
                    case BATTERY_CURRENT:
                        sprintf(valueStr, "%4.1f", battery_current);
                        unitStr = "A";
                        descText = "Prad";
                        break;
                    case BATTERY_CAPACITY_WH:
                        sprintf(valueStr, "%4.0f", battery_capacity_wh);
                        unitStr = "Wh";
                        descText = "Pojemnosc";
                        break;
                    case BATTERY_CAPACITY_AH:
                        sprintf(valueStr, "%4.1f", batteryCapacity);
                        unitStr = "Ah";
                        descText = "Pojemnosc";
                        break;
                    case BATTERY_CAPACITY_PERCENT:
                        sprintf(valueStr, "%3d", batteryPercent);
                        unitStr = "%";
                        descText = "Bateria";
                        break;
                }
                break;

            case POWER_SCREEN:
                switch (currentSubScreen) {
                    case POWER_W:
                        sprintf(valueStr, "%4d", power);
                        unitStr = "W";
                        descText = "Moc";
                        break;
                    case POWER_AVG_W:
                        sprintf(valueStr, "%4d", power_avg_w);
                        unitStr = "W";
                        descText = "Moc AVG";
                        break;
                    case POWER_MAX_W:
                        sprintf(valueStr, "%4d", power_max_w);
                        unitStr = "W";
                        descText = "Moc MAX";
                        break;
                }
                break;

            case PRESSURE_SCREEN:
                switch (currentSubScreen) {
                    case PRESSURE_BAR:
                        sprintf(valueStr, "%4.1f", pressure_bar);
                        unitStr = "bar";
                        descText = "Cisnienie";
                        break;
                    case PRESSURE_VOLTAGE:
                        sprintf(valueStr, "%4.2f", pressure_voltage);
                        unitStr = "V";
                        descText = "Napiecie";
                        break;
                    case PRESSURE_TEMP:
                        sprintf(valueStr, "%4.1f", pressure_temp);
                        unitStr = "°C";
                        descText = "Temperatura";
                        break;
                }
                break;
        }
    } else {
        // Wyświetlanie głównych ekranów
        switch (currentMainScreen) {
            case SPEED_SCREEN:
                sprintf(valueStr, "%4.1f", speed_kmh);
                unitStr = "km/h";
                descText = "Predkosc";
                break;
            case TEMP_SCREEN:
                if (currentTemp != TEMP_ERROR && currentTemp != DEVICE_DISCONNECTED_C) {
                    sprintf(valueStr, "%4.1f", currentTemp);
                } else {
                    strcpy(valueStr, "---");
                }
                unitStr = "°C";
                descText = "Temperatura";
                break;
            case RANGE_SCREEN:
                sprintf(valueStr, "%4.1f", range_km);
                unitStr = "km";
                descText = "Zasieg";
                break;
            case BATTERY_SCREEN:
                sprintf(valueStr, "%3d", batteryPercent);
                unitStr = "%";
                descText = "Bateria";
                break;
            case POWER_SCREEN:
                sprintf(valueStr, "%4d", power);
                unitStr = "W";
                descText = "Moc";
                break;
            case PRESSURE_SCREEN:
                sprintf(valueStr, "%4.1f", pressure_bar);
                unitStr = "bar";
                descText = "Cisnienie";
                break;
        }
    }

    drawValueAndUnit(valueStr, unitStr);
    display.setFont(u8g2_font_profont11_tr);
    display.drawStr(52, 62, descText);
}

void handleButtons() {
    unsigned long currentTime = millis();
    bool setState = digitalRead(BTN_SET);
    bool upState = digitalRead(BTN_UP);
    bool downState = digitalRead(BTN_DOWN);

    // Obsługa włączania/wyłączania wyświetlacza
    if (!displayActive) {
        if (!setState && (currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
            if (!setPressStartTime) {
                setPressStartTime = currentTime;
            } else if (!setLongPressExecuted && (currentTime - setPressStartTime) > SET_LONG_PRESS) {
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
        return;
    }

    // Obsługa przycisków gdy wyświetlacz jest aktywny
    if (!showingWelcome) {
        // Obsługa przycisku UP (zmiana asysty)
        if (!upState && (currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
            if (!upPressStartTime) {
                upPressStartTime = currentTime;
            } else if (!upLongPressExecuted && (currentTime - upPressStartTime) > LONG_PRESS_TIME) {
                lightMode = (lightMode + 1) % 3;
                setLights();
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

        // Obsługa przycisku DOWN (zmiana asysty)
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

        // Obsługa przycisku SET (nawigacja po menu)
        if (!setState && (currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
            if (!setPressStartTime) {
                setPressStartTime = currentTime;
            } else if (!setLongPressExecuted && (currentTime - setPressStartTime) > SET_LONG_PRESS) {
                // Bardzo długie naciśnięcie SET (>3s) - wyłączenie wyświetlacza
                display.clearBuffer();
                display.setFont(u8g2_font_pxplusibmvga9_mf);
                display.drawStr(20, 32, "Do widzenia :)");
                display.sendBuffer();
                messageStartTime = currentTime;
                setLongPressExecuted = true;
            }
        } else if (setState && setPressStartTime) {
            // Puszczenie przycisku SET
            unsigned long pressDuration = currentTime - setPressStartTime;
            
            if (!setLongPressExecuted && pressDuration < LONG_PRESS_TIME) {
                // Krótkie naciśnięcie - obsługa pojedynczego/podwójnego kliknięcia
                if (firstClick && (currentTime - lastClickTime) < DOUBLE_CLICK_TIME) {
                    // Podwójne kliknięcie - wejście/wyjście z pod-ekranów
                    if (inSubScreen) {
                        inSubScreen = false;
                    } else if (hasSubScreens(currentMainScreen)) {
                        inSubScreen = true;
                        currentSubScreen = 0;
                    }
                    firstClick = false;
                } else {
                    // Pierwsze kliknięcie lub po czasie na podwójne
                    if (!firstClick) {
                        firstClick = true;
                        lastClickTime = currentTime;
                    } else if ((currentTime - lastClickTime) >= DOUBLE_CLICK_TIME) {
                        // Pojedyncze kliknięcie - przełączanie ekranów
                        if (inSubScreen) {
                            currentSubScreen = (currentSubScreen + 1) % getSubScreenCount(currentMainScreen);
                        } else {
                            currentMainScreen = (MainScreen)((currentMainScreen + 1) % MAIN_SCREEN_COUNT);
                        }
                        firstClick = false;
                    }
                }
            }
            setPressStartTime = 0;
            setLongPressExecuted = false;
            lastDebounceTime = currentTime;
        }
    }

    // Reset flagi pierwszego kliknięcia po przekroczeniu czasu
    if (firstClick && (currentTime - lastClickTime) >= DOUBLE_CLICK_TIME) {
        firstClick = false;
    }

    // Obsługa komunikatów powitalnych/pożegnalnych
    if (messageStartTime > 0 && (currentTime - messageStartTime) >= GOODBYE_DELAY) {
        if (!showingWelcome) {
            displayActive = false;
            goToSleep();
        }
        messageStartTime = 0;
        showingWelcome = false;
    }
}

// Funkcja pomocnicza sprawdzająca czy ekran ma pod-ekrany
bool hasSubScreens(MainScreen screen) {
    switch (screen) {
        case SPEED_SCREEN: return SPEED_SUB_COUNT > 1;
        case TEMP_SCREEN: return TEMP_SUB_COUNT > 1;
        case RANGE_SCREEN: return RANGE_SUB_COUNT > 1;
        case BATTERY_SCREEN: return BATTERY_SUB_COUNT > 1;
        case POWER_SCREEN: return POWER_SUB_COUNT > 1;
        case PRESSURE_SCREEN: return PRESSURE_SUB_COUNT > 1;
        default: return false;
    }
}

// Funkcja pomocnicza zwracająca liczbę pod-ekranów dla danego ekranu
int getSubScreenCount(MainScreen screen) {
    switch (screen) {
        case SPEED_SCREEN: return SPEED_SUB_COUNT;
        case TEMP_SCREEN: return TEMP_SUB_COUNT;
        case RANGE_SCREEN: return RANGE_SUB_COUNT;
        case BATTERY_SCREEN: return BATTERY_SUB_COUNT;
        case POWER_SCREEN: return POWER_SUB_COUNT;
        case PRESSURE_SCREEN: return PRESSURE_SUB_COUNT;
        default: return 0;
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

void initializeDS18B20() {
    sensors.begin();
}

void requestGroundTemperature() {
    sensors.requestTemperatures();
    ds18b20RequestTime = millis();
}

bool isGroundTemperatureReady() {
    return millis() - ds18b20RequestTime >= DS18B20_CONVERSION_DELAY_MS;
}

bool isValidTemperature(float temp) {
    return (temp >= -50.0 && temp <= 100.0);
}

float readGroundTemperature() {
    if (isGroundTemperatureReady()) {
        float temperature = sensors.getTempCByIndex(0);
        if (isValidTemperature(temperature)) {
            return temperature;
        } else {
            return -999.0;
        }
    }
    return -999.0;
}

void handleTemperature() {
    unsigned long currentMillis = millis();
    
    if (!conversionRequested && (currentMillis - lastTempRequest >= TEMP_REQUEST_INTERVAL)) {
        sensors.requestTemperatures();
        conversionRequested = true;
        lastTempRequest = currentMillis;
    }
    
    if (conversionRequested && (currentMillis - lastTempRequest >= 750)) {
        currentTemp = sensors.getTempCByIndex(0);
        conversionRequested = false;
    }
}

void setup() {
    // Sprawdź przyczynę wybudzenia
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    Serial.begin(115200);

    Wire.begin();

    // Inicjalizacja DS18B20
    initializeDS18B20();
    sensors.setWaitForConversion(false);  // Ważne - tryb nieblokujący
    
    sensors.setResolution(12);  // Ustaw najwyższą rozdzielczość
    tempSensor.requestTemperature();  // Pierwsze żądanie pomiaru
    
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
    setLights();

    // Ładowarka USB
    pinMode(UsbPin, OUTPUT);
    digitalWrite(UsbPin, LOW);
    
    // Inicjalizacja I2C i wyświetlacza
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
        handleTemperature();

        if (currentTime - lastUpdate >= updateInterval) { 
              speed_kmh = (speed_kmh >= 35.0) ? 0.0 : speed_kmh + 0.1;
              cadence_rpm = random(60, 90);
              temp_controller = 25.0 + random(15);
              temp_motor = 30.0 + random(20);
              range_km = 50.0 - (random(20) / 10.0);
              tripDistance += 0.1;
              totalDistance += 0.1;
              power = 100 + random(300);
              power_avg_w = power * 0.8;
              power_max_w = power * 1.2;
              battery_current = random(50, 150) / 10.0;
              battery_capacity_wh = batteryCapacity * batteryVoltage;
              pressure_bar = 2.0 + (random(20) / 10.0);
              pressure_voltage = 0.5 + (random(20) / 100.0);
              pressure_temp = 20.0 + (random(100) / 10.0); 
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
