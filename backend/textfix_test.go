package main

import "testing"

func TestRepairBrokenUTF8(t *testing.T) {
	cases := []struct {
		name     string
		input    string
		expected string
	}{
		{
			name:     "Aland Islands broken encoding with U+0085",
			input:    "\u00c3\u0085land Islands",
			expected: "Åland Islands",
		},
		{
			name:     "Reunion broken encoding",
			input:    "R\u00c3\u00a9union",
			expected: "Réunion",
		},
		{
			name:     "Sao Tome and Principe broken encoding",
			input:    "S\u00c3\u00a3o Tom\u00c3\u00a9 and Pr\u00c3\u00adncipe",
			expected: "São Tomé and Príncipe",
		},
		{
			name:     "Sweden plain ASCII unchanged",
			input:    "Sweden",
			expected: "Sweden",
		},
		{
			name:     "Sao Tome already correct UTF8 unchanged",
			input:    "São Tomé",
			expected: "São Tomé",
		},
		{
			name:     "Aland Islands already correct UTF8 unchanged",
			input:    "Åland Islands",
			expected: "Åland Islands",
		},
		{
			name:     "empty string unchanged",
			input:    "",
			expected: "",
		},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			got := repairBrokenUTF8(tc.input)
			if got != tc.expected {
				t.Errorf("repairBrokenUTF8(%q) = %q, expected %q", tc.input, got, tc.expected)
			}
		})
	}
}
