#ifndef CONFIG_H
#define CONFIG_H

// Upstream Countries API
const char API_HOST[] = "api-opgaver.mercantec.tech";
const int API_PORT = 443;
const char API_PATH_PREFIX[] = "/api/Countries/name/";

// Local Go backend
const char BACKEND_HOST[] = "172.20.10.11";
const int BACKEND_PORT = 8080;
const char BACKEND_PATH[] = "/api/countries";

// Network and timing
const unsigned long HTTP_TIMEOUT_MS = 10000;
const unsigned long SERIAL_BAUD = 9600;
const unsigned long SAVED_SCREEN_DURATION_MS = 2000;

// IMU shake sensitivity
const float SHAKE_THRESHOLD = 1.35f;
const unsigned long SHAKE_COOLDOWN_MS = 1500;

#endif
