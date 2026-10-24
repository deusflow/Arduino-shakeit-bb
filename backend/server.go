package main

import (
	"database/sql"
	"embed"
	"encoding/json"
	"fmt"
	"io/fs"
	"log/slog"
	"net/http"
	"strconv"
	"strings"
	"time"
)

//go:embed web/*
var webFS embed.FS

type CountryInput struct {
	ApiName    *string `json:"apiName"`
	Choice     *string `json:"choice"`
	Cca3       *string `json:"cca3"`
	Capital    *string `json:"capital"`
	Region     *string `json:"region"`
	Population *int64  `json:"population"`
}

type ErrorResponse struct {
	Error string `json:"error"`
}

func newServer(cfg Config, db *sql.DB, logger *slog.Logger) http.Handler {
	mux := http.NewServeMux()

	mux.HandleFunc("POST /api/countries", handleCreateCountry(cfg, db, logger))
	mux.HandleFunc("GET /api/countries", handleListCountries(db, logger))

	staticContent, err := fs.Sub(webFS, "web")
	if err != nil {
		logger.Error("failed to mount static web assets", "error", err)
	} else {
		mux.Handle("/", http.FileServer(http.FS(staticContent)))
	}

	return mux
}

func validateCountryInput(input CountryInput, allowedRegions []string) error {
	if input.ApiName == nil || strings.TrimSpace(*input.ApiName) == "" {
		return fmt.Errorf("field apiName is required and cannot be empty")
	}

	if input.Choice == nil || strings.TrimSpace(*input.Choice) == "" {
		return fmt.Errorf("field choice is required and cannot be empty")
	}

	if input.Cca3 == nil {
		return fmt.Errorf("field cca3 is required")
	}
	cca3 := *input.Cca3
	if len(cca3) != 3 {
		return fmt.Errorf("field cca3 must be exactly 3 uppercase letters")
	}
	for i := 0; i < len(cca3); i++ {
		if cca3[i] < 'A' || cca3[i] > 'Z' {
			return fmt.Errorf("field cca3 must be exactly 3 uppercase letters")
		}
	}

	if input.Capital == nil || strings.TrimSpace(*input.Capital) == "" {
		return fmt.Errorf("field capital is required and cannot be empty")
	}

	if input.Region == nil || strings.TrimSpace(*input.Region) == "" {
		return fmt.Errorf("field region is required and cannot be empty")
	}
	matched := false
	for _, ar := range allowedRegions {
		if strings.EqualFold(strings.TrimSpace(*input.Region), ar) {
			matched = true
			break
		}
	}
	if !matched {
		return fmt.Errorf("field region must be one of: %s", strings.Join(allowedRegions, ", "))
	}

	if input.Population == nil {
		return fmt.Errorf("field population is required")
	}
	if *input.Population < 0 {
		return fmt.Errorf("field population must be a whole number greater than or equal to 0")
	}

	return nil
}

func handleCreateCountry(cfg Config, db *sql.DB, logger *slog.Logger) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		r.Body = http.MaxBytesReader(w, r.Body, cfg.MaxBodyBytes)

		dec := json.NewDecoder(r.Body)
		dec.DisallowUnknownFields()

		var input CountryInput
		if err := dec.Decode(&input); err != nil {
			writeJSONError(w, http.StatusBadRequest, fmt.Sprintf("invalid json payload: %v", err))
			return
		}

		// Mercantec demo API occasionally returns double-encoded Latin-1 bytes
		if input.ApiName != nil {
			repaired := repairBrokenUTF8(*input.ApiName)
			input.ApiName = &repaired
		}
		if input.Choice != nil {
			repaired := repairBrokenUTF8(*input.Choice)
			input.Choice = &repaired
		}
		if input.Capital != nil {
			repaired := repairBrokenUTF8(*input.Capital)
			input.Capital = &repaired
		}
		if input.Region != nil {
			repaired := repairBrokenUTF8(*input.Region)
			input.Region = &repaired
		}

		if err := validateCountryInput(input, cfg.AllowedRegions); err != nil {
			writeJSONError(w, http.StatusBadRequest, err.Error())
			return
		}

		savedAt := time.Now().UTC().Format(time.RFC3339)
		saved, err := insertCountry(db, input, savedAt)
		if err != nil {
			logger.Error("failed to insert country", "error", err)
			writeJSONError(w, http.StatusInternalServerError, "failed to store country record")
			return
		}

		logger.Info("country saved", "id", saved.ID, "choice", saved.Choice, "region", saved.Region)

		w.Header().Set("Content-Type", "application/json; charset=utf-8")
		w.WriteHeader(http.StatusCreated)
		_ = json.NewEncoder(w).Encode(saved)
	}
}

func handleListCountries(db *sql.DB, logger *slog.Logger) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		q := r.URL.Query()
		filter := CountryFilter{
			Region: strings.TrimSpace(q.Get("region")),
			Name:   strings.TrimSpace(q.Get("name")),
		}

		if limitStr := strings.TrimSpace(q.Get("limit")); limitStr != "" {
			if limitVal, err := strconv.Atoi(limitStr); err == nil && limitVal > 0 {
				filter.Limit = limitVal
			}
		}

		list, err := listCountries(db, filter)
		if err != nil {
			logger.Error("failed to list countries", "error", err)
			writeJSONError(w, http.StatusInternalServerError, "failed to query countries")
			return
		}

		w.Header().Set("Content-Type", "application/json; charset=utf-8")
		_ = json.NewEncoder(w).Encode(list)
	}
}

func writeJSONError(w http.ResponseWriter, status int, msg string) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(ErrorResponse{Error: msg})
}
