#include <iostream>
#include <locale>
#include <fontconfig/fontconfig.h>

// FreeType headers
#include <ft2build.h>
#include FT_FREETYPE_H

// ICU headers
#include <format>
#include <unicode/ustring.h>

#define STR "火"
FT_ULong utf8ToCodePoint(const std::string& utf8);

int main(const int argc, const char* argv[]) {
	// Parse command-line arguments
	// if (argc == 1) {
	// 	std::cerr << "Usage: " << argv[0] << " <kanji>" << std::endl;
	// 	return 1;
	// }
	// const std::string kanji = argv[1];
	// std::cout << "You entered the kanji: " << kanji << std::endl;
	const std::string& kanji = STR; // TODO

	// Get character font information
	FcInit();
	FcPattern* pattern = FcPatternCreate();

	// Convert kanji to Unicode codepoint and create a charset
	// It's kinda cursed, but it works
	if (kanji.empty()) {
		std::cerr << "Empty kanji input" << std::endl;
		return 1;
	}

	FcCharSet* charset = FcCharSetCreate();
	for (const char c : kanji) {
		FcCharSetAddChar(charset, c);
	}
	FcPatternAddCharSet(pattern, FC_CHARSET, charset);
	FcCharSetDestroy(charset);

	FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
	FcDefaultSubstitute(pattern);
	FcResult result;
	FcPattern* matched = FcFontMatch(nullptr, pattern, &result);
	if (matched) {
		FcChar8* fontFile = nullptr;
		if (FcPatternGetString(matched, FC_FILE, 0, &fontFile) == FcResultMatch) {
			std::cout << "Font file: " << fontFile << std::endl;

			// Initialize FreeType
			FT_Library ft;
			if (FT_Init_FreeType(&ft)) {
				std::cerr << "Could not initialize FreeType library" << std::endl;
				return 1;
			}
			// Load font face
			FT_Face face;
			if (FT_New_Face(ft, reinterpret_cast<const char *>(fontFile), 0, &face)) {
				std::cerr << "Could not load font face" << std::endl;
				FT_Done_FreeType(ft);
				return 1;
			}
			FT_Set_Pixel_Sizes(face, 0, 48); // Set desired glyph size
			// Ensure Unicode charmap is loaded
			FT_Select_Charmap(face, FT_ENCODING_UNICODE);

			// Load and render glyph
			const FT_ULong codepoint = utf8ToCodePoint(kanji);
			// if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER)) {
			// 	std::cerr << "Could not load glyph for character" << std::endl;
			// 	FT_Done_Face(face);
			// 	FT_Done_FreeType(ft);
			// 	return 1;
			// }
			std::cout << "Codepoint: U+" << std::hex << codepoint << std::dec << std::endl;
			FT_ULong charcode;
			FT_UInt gid;

			charcode = FT_Get_First_Char(face, &gid);
			// TODO: seems like it uses a fallback font (not NotoSans Regular) for the glyph
			//       how to get the actual font used?
			while (gid != 0) {
				if (charcode == 0x706b) {
					std::cout << std::format("Codepoint: {:x}, gid: {}", charcode, gid) << std::endl;
				}
				charcode = FT_Get_Next_Char(face, charcode, &gid);
			}
			const FT_UInt glyph_index = FT_Get_Char_Index(face, codepoint);
			if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER)) {
				std::cerr << "Could not load glyph for character" << std::endl;
				FT_Done_Face(face);
				FT_Done_FreeType(ft);
				return 1;
			}
			FT_GlyphSlot g = face->glyph;

			// ASCII rendering
			for (int y = 0; y < g->bitmap.rows; ++y) {
				for (int x = 0; x < g->bitmap.width; ++x) {
					constexpr int shades_len = 10;
					const auto shades = " .:-=+*#%@";
					const unsigned char pixel = g->bitmap.buffer[y * g->bitmap.width + x];
					// const char ascii = shades[(pixel * (shades_len - 1)) / 255];
					const char ascii = pixel ? '#' : ' ';
					std::cout << ascii;
				}
				std::cout << std::endl;
			}

			FT_Done_Face(face);
			FT_Done_FreeType(ft);
		} else {
			std::cout << "Font file not found." << std::endl;
		}
	}
	FcPatternDestroy(pattern);
	FcPatternDestroy(matched);
	FcFini();

	return 0;
}

FT_ULong utf8ToCodePoint(const std::string& utf8) {
	if (utf8.empty()) return 0;

	FT_ULong codePoint = 0;
	const auto firstByte = static_cast<uint8_t>(utf8[0]);
	int numBytes = 0;
	if ((firstByte & 0x80) == 0) {
		codePoint = firstByte;
		numBytes = 1;
	} else if ((firstByte & 0xE0) == 0xC0) {
		codePoint = firstByte & 0x1F;
		numBytes = 2;
	} else if ((firstByte & 0xF0) == 0xE0) {
		codePoint = firstByte & 0x0F;
		numBytes = 3;
	} else if ((firstByte & 0xF8) == 0xF0) {
		codePoint = firstByte & 0x07;
		numBytes = 4;
	} else {
		return 0; // Invalid UTF-8
	}
	if (utf8.size() < static_cast<size_t>(numBytes)) return 0; // Incomplete UTF-8
	for (int i = 1; i < numBytes; ++i) {
		const auto byte = static_cast<uint8_t>(utf8[i]);
		if ((byte & 0xC0) != 0x80) return 0; // Invalid continuation byte
		codePoint = (codePoint << 6) | (byte & 0x3F);
	}
	return codePoint;
}