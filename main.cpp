/**
 * Print a simple unicode file
 *
 * @file main.cpp
 * @section LICENSE

    This code is under MIT License, http://opensource.org/licenses/MIT
 */

#include <iostream>
#include <locale>
#include <codecvt>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <cstdint>
#include <assert.h>

namespace
{

// utility wrapper to adapt locale-bound facets for wstring/wbuffer convert
template<class Facet>
struct deletable_facet : Facet
{
	template<class ...Args>
	deletable_facet(Args&& ...args) : Facet(std::forward<Args>(args)...) {}
	~deletable_facet() {}
};

enum struct UnicodeType : int8_t
{
	Mbcs,
	Utf8,
	Utf8_BOM,
	Utf16_LE_BOM,
	Utf16_BE_BOM,
	Ucs2
};


// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
const uint8_t utf8d[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 00..1f
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 20..3f
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 40..5f
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 60..7f
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 80..9f
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, // a0..bf
	8, 8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // c0..df
	0xa, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x4, 0x3, 0x3, // e0..ef
	0xb, 0x6, 0x6, 0x6, 0x5, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, // f0..ff
	0x0, 0x1, 0x2, 0x3, 0x5, 0x8, 0x7, 0x1, 0x1, 0x1, 0x4, 0x6, 0x1, 0x1, 0x1, 0x1, // s0..s0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, // s1..s2
	1, 2, 1, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, // s3..s4
	1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 3, 1, 1, 1, 1, 1, 1, // s5..s6
	1, 3, 1, 1, 1, 1, 1, 3, 1, 3, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // s7..s8
};

UnicodeType GetFormatType(const char* begin, const char* end)
{
	static const char UTF_8_BOM[] = "\xEF\xBB\xBF";
	static const char UTF_16_LE_BOM[] = "\xFF\xFE";
	static const char UTF_16_BE_BOM[] = "\xFE\xFF";

	assert(begin != nullptr);
	assert(end != nullptr);
	assert(begin <= end);
	const size_t size = std::distance(begin, end);

	if (size >= 3 && std::equal(UTF_8_BOM, UTF_8_BOM + 3, begin)) // danger but use memcmp
		return UnicodeType::Utf8_BOM;

	if (size >= 2) {
		if (std::equal(UTF_16_LE_BOM, UTF_16_LE_BOM + 2, begin)) // danger but use memcmp
			return UnicodeType::Utf16_LE_BOM;

		if (std::equal(UTF_16_BE_BOM, UTF_16_BE_BOM + 2, begin)) // danger but use memcmp
			return UnicodeType::Utf16_BE_BOM;
	}

	// We don't care about the codepoint, so this is
	// a simplified version of the decode function.
	uint32_t state = 1;
	auto result = UnicodeType::Mbcs;
	return end != std::find_if(begin, end, [&state, &result](char c)
	{
		if (c == 0x9 || c == 0x0a || c == 0x0d || (0x20 <= c && c <= 0x7E))
			return false;

		result = UnicodeType::Ucs2;

		const uint32_t type = utf8d[static_cast<uint8_t>(c)];
		state = utf8d[256 + 16*state + type];
		return state != 0;
	}) ? UnicodeType::Utf8 : result;
}


std::u16string ConvertFromBytes(const char* begin, const char* end)
{
	assert(begin != nullptr);
	assert(end != nullptr);
	assert(begin <= end);

	const auto type = GetFormatType(begin, end);
	switch (type)
	{
	case UnicodeType::Mbcs:
	{
		std::wstring_convert<deletable_facet<std::codecvt<char16_t, char, mbstate_t>>, char16_t> converter;
		return converter.from_bytes(begin, end);
	}
	case UnicodeType::Utf8:
	{
		std::wstring_convert<deletable_facet<std::codecvt_utf8_utf16<char16_t>>, char16_t> converter;
		return converter.from_bytes(begin, end);
	}
	case UnicodeType::Utf8_BOM:
	{
		std::wstring_convert<deletable_facet<std::codecvt_utf8_utf16<char16_t, 0x10ffff, std::consume_header>>, char16_t> converter;
		return converter.from_bytes(begin, end);
	}
	case UnicodeType::Utf16_LE_BOM:
	case UnicodeType::Utf16_BE_BOM:
	{
		std::wstring_convert<deletable_facet<std::codecvt_utf16<char16_t, 0x10ffff, static_cast<std::codecvt_mode>(std::consume_header)>>, char16_t> converter;
		return converter.from_bytes(begin, end);
	}
	case UnicodeType::Ucs2: // utf16 without BOM
	{
		std::wstring_convert<deletable_facet<std::codecvt_utf16<char16_t>>, char16_t> converter;
		return converter.from_bytes(begin, end);
	}
	default:
		assert(false);
		break;
	}
}

}

int main(int argc, char* argv[]) {
	std::cout << "Test print unicode text file\n";

	if (argc < 2)
		return -1;

	std::ifstream inputFile(argv[1], std::ios::in | std::ios_base::binary);
	
	if (inputFile.bad())
		return -1;

	std::cout << "bytes before convert:\n";
	const std::vector<char> buf((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
	if (buf.empty())
		return -1;

	std::for_each(cbegin(buf), cend(buf), [](char c)
	{
		std::cout << std::hex << std::showbase << c;
	});

	auto str16 = ConvertFromBytes(buf.data(), buf.data() + buf.size());

	std::cout << "Converted to following UTF-16 code points: \n";
	for (auto c : str16)
		std::cout << "U+" << std::hex << std::setw(4) << std::setfill('0') << c << '\n';

	return 0;
}
