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
#include <ios>
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

	return UnicodeType::Utf8;
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
		std::wstring_convert<deletable_facet<std::codecvt_utf16<char16_t, 0x10ffff, std::consume_header>>, char16_t> converter;
		return converter.from_bytes(begin, end);
	}
	default:
		break;
	}

	std::wstring_convert<deletable_facet<std::codecvt_utf16<char16_t>>, char16_t> converter;
	return converter.from_bytes(begin, end);
}

std::wifstream& SafeGetLine(std::wifstream& in, std::wstring& line)
{
	line.clear();
	std::wifstream::sentry se(in, true);
	
	auto* buf = in.rdbuf();
	for (;;)
	{
		int c = buf->sbumpc();
		switch(c) {
		case 0xfeff:
		case 0xfffe:
			continue;
		case 0xa00:
		case L'\n':
			return in;
		case 0xd00:
		case L'\r':
		{
			auto ch = buf->sgetc();
			if (ch == L'\n' || ch == 0xa00)
				buf->sbumpc();
			return in;
		}
		case WEOF:
			// Also handle the case when the last line has no line ending
			if (line.empty())
				in.setstate(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
			return in;
		default:
			line += static_cast<wchar_t>(c);
		}
	}
}

}

int main(int argc, char* argv[]) {
	std::cout << "Test print unicode text file\n";

	if (argc < 2)
		return -1;

	{
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
	}
	/*
	FILE* f = nullptr;
	if (fopen_s(&f, argv[1], "rt, ccs=UTF-8"))
		return -1;

	std::cout << "\nConverted to following UTF-16 by fgetws: \n";

	wchar_t wbuf[4096] = {};
	while (fgetws(wbuf, 4096, f))
	{
		for (auto c : wbuf)
		{
			if (!c || c==10) break;
			std::cout << "U+" << std::hex << std::setw(4) << std::setfill('0') << c << ' ';
		}
		std::cout << std::endl;
	}

	fclose(f);
	*/
	std::wifstream in(argv[1], std::ios::binary | std::ios::ate);
	if (in.bad())
		return -1;
	int64_t fsize = in.tellg();
	if (!fsize)
		return -1;

	in.seekg(0, std::ios::beg);
	in.imbue(std::locale(in.getloc(), new std::codecvt_utf8_utf16<wchar_t, 0x10ffff, std::consume_header>));

	std::cout << "\nConverted to following UTF-16 by wifstream: \n";
	std::wstring line;
	for (bool first = true;; first = false)
	{
		//getline(in, line);
		SafeGetLine(in, line);

		if (in.bad() || in.fail())
		{
			if (first)
			{
				in.close();
				in.open(argv[1], std::ios::binary);
				in.seekg(0, std::ios::beg);
				in.imbue(std::locale(in.getloc(), new std::codecvt_utf16<wchar_t, 0x10ffff, std::consume_header>));
				continue;
			}
			break;
		}
		else
		{
			for (auto c : line)
				std::cout << "U+" << std::hex << std::setw(4) << std::setfill('0') << c << ' ';

			std::cout << std::endl;
			if (in.eof())
				break;
		}
	}
	return 0;
}
