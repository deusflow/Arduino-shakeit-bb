.PHONY: help run test gencountries compile-arduino clean archive

help:
	@echo "Shake to Travel - available commands:"
	@echo "  make run             - Start server and dashboard (http://localhost:8080)"
	@echo "  make test            - Run backend autotests"
	@echo "  make gencountries    - Regenerate arduino/countries.h from API"
	@echo "  make compile-arduino - Verify Arduino sketch compilation"
	@echo "  make clean           - Remove local database and build artifacts"
	@echo "  make archive         - Create clean zip archive without secrets"

run:
	go run ./backend

test:
	go test ./... -v

gencountries:
	go run ./backend/cmd/gencountries

compile-arduino:
	"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" compile --fqbn arduino:samd:mkrwifi1010 arduino

clean:
	rm -f countries.db countries.db-journal

archive:
	git archive --format=zip --output=shake-to-travel.zip HEAD
	@echo "Created shake-to-travel.zip (clean git tracked files only, no secrets)"
