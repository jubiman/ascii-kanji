#include <iostream>
#include <locale>
#include <vector>
#include <fontconfig/fontconfig.h>

// FreeType headers
#include <ft2build.h>
#include FT_FREETYPE_H

// ICU headers
#include <format>
#include <unicode/ustring.h>

std::vector<uint32_t> utf8ToUnicode(const std::string& utf8);

// Struct to hold glyph bitmap data
struct GlyphBitmap {
    unsigned int width{};
    unsigned int rows{};
    int pitch{};
    int left{};
    int top{};
    std::vector<unsigned char> buffer;
};

int main(const int argc, const char* argv[]) {
	// Parse command-line arguments
	if (argc == 1) {
		std::cerr << "Usage: " << argv[0] << " <kanji>" << std::endl;
		return 1;
	}
	std::string kanji;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) kanji += " ";
        kanji += argv[i];
    }

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
	for (const std::vector<uint32_t> utf32 = utf8ToUnicode(kanji); const auto code : utf32)
		FcCharSetAddChar(charset, code);

	FcPatternAddCharSet(pattern, FC_CHARSET, charset);
	FcCharSetDestroy(charset);

	FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
	FcDefaultSubstitute(pattern);
	FcResult result;
	FcPattern* matched = FcFontMatch(nullptr, pattern, &result);
	if (matched) {
		FcChar8* fontFile = nullptr;
		if (FcPatternGetString(matched, FC_FILE, 0, &fontFile) == FcResultMatch) {
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
			const std::vector<uint32_t> codes = utf8ToUnicode(kanji);

			// Draw the characters side by side
			constexpr int padding = 2; // Padding between characters
			uint32_t totalWidth = 0;
			uint32_t maxHeight = 0;
			std::vector<GlyphBitmap> glyphs;
			for (const auto codepoint : codes) {
				if (const FT_UInt glyph_index = FT_Get_Char_Index(face, codepoint); FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER)) {
					std::cerr << "Could not load glyph for character" << std::endl;
					FT_Done_Face(face);
					FT_Done_FreeType(ft);
					return 1;
				}
				FT_GlyphSlot g = face->glyph;
				// Copy bitmap data
				GlyphBitmap gb;
				gb.width = g->bitmap.width;
				gb.rows = g->bitmap.rows;
				gb.pitch = g->bitmap.pitch;
				gb.left = g->bitmap_left;
				gb.top = g->bitmap_top;
				gb.buffer.assign(g->bitmap.buffer, g->bitmap.buffer + g->bitmap.width * g->bitmap.rows);
				glyphs.push_back(std::move(gb));
				totalWidth += g->bitmap.width + padding;
				if (g->bitmap.rows > maxHeight) {
					maxHeight = g->bitmap.rows;
				}
			}
			totalWidth -= padding; // Remove last padding
			std::vector<unsigned char> canvas(maxHeight * totalWidth, ' ');
			unsigned int xOffset = 0;
			for (const auto& gb : glyphs) {
				for (int y = 0; y < gb.rows; ++y) {
					for (int x = 0; x < gb.width; ++x) {
						constexpr int shades_len = 10;
						const auto shades = " .:-=+*#%@";
						const unsigned char pixel = gb.buffer[y * gb.width + x];
						const char ascii = shades[(pixel * (shades_len - 1)) / 255];
						canvas[y * totalWidth + xOffset + x] = ascii;
					}
				}
				xOffset += gb.width + padding;
			}
			// Print the canvas
			for (int y = 0; y < maxHeight; ++y) {
				for (int x = 0; x < totalWidth; ++x) {
					std::cout << canvas[y * totalWidth + x];
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

std::vector<uint32_t> utf8ToUnicode(const std::string& utf8) {
	std::vector<uint32_t> codepoints;
	if (utf8.empty()) return codepoints;

	uint32_t codepoint = 0;
	size_t i = 0;
	while (i < utf8.size()) {
		const auto firstByte = static_cast<uint8_t>(utf8[i]);
		int numBytes = 0;
		if ((firstByte & 0x80) == 0) {
			codepoint = firstByte;
			numBytes = 1;
		} else if ((firstByte & 0xE0) == 0xC0) {
			codepoint = firstByte & 0x1F;
			numBytes = 2;
		} else if ((firstByte & 0xF0) == 0xE0) {
			codepoint = firstByte & 0x0F;
			numBytes = 3;
		} else if ((firstByte & 0xF8) == 0xF0) {
			codepoint = firstByte & 0x07;
			numBytes = 4;
		} else {
			++i; // Skip invalid byte
			continue;
		}
		if (i + numBytes > utf8.size()) break; // Incomplete UTF-8
		for (int j = 1; j < numBytes; ++j) {
			const auto byte = static_cast<uint8_t>(utf8[i + j]);
			if ((byte & 0xC0) != 0x80) {
				numBytes = j; // Adjust to valid bytes only
				break;
			}
			codepoint = (codepoint << 6) | (byte & 0x3F);
		}
		codepoints.push_back(codepoint);
		i += numBytes;
	}

	return codepoints;
}