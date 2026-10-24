# Shake to Travel

An interactive IoT country picker connecting an Arduino Oplà board, a Go backend with embedded web dashboard, and a local SQLite database.

---

## Overview

* **Arduino Oplà:** Player chooses a continent (Europe, Asia, Africa), shakes the board to fetch a random country over HTTPS from the Mercantec API, and views the result on the circular screen. Touching `TOUCH 0` saves the country to the computer.
* **Go Backend & Web Dashboard:** A single lightweight server listening on port 8080. It handles REST API endpoints (`/api/countries`), validates country codes, repairs broken UTF-8 text, stores records in SQLite, and serves an embedded dark-themed web dashboard with live auto-refresh.
* **SQLite Database:** A local file (`countries.db`) storing saved destinations with timestamps.

---

## Project Structure

```
├── arduino/
│   ├── arduino.ino            # Main Arduino sketch (C++)
│   ├── config.h               # Server IP, port, and shake thresholds
│   ├── countries.h            # Pre-validated lists of country names
│   ├── arduino_secrets.h.example # Wi-Fi credentials template
│   └── arduino_secrets.h      # Your actual Wi-Fi credentials (git-ignored)
├── backend/
│   ├── main.go                # Server entry point and graceful shutdown
│   ├── server.go              # HTTP routes and embedded static file server
│   ├── db.go                  # SQLite schema and queries
│   ├── config.go              # Environment variable loader
│   ├── textfix.go             # UTF-8 encoding repair function
│   ├── server_test.go         # Backend unit tests
│   └── web/                   # Embedded dashboard UI
│       ├── index.html         # Semantic dashboard markup
│       ├── style.css          # Dark-themed responsive layout
│       └── app.js             # Live polling and search filters
├── tools/gencountries/
│   └── main.go                # Helper tool to generate countries.h
└── Makefile                   # Build and test shortcuts
```

---

## Requirements

* **Go 1.22+**
* **Arduino IDE 2.x** with `WiFiNINA` and `Arduino_MKRIoTCarrier` libraries
* **Arduino Oplà IoT Kit** (MKR WiFi 1010 + MKR IoT Carrier)

---

## Quick Start

Start the server and dashboard:

```bash
make run
```
*(or `go run ./backend`)*

Open your browser at **`http://localhost:8080`**. Press `Ctrl+C` in the terminal to stop.

---

## Step-by-Step Setup

### 1. Arduino SSL Certificate (one-time setup)
The board needs to trust the external API (`api-opgaver.mercantec.tech`):
1. Plug the MKR WiFi 1010 into your PC via USB.
2. In Arduino IDE, open **Tools > Firmware Updater**.
3. Under **SSL root certificates**, add `api-opgaver.mercantec.tech:443`.
4. Click **Upload Certificates to WiFi module**.

### 2. Configure and Upload Firmware
1. Copy `arduino/arduino_secrets.h.example` to `arduino/arduino_secrets.h` and enter your Wi-Fi name and password.
2. Find your computer's local IP address:
   ```bash
   ipconfig getifaddr en0
   ```
3. Set `BACKEND_HOST` in `arduino/config.h` to your computer's IP address.
4. Open `arduino/arduino.ino` in Arduino IDE and click **Upload**.

### 3. Run Backend Server
```bash
make run
```

---

## API Reference

### Save a Country
```bash
curl -X POST http://localhost:8080/api/countries \
  -H "Content-Type: application/json" \
  -d '{
    "apiName": "Mercantec Countries API",
    "choice": "Sweden",
    "cca3": "SWE",
    "capital": "Stockholm",
    "region": "Europe",
    "population": 10500000
  }'
```
Response: `201 Created`

### List Saved Countries
```bash
curl http://localhost:8080/api/countries
```
Returns countries ordered newest first. Supports optional filters:
* `?region=Europe` — filter by continent
* `?name=land` — search by name substring
* `?limit=10` — limit results count

---

## Testing

Run backend automated tests:
```bash
make test
```
*(or `go test ./backend/... -v`)*

---

## License

MIT

