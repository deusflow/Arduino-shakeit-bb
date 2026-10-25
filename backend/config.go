package main

import (
	"cmp"
	"os"
)

type Config struct {
	ListenAddr   string
	DbPath       string
	MaxBodyBytes int64
}

func loadConfig() Config {
	return Config{
		ListenAddr:   cmp.Or(os.Getenv("SERVER_ADDR"), "0.0.0.0:8080"),
		DbPath:       cmp.Or(os.Getenv("DB_PATH"), "countries.db"),
		MaxBodyBytes: 64 * 1024,
	}
}
