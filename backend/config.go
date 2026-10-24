package main

import (
	"cmp"
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
	var regions []string
	if env := os.Getenv("ALLOWED_REGIONS"); env != "" {
		for _, r := range strings.Split(env, ",") {
			if trimmed := strings.TrimSpace(r); trimmed != "" {
				regions = append(regions, trimmed)
			}
		}
	}
	if len(regions) == 0 {
		regions = []string{"Europe", "Asia", "Africa"}
	}

	return Config{
		ListenAddr:     cmp.Or(os.Getenv("SERVER_ADDR"), "0.0.0.0:8080"),
		DbPath:         cmp.Or(os.Getenv("DB_PATH"), "countries.db"),
		AllowedRegions: regions,
		MaxBodyBytes:   64 * 1024,
	}
}
