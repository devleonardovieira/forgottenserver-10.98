#include "otpch.h"

#include "localization.h"

#include <fstream>
#include <sstream>
#include <algorithm>

namespace {
	static inline void trim(std::string& s) {
		auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
		s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
	}

	static inline bool startsWith(const std::string& s, const char* prefix) {
		return s.compare(0, std::strlen(prefix), prefix) == 0;
	}

	// Replace ordered %s/%d placeholders with provided params
	static std::string replacePlaceholders(std::string text, const std::vector<std::string>& params) {
		std::size_t searchPos = 0;
		std::size_t argIndex = 0;
		while (argIndex < params.size()) {
			std::size_t p = text.find('%', searchPos);
			if (p == std::string::npos || p + 1 >= text.size()) {
				break;
			}
			char spec = text[p + 1];
			if (spec == 's' || spec == 'd') {
				text.replace(p, 2, params[argIndex]);
				searchPos = p + params[argIndex].size();
				++argIndex;
			} else {
				searchPos = p + 1; // skip unknown specifier
			}
		}
		return text;
	}

	static std::string lower(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
		return s;
	}

	// Helper to convert string_view to string in C++14-compatible way
	static inline std::string sv_to_string(std::string_view sv) {
		return std::string(sv.data(), sv.size());
	}

	// Derive language code from a file path (basename without extension)
	static std::string deriveLanguageCode(std::string_view path) {
		std::string p = sv_to_string(path);
		std::size_t pos = p.find_last_of("/\\");
		std::string fname = (pos == std::string::npos) ? p : p.substr(pos + 1);
		std::size_t dot = fname.find_last_of('.');
		std::string base = (dot == std::string::npos) ? fname : fname.substr(0, dot);
		return lower(base);
	}
}

Localization& Localization::instance() {
	static Localization inst;
	return inst;
}

bool Localization::load(std::string_view filePath) {
	std::lock_guard<std::mutex> lock(mtx);
	// Load into a local map so multiple languages can be loaded across calls
	std::unordered_map<std::string, std::string> map;

	std::ifstream in{sv_to_string(filePath)};
	if (!in.is_open()) {
		std::cout << ">> Localization: file not found: " << sv_to_string(filePath) << std::endl;
		return false;
	}

	std::string line;
	std::size_t lineNo = 0;
	while (std::getline(in, line)) {
		++lineNo;
		trim(line);
		if (line.empty()) continue;
		if (startsWith(line, "#") || startsWith(line, "//")) continue;

		// split at first '@'
		auto atPos = line.find('@');
		if (atPos == std::string::npos) {
			// allow "key=value" as alternative
			auto eqPos = line.find('=');
			if (eqPos == std::string::npos) {
				std::cout << ">> Localization: invalid line " << lineNo << ": " << line << std::endl;
				continue;
			}
			std::string key = line.substr(0, eqPos);
			std::string val = line.substr(eqPos + 1);
			trim(key);
			trim(val);
			map[key] = val;
			continue;
		}

		std::string key = line.substr(0, atPos);
		std::string val = line.substr(atPos + 1);

		// Optional ",," terminator in value (per user example)
		if (val.size() >= 2 && val.substr(val.size() - 2) == ",,") {
			val.erase(val.size() - 2);
		}

		trim(key);
		trim(val);
		if (!key.empty() && !val.empty()) {
			map[key] = val;
		}
	}

	const std::string lang = deriveLanguageCode(filePath);
	translationsByLang[lang] = std::move(map);
	translations = translationsByLang[lang]; // keep old API working
	defaultLanguage = lang;

	std::cout << ">> Localization: loaded " << translations.size() << " entries for language '" << lang << "' from " << sv_to_string(filePath) << std::endl;
	return true;
}

std::string Localization::translate(std::string_view key) const {
	std::lock_guard<std::mutex> lock(mtx);
	auto it = translations.find(sv_to_string(key));
	if (it != translations.end()) {
		return it->second;
	}
	return sv_to_string(key);
}

std::string Localization::translate(std::string_view key, const std::vector<std::string>& args) const {
	std::lock_guard<std::mutex> lock(mtx);
	std::string tmpl;
	auto it = translations.find(sv_to_string(key));
	if (it != translations.end()) {
		tmpl = it->second;
	} else {
		tmpl = sv_to_string(key);
	}
	return replacePlaceholders(std::move(tmpl), args);
}

std::string Localization::translate(std::string_view key, std::string_view language) const {
	std::lock_guard<std::mutex> lock(mtx);
	const auto langKey = sv_to_string(language);
	const auto keyStr = sv_to_string(key);

	// Try exact language first
	auto dictIt = translationsByLang.find(langKey);
	if (dictIt != translationsByLang.end()) {
		const auto& dict = dictIt->second;
		auto it = dict.find(keyStr);
		if (it != dict.end()) {
			return it->second;
		}
	}

	// Try language variants, e.g. 'pt' -> 'pt-br', 'en' -> 'en-us'
	std::string base = langKey;
	auto sep = base.find_first_of("-_");
	if (sep != std::string::npos) {
		base.erase(sep);
	}
	for (const auto& kv : translationsByLang) {
			const std::string& code = kv.first;
			if (code == base || (code.size() > base.size() && (startsWith(code, (base + "-").c_str()) || startsWith(code, (base + "_").c_str())))) {
				const auto& dict = kv.second;
				auto it = dict.find(keyStr);
				if (it != dict.end()) {
					return it->second;
				}
			}
		}

	// Final fallback: the key itself (original text)
	return keyStr;
}

std::string Localization::translate(std::string_view key, std::string_view language, const std::vector<std::string>& args) const {
	std::lock_guard<std::mutex> lock(mtx);
	const auto langKey = sv_to_string(language);
	const auto keyStr = sv_to_string(key);
	std::string tmpl;

	// Try exact language first
	auto dictIt = translationsByLang.find(langKey);
	if (dictIt != translationsByLang.end()) {
		const auto& dict = dictIt->second;
		auto it = dict.find(keyStr);
		if (it != dict.end()) {
			tmpl = it->second;
		}
	}

	// Try language variants, e.g. 'pt' -> 'pt-br', 'en' -> 'en-us'
	if (tmpl.empty()) {
		std::string base = langKey;
		auto sep = base.find_first_of("-_");
		if (sep != std::string::npos) {
			base.erase(sep);
		}
		for (const auto& kv : translationsByLang) {
			const std::string& code = kv.first;
			if (code == base || (code.size() > base.size() && (startsWith(code, (base + "-").c_str()) || startsWith(code, (base + "_").c_str())))) {
				const auto& dict = kv.second;
				auto it = dict.find(keyStr);
				if (it != dict.end()) {
					tmpl = it->second;
					break;
				}
			}
		}
	}

	if (tmpl.empty()) {
		tmpl = keyStr;
	}
	return replacePlaceholders(std::move(tmpl), args);
}