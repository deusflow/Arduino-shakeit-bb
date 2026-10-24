package main

import (
	"context"
	"errors"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"
)

func main() {
	// structured logging
	logger := slog.New(slog.NewTextHandler(os.Stdout, nil))

	cfg := loadConfig()

	db, err := openDB(cfg.DbPath)
	if err != nil {
		logger.Error("could not open sqlite database", "error", err, "path", cfg.DbPath)
		os.Exit(1)
	}
	defer db.Close()
	logger.Info("database opened", "path", cfg.DbPath)

	handler := newServer(cfg, db, logger)

	srv := &http.Server{
		Addr:              cfg.ListenAddr,
		Handler:           handler,
		ReadHeaderTimeout: 5 * time.Second,
		ReadTimeout:       10 * time.Second,
		WriteTimeout:      10 * time.Second,
		IdleTimeout:       60 * time.Second,
	}

	// start server in background
	go func() {
		logger.Info("server starting", "addr", cfg.ListenAddr)
		if err := srv.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
			logger.Error("server failed", "error", err)
			os.Exit(1)
		}
	}()

	// wait for interrupt signal for graceful shutdown
	stop := make(chan os.Signal, 1)
	signal.Notify(stop, os.Interrupt, syscall.SIGTERM)
	<-stop

	logger.Info("shutting down server...")
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	if err := srv.Shutdown(ctx); err != nil {
		logger.Error("server forced to shutdown", "error", err)
	} else {
		logger.Info("server exited cleanly")
	}
}
