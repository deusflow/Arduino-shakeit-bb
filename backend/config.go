package main

import (
	"os"
	"strings"
)

type Config struct {
	ListenAddr     string
	DbPath         string
	AllowedRegions []string
	MaxBodyBytes   int64
}

func loadConfig() Config {
	addr := os.Getenv("SERVER_ADDR")
	if addr == "" {
		addr = "0.0.0.0:8080"
	}

	dbPath := os.Getenv("DB_PATH")
	if dbPath == "" {
		dbPath = "countries.db"
	}

	regionsEnv := os.Getenv("ALLOWED_REGIONS")
	var regions []string
	if regionsEnv != "" {
		for _, r := range strings.Split(regionsEnv, ",") {
			r = strings.TrimSpace(r)
			if r != "" {
				regions = append(regions, r)
			}
		}
	}
	if len(regions) == 0 {
		regions = []string{"Europe", "Asia", "Africa"}
	}

	return Config{
		ListenAddr:     addr,
		DbPath:         dbPath,
		AllowedRegions: regions,
		MaxBodyBytes:   64 * 1024,
	}
}
