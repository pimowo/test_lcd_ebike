# e-Bike System by PMW

## Przegląd
e-Bike System by PMW to zaawansowany system zaprojektowany dla rowerów elektrycznych. Oferuje funkcje takie jak monitorowanie prędkości, pomiar temperatury, zarządzanie baterią, pomiar mocy i wiele innych. System wykorzystuje różne czujniki i komponenty, aby dostarczać dane w czasie rzeczywistym oraz opcje sterowania, które poprawiają doświadczenie jazdy na rowerze elektrycznym.

## Funkcje
- **Monitorowanie prędkości**: Wyświetla aktualną, średnią i maksymalną prędkość.
- **Pomiar temperatury**: Monitoruje temperaturę powietrza, kontrolera i silnika.
- **Obliczanie zasięgu**: Wyświetla szacowany zasięg, przebyty dystans i wartość licznika kilometrów.
- **Zarządzanie baterią**: Pokazuje napięcie, prąd, pojemność i procent naładowania baterii.
- **Pomiar mocy**: Wyświetla aktualną, średnią i maksymalną moc wyjściową.
- **Monitorowanie ciśnienia**: Monitoruje ciśnienie w oponach, napięcie i temperaturę.
- **Sterowanie USB**: Kontroluje port ładowania USB.
- **Sterowanie światłami**: Zarządza przednimi i tylnymi światłami z trybami dziennym i nocnym.
- **Interfejs użytkownika**: Interaktywny wyświetlacz OLED z wieloma ekranami i pod-ekranami dla szczegółowych informacji.

## Komponenty
- **Mikrokontroler**: ESP32
- **Wyświetlacz**: SSD1306 128x64 OLED
- **Czujnik temperatury**: DS18B20
- **Zegar czasu rzeczywistego (RTC)**: DS3231
- **Bluetooth**: BLE do komunikacji z systemem zarządzania baterią (BMS)
- **EEPROM**: Do przechowywania ustawień użytkownika

## Definicje pinów
- **Przyciski**:
  - `BTN_UP`: GPIO 13
  - `BTN_DOWN`: GPIO 14
  - `BTN_SET`: GPIO 12
- **Światła**:
  - `FrontDayPin`: GPIO 5
  - `FrontPin`: GPIO 18
  - `RealPin`: GPIO 19
- **Ładowarka USB**:
  - `UsbPin`: GPIO 32
- **Czujnik temperatury**:
  - `ONE_WIRE_BUS`: GPIO 15

## Użytkowanie
1. **Instalacja**:
    - Podłącz wszystkie komponenty zgodnie z definicjami pinów.
    - Wgraj dostarczony kod do mikrokontrolera ESP32.

2. **Obsługa**:
    - Użyj przycisku `BTN_SET` do nawigacji między głównymi ekranami.
    - Użyj przycisków `BTN_UP` i `BTN_DOWN` do zmiany ustawień lub przełączania między pod-ekranami.
    - Długie naciśnięcie `BTN_SET` włącza/wyłącza wyświetlacz.
    - Podwójne kliknięcie `BTN_SET` przełącza wyjście USB lub wchodzi do pod-ekranów.

3. **Konfiguracja**:
    - Ustawienia systemu, takie jak obwód koła i pojemność baterii, są przechowywane w EEPROM.
    - Wyświetlacz pokazuje różne ekrany dotyczące prędkości, temperatury, zasięgu, baterii, mocy i ciśnienia.

## Struktura kodu
- **Biblioteki**:
  - `Wire.h`, `U8g2lib.h`, `RTClib.h`, `OneWire.h`, `DallasTemperature.h`, `EEPROM.h`, `BLEDevice.h`, `BLEClient.h`, `BLEUtils.h`, `BLEServer.h`
- **Główny kod**:
  - `setup()`: Inicjalizuje komponenty i ustawienia.
  - `loop()`: Obsługuje wejścia przycisków, aktualizuje wyświetlacz i przetwarza dane z czujników.
- **Funkcje**:
  - `connectToBms()`: Łączy się z systemem zarządzania baterią przez BLE.
  - `loadSettingsFromEEPROM()`: Ładuje ustawienia użytkownika z EEPROM.
  - `saveSettingsToEEPROM()`: Zapisuje ustawienia użytkownika do EEPROM.
  - `drawMainDisplay()`, `drawTopBar()`, `drawAssistLevel()`, `drawLightStatus()`, `drawValueAndUnit()`: Funkcje do aktualizacji wyświetlacza OLED.
  - `handleTemperature()`: Zarządza odczytami z czujnika temperatury.

## Licencja
Projekt jest licencjonowany na podstawie licencji MIT. Zobacz plik [LICENSE](LICENSE) dla szczegółów.

## Wkład
Wkłady są mile widziane! Proszę rozwidlić repozytorium i złożyć pull request na wszelkie ulepszenia lub poprawki błędów.

## Kontakt
W razie pytań lub wsparcia prosimy o kontakt na adres [Twoje Imię] pod adresem [twoj.email@przyklad.com].
