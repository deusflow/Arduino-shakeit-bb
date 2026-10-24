package main

import "unicode/utf8"

// repairBrokenUTF8 fixes strings that were parsed as Latin-1 / ISO-8859-1
// instead of UTF-8 by reinterpreting individual code points as raw bytes.
func repairBrokenUTF8(s string) string {
	if s == "" {
		return s
	}

	buf := make([]byte, 0, len(s))
	for _, r := range s {
		if r > 0xFF {
			return s
		}
		buf = append(buf, byte(r))
	}

	if !utf8.Valid(buf) {
		return s
	}

	return string(buf)
}
