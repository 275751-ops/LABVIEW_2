# Dokumentacja Projektu Rozproszony System Pomiarowy

Niniejszy projekt przedstawia rozproszony system end-to-end przeznaczony do zbierania, przetwarzania, składowania oraz udostępniania danych pomiarowych z urządzeń brzegowych (ESP32). Całość infrastruktury serwerowej została w pełni zaprojektowana przy użyciu Dockera.

---

## 1. Architektura Systemu i Przepływ Danych

System składa się z czterech głównych warstw:
1. **Warstwa Brzegowa (Firmware):** Układ ESP32 odczytuje dane z czujnika (BMP280/BME280), synchronizuje czas przez NTP i wysyła paczki danych JSON przez bezpieczny protokół MQTT (TLS).
2. **Warstwa Komunikacyjna (Broker):** Serwer Eclipse Mosquitto pośredniczy w wymianie wiadomości na porcie szyfrowanym 8883.
3. **Warstwa Backendowa (Ingestor i Baza danych):** Usługa skryptowa w Pythonie subskrybuje dane z brokera, waliduje je strukturalnie i zapisuje do relacyjnej bazy danych PostgreSQL.
4. **Warstwa Dostępu (REST API):** Aplikacja we Flasku wystawia zabezpieczone endpointy, umożliwiając zewnętrznym klientom (np. aplikacjom w LabVIEW) pobieranie historii oraz najświeższych pomiarów.

---

## 2. Kontrakt Danych i Struktura Topiców MQTT

Aby zapewnić bezproblemową integrację komponentów systemu, wprowadzono ścisły podział kanałów (topików) oraz ujednolicony format wiadomości pomiarowej.

### 2.1. Projekt Topików (Topic Design)
Komunikacja realizowana jest w oparciu o następujące wzorce topików:

* **Pomiary:** `lab/{group_id}/{device_id}/{sensor_type}`
* **Status urządzenia (LWT):** `lab/{group_id}/{device_id}/status`

*Gdzie:*
* `group_id` – identyfikator grupy laboratoryjnej (np. `g01`, `g03`).
* `device_id` – unikalny identyfikator sprzętowy generowany na podstawie adresu MAC pamięci eFuse układu ESP32 (np. `esp32-A1B2C3D4E5F6`).
* `sensor_type` – typ przesyłanej wartości fizycznej (`temperature`, `pressure`, `Altitude`).

### 2.2. Format Wiadomości Pomiarowej (JSON v1)
Wszystkie wiadomości pomiarowe publikowane przez urządzenia brzegowe muszą być zgodne z poniższym schematem strukturalnym:

```json
{
  "schema_version": 1,
  "device_id": "esp32-ID",
  "group_id": "g03",
  "sensor": "temperature",
  "value": 24.5,
  "unit": "C",
  "ts_ms": 1742030400000
}
```
# Specyfikacja pól schematu JSON

| Pole | Typ danych | Wymagane? | Opis | Przykład |
|--------|------------|------------|--------|------------|
| `schema_version` | Integer | Nie | Wersja struktury kontraktu danych. | `1` |
| `device_id` | String | Tak | Unikalne ID urządzenia pobrane z eFuse. | `"esp32-B4F1"` |
| `group_id` | String | Tak | Identyfikator zespołu projektowego. | `"g03"` |
| `sensor` | String | Tak | Typ mierzonej wielkości (np. kanał pomiaru). | `"pressure"` |
| `value` | Float | Tak | Fizyczna wartość liczbowa odczytana z czujnika. | `1011.25` |
| `unit` | String | Nie | Jednostka miary powiązana z sensorem. | `"hPa"` |
| `ts_ms` | Long Long | Tak | Znacznik czasu w milisekundach (epoch time UTC). | `1742030400000` |

# 3. Specyfikacja Urządzenia Brzegowego (ESP32 Firmware)

Firmware został zaimplementowany w frameworku Arduino (PlatformIO). Główne cechy algorytmu to:

## 3.1. Synchronizacja Czasu i Bezpieczeństwo

### Precyzyjny czas UTC

Po połączeniu z siecią Wi-Fi urządzenie odpytuje serwery Głównego Urzędu Miar (`tempus1.gum.gov.pl` oraz `tempus2.gum.gov.pl`) przez protokół NTP. Znacznik czasu jest wymagany do poprawnego budowania paczek pomiarowych na poziomie mikrokontrolera.

### Szyfrowanie TLS

Wykorzystywana jest klasa `WiFiClientSecure`. Firmware posiada wbudowany certyfikat CA (`ca_cert`) pozwalający na autentykację i weryfikację tożsamości brokera MQTT.

## 3.2. Mechanizmy Niezawodnościowe (Resilience)

### Asynchroniczny Reconnect (Non-blocking)

Ponowne próby nawiązania połączenia z Wi-Fi (`WIFI_RETRY_MS = 5000`) oraz brokerem MQTT (`MQTT_RETRY_MS = 3000`) realizowane są bez użycia funkcji blokujących `delay()` w pętli głównej. Zapewnia to ciągłość pracy programu.

### Last Will and Testament (LWT)

Podczas zestawiania sesji MQTT ESP32 rejestruje u brokera swoją „ostatnią wolę”. Jeśli urządzenie niespodziewanie straci zasilanie lub zasięg sieci, broker automatycznie opublikuje na topiku statusowym wiadomość o stanie `offline`.

Po udanym połączeniu urządzenie wysyła status `online` jako wiadomość zachowaną (*retained*).

# 4. Ekosystem Serwerowy (Backend w Dockerze)

Cała infrastruktura serwerowa uruchamiana jest jedną komendą dzięki definicji wielousługowej w pliku `docker-compose.yml`.

## docker-compose.yml

```yaml
services:
  flask:
    build: ./api
    ports:
      - 5001:5001
    image: api:v1
    container_name: api
    networks:
      - backend

  broker:
    build: ./broker
    ports:
      - 8883:8883
    image: broker:v1
    container_name: broker
    networks:
      - backend

  database:
    build: ./database
    ports:
      - 5432:5432
    image: database:v1
    container_name: postgres
    networks:
      - backend

  ingestor:
    build: ./ingestor
    container_name: ingestor
    depends_on:
      - broker
      - database
    networks:
      - backend
    volumes:
      - ./broker/certs:/app/certs:ro

networks:
  backend:
    driver: bridge
```

## 4.1. Ingestor MQTT (`ingestor.py`)

Skrypt nasłuchuje na globalnym topiku z użyciem wildcardów:

```text
lab/+/+/+
```

### Walidacja struktury

Każda odebrana wiadomość przechodzi test funkcją `is_valid()`, która weryfikuje istnienie kluczowych parametrów:

- `device_id`
- `sensor`
- `value`
- `ts_ms`

### Trwałość danych

Dane niespełniające kryteriów kontraktu są odrzucane i logowane w konsoli jako błąd. Prawidłowe rekordy trafiają bezpośrednio do tabeli `measurements` w bazie PostgreSQL.

## 4.2. Flask REST API (`app.py`)

Serwis API wystawia dane pomiarowe na porcie `5001`.

Większość endpointów (poza ogólnodostępnymi informacjami o stanie) chroniona jest dedykowanym dekoratorem bezpieczeństwa `@auth_required` i hasłem zadaklarowanym w pliku `auth.py`.

### Specyfikacja endpointów API

| Metoda HTTP | Ścieżka (URI) | Autoryzacja | Opis funkcjonalny |
|------------|--------------|-------------|-------------------|
| GET | `/` | Brak | Powitalny punkt API (*Hello World*). |
| GET | `/health` | Brak | Odpowiedź o stanie kontenera (*Healthcheck*). |
| GET | `/measurements` | Wymagana | Pobiera 20 ostatnich pomiarów zapisanych w bazie. |
| GET | `/measurements/latest` | Wymagana | Zwraca bezwzględnie ostatni zarejestrowany rekord. |
| GET | `/measurements/latest/<sensor>` | Wymagana | Zwraca ostatni pomiar dla konkretnego sensora (np. `/latest/temperature`). |
| GET | `/measurements/history` | Wymagana | Zaawansowane filtrowanie po `device_id` i `sensor` z obsługą limitu wyników (`?limit=X`). |

# 5. Instrukcja Uruchomienia i Testowania

## 5.1. Uruchomienie infrastruktury

W celu zbudowania obrazów oraz jednoczesnego uruchomienia wszystkich kontenerów w tle wykonaj polecenie w katalogu głównym:

```bash
docker compose up --build -d
```

## 5.2. Weryfikacja działania API

Możesz przetestować poprawność działania punktu zdrowia (*healthcheck*) za pomocą narzędzia `curl` lub przeglądarki internetowej:

```bash
curl http://localhost:5001/health
```

### Oczekiwany rezultat

```json
{
  "status": "ok"
}
```