package main

import (
	"bytes"
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"
)

func setupTestServer(t *testing.T) (http.Handler, Config) {
	t.Helper()
	tmpDir := t.TempDir()
	dbPath := filepath.Join(tmpDir, "test.db")

	cfg := Config{
		ListenAddr:   "127.0.0.1:0",
		DbPath:       dbPath,
		MaxBodyBytes: 64 * 1024,
	}

	db, err := openDB(cfg.DbPath)
	if err != nil {
		t.Fatalf("open test db: %v", err)
	}
	t.Cleanup(func() { db.Close() })

	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	return newServer(cfg, db, logger), cfg
}

func TestPostCountry_Valid201(t *testing.T) {
	handler, _ := setupTestServer(t)

	payload := map[string]any{
		"apiName":    "Mercantec Countries API",
		"choice":     "Sweden",
		"cca3":       "SWE",
		"capital":    "Stockholm",
		"region":     "Europe",
		"population": 10500000,
	}
	body, _ := json.Marshal(payload)

	req := httptest.NewRequest("POST", "/api/countries", bytes.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()

	handler.ServeHTTP(w, req)

	if w.Code != http.StatusCreated {
		t.Fatalf("expected status 201, got %d: %s", w.Code, w.Body.String())
	}

	var res SavedCountry
	if err := json.Unmarshal(w.Body.Bytes(), &res); err != nil {
		t.Fatalf("unmarshal response: %v", err)
	}

	if res.ID <= 0 {
		t.Errorf("expected positive id, got %d", res.ID)
	}
	if res.Choice != "Sweden" {
		t.Errorf("expected Sweden, got %s", res.Choice)
	}
	if res.SavedAt == "" {
		t.Error("expected non-empty savedAt")
	}
}

func TestPostCountry_BrokenEncodingName(t *testing.T) {
	handler, _ := setupTestServer(t)

	// real broken encoding from API with U+0085
	brokenName := "\u00c3\u0085land Islands"

	payload := map[string]any{
		"apiName":    "Mercantec Countries API",
		"choice":     brokenName,
		"cca3":       "ALA",
		"capital":    "Mariehamn",
		"region":     "Europe",
		"population": 30654,
	}
	body, _ := json.Marshal(payload)

	req := httptest.NewRequest("POST", "/api/countries", bytes.NewReader(body))
	w := httptest.NewRecorder()

	handler.ServeHTTP(w, req)

	if w.Code != http.StatusCreated {
		t.Fatalf("expected status 201, got %d: %s", w.Code, w.Body.String())
	}

	var res SavedCountry
	_ = json.Unmarshal(w.Body.Bytes(), &res)
	if res.Choice != "Åland Islands" {
		t.Errorf("expected repaired %q, got %q", "Åland Islands", res.Choice)
	}
}

func TestPostCountry_ValidationErrors400(t *testing.T) {
	handler, _ := setupTestServer(t)

	cases := []struct {
		name       string
		payload    map[string]any
		expectWord string
	}{
		{
			name: "bad cca3 lowercase",
			payload: map[string]any{
				"apiName": "Mercantec", "choice": "Sweden", "cca3": "swe",
				"capital": "Stockholm", "region": "Europe", "population": 100,
			},
			expectWord: "cca3",
		},
		{
			name: "bad cca3 length",
			payload: map[string]any{
				"apiName": "Mercantec", "choice": "Sweden", "cca3": "SW",
				"capital": "Stockholm", "region": "Europe", "population": 100,
			},
			expectWord: "cca3",
		},
		{
			name: "bad cca3 with spaces",
			payload: map[string]any{
				"apiName": "Mercantec", "choice": "Sweden", "cca3": " SWE ",
				"capital": "Stockholm", "region": "Europe", "population": 100,
			},
			expectWord: "cca3",
		},
		{
			name: "unknown region",
			payload: map[string]any{
				"apiName": "Mercantec", "choice": "Sweden", "cca3": "SWE",
				"capital": "Stockholm", "region": "Moon", "population": 100,
			},
			expectWord: "region",
		},
		{
			name: "negative population",
			payload: map[string]any{
				"apiName": "Mercantec", "choice": "Sweden", "cca3": "SWE",
				"capital": "Stockholm", "region": "Europe", "population": -5,
			},
			expectWord: "population",
		},
		{
			name: "missing choice",
			payload: map[string]any{
				"apiName": "Mercantec", "cca3": "SWE",
				"capital": "Stockholm", "region": "Europe", "population": 100,
			},
			expectWord: "choice",
		},
		{
			name: "unknown field rejected",
			payload: map[string]any{
				"apiName": "Mercantec", "choice": "Sweden", "cca3": "SWE",
				"capital": "Stockholm", "region": "Europe", "population": 100,
				"extraField": "surprise",
			},
			expectWord: "extraField",
		},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			body, _ := json.Marshal(tc.payload)
			req := httptest.NewRequest("POST", "/api/countries", bytes.NewReader(body))
			w := httptest.NewRecorder()

			handler.ServeHTTP(w, req)

			if w.Code != http.StatusBadRequest {
				t.Fatalf("expected status 400, got %d: %s", w.Code, w.Body.String())
			}

			var errRes ErrorResponse
			if err := json.Unmarshal(w.Body.Bytes(), &errRes); err != nil {
				t.Fatalf("invalid error json: %v", err)
			}
			if errRes.Error == "" {
				t.Error("expected non-empty error message")
			}
			if !strings.Contains(errRes.Error, tc.expectWord) {
				t.Errorf("expected error to mention %q, got %q", tc.expectWord, errRes.Error)
			}
		})
	}
}

func TestGetCountries_Ordering(t *testing.T) {
	handler, _ := setupTestServer(t)

	countries := []map[string]any{
		{"apiName": "Mercantec", "choice": "Sweden", "cca3": "SWE", "capital": "Stockholm", "region": "Europe", "population": 1000},
		{"apiName": "Mercantec", "choice": "Japan", "cca3": "JPN", "capital": "Tokyo", "region": "Asia", "population": 2000},
		{"apiName": "Mercantec", "choice": "Kenya", "cca3": "KEN", "capital": "Nairobi", "region": "Africa", "population": 3000},
	}

	for _, c := range countries {
		body, _ := json.Marshal(c)
		req := httptest.NewRequest("POST", "/api/countries", bytes.NewReader(body))
		w := httptest.NewRecorder()
		handler.ServeHTTP(w, req)
		if w.Code != http.StatusCreated {
			t.Fatalf("setup post failed: %s", w.Body.String())
		}
	}

	// test newest first: first result should be Kenya, then Japan, then Sweden
	req := httptest.NewRequest("GET", "/api/countries", nil)
	w := httptest.NewRecorder()
	handler.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}

	var list []SavedCountry
	_ = json.Unmarshal(w.Body.Bytes(), &list)
	if len(list) != 3 {
		t.Fatalf("expected 3 items, got %d", len(list))
	}
	if list[0].Choice != "Kenya" || list[1].Choice != "Japan" || list[2].Choice != "Sweden" {
		t.Errorf("incorrect order: %v, %v, %v", list[0].Choice, list[1].Choice, list[2].Choice)
	}
}

func TestStaticFiles(t *testing.T) {
	handler, _ := setupTestServer(t)

	req := httptest.NewRequest("GET", "/", nil)
	w := httptest.NewRecorder()
	handler.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected status 200, got %d", w.Code)
	}

	body := w.Body.String()
	if !strings.Contains(body, "Shake to Travel") {
		t.Errorf("expected index.html content to contain 'Shake to Travel', got %s", body)
	}
}
