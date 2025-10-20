// Localization support for runtime string translation via external .loc files.

#ifndef FS_LOCALIZATION_H
#define FS_LOCALIZATION_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <type_traits>

class Localization {
public:
	static Localization& instance();

	// Load translations from a file. Format per line:
	// <english>@<translation>
	// Supports placeholders "%s" / "%d" replaced in order using translate(key, args).
	bool load(std::string_view filePath);

	// Translate a key. If not found, returns the key itself.
	std::string translate(std::string_view key) const;

	// Translate and apply ordered placeholder substitution for "%s"/%"d".
	std::string translate(std::string_view key, const std::vector<std::string>& args) const;

	// Per-language translation. Tenta idioma exato e variantes; fallback é a própria chave.
	std::string translate(std::string_view key, std::string_view language) const;

	// Per-language translation with placeholder substitution.
	std::string translate(std::string_view key, std::string_view language, const std::vector<std::string>& args) const;

private:
	Localization() = default;
	Localization(const Localization&) = delete;
	Localization& operator=(const Localization&) = delete;

	// Backward-compatible default translations (last loaded language)
	std::unordered_map<std::string, std::string> translations;
	// Per-language dictionaries (keyed by language code, e.g., "pt-br")
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> translationsByLang;
	// Default language used by translate(key)
	std::string defaultLanguage;
	mutable std::mutex mtx;
};

// Legacy convenience wrappers for tr()/trf() removed. Use translate(key, language[, args]).

#endif // FS_LOCALIZATION_H