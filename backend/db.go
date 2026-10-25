package main

import (
	"database/sql"
	"fmt"

	_ "modernc.org/sqlite"
)

type SavedCountry struct {
	ID         int64  `json:"id"`
	ApiName    string `json:"apiName"`
	Choice     string `json:"choice"`
	Cca3       string `json:"cca3"`
	Capital    string `json:"capital"`
	Region     string `json:"region"`
	Population int64  `json:"population"`
	SavedAt    string `json:"savedAt"`
}

func openDB(path string) (*sql.DB, error) {
	db, err := sql.Open("sqlite", path)
	if err != nil {
		return nil, fmt.Errorf("open sqlite: %w", err)
	}

	schema := `
	CREATE TABLE IF NOT EXISTS countries (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		api_name TEXT NOT NULL,
		choice TEXT NOT NULL,
		cca3 TEXT NOT NULL,
		capital TEXT NOT NULL,
		region TEXT NOT NULL,
		population INTEGER NOT NULL,
		saved_at TEXT NOT NULL
	);
	`
	if _, err := db.Exec(schema); err != nil {
		db.Close()
		return nil, fmt.Errorf("init schema: %w", err)
	}

	return db, nil
}

func insertCountry(db *sql.DB, c CountryInput, savedAt string) (SavedCountry, error) {
	query := `
	INSERT INTO countries (api_name, choice, cca3, capital, region, population, saved_at)
	VALUES (?, ?, ?, ?, ?, ?, ?)
	`
	res, err := db.Exec(query, c.ApiName, c.Choice, c.Cca3, c.Capital, c.Region, *c.Population, savedAt)
	if err != nil {
		return SavedCountry{}, fmt.Errorf("insert country: %w", err)
	}

	id, err := res.LastInsertId()
	if err != nil {
		return SavedCountry{}, fmt.Errorf("get last insert id: %w", err)
	}

	return SavedCountry{
		ID:         id,
		ApiName:    c.ApiName,
		Choice:     c.Choice,
		Cca3:       c.Cca3,
		Capital:    c.Capital,
		Region:     c.Region,
		Population: *c.Population,
		SavedAt:    savedAt,
	}, nil
}

func listCountries(db *sql.DB) ([]SavedCountry, error) {
	query := "SELECT id, api_name, choice, cca3, capital, region, population, saved_at FROM countries ORDER BY id DESC"

	rows, err := db.Query(query)
	if err != nil {
		return nil, fmt.Errorf("query countries: %w", err)
	}
	defer rows.Close()

	var result []SavedCountry
	for rows.Next() {
		var item SavedCountry
		err := rows.Scan(
			&item.ID,
			&item.ApiName,
			&item.Choice,
			&item.Cca3,
			&item.Capital,
			&item.Region,
			&item.Population,
			&item.SavedAt,
		)
		if err != nil {
			return nil, fmt.Errorf("scan country: %w", err)
		}
		result = append(result, item)
	}

	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("rows iteration: %w", err)
	}

	if result == nil {
		result = []SavedCountry{}
	}

	return result, nil
}
