#ifndef _2609BFC9_2CCB_490F_9B56_0A4D3DA0132B_
#define  _2609BFC9_2CCB_490F_9B56_0A4D3DA0132B_

#include <assert.h>
#include <atlcomcli.h>
#include <MLang.h>

#define CP_UTF16_LE 1200
#define CP_UTF16_BE 1201
#define CP_UTF32_LE 12000
#define CP_UTF32_BE 12001

// utility wrapper to adapt locale-bound facets for wstring/wbuffer convert
template<class Facet>
struct deletable_facet : Facet
{
	template<class ...Args>
	deletable_facet(Args&& ...args) : Facet(std::forward<Args>(args)...) {}
	~deletable_facet() {}
};

enum class TextEncoding : int
{
	Ansi = CP_ACP, UTF8 = CP_UTF8, UTF16LE = CP_UTF16_LE, UTF16BE = CP_UTF16_BE, UTF32LE = CP_UTF32_LE, UTF32BE = CP_UTF32_BE
};

class EncodeDetector
	: public CComPtr<IMultiLanguage2>
{
	static const int MaxCodePages = 10;
public:
	EncodeDetector()
	{
		CComQIPtr<IMultiLanguage> pML;
		if (pML.CoCreateInstance(CLSID_CMultiLanguage) == S_OK)
		{
			if (pML.QueryInterface(&p) != S_OK)
			{
				throw;
			}
		}
	};

	// this function is specific to our CS needs. It'll only tell one of 3 answers:
	//  eUTF16 -    if the file actually has the UTF-16 BOM (or DetectInputCodepage tells us
	//              it is; though I've never see it returned even when it was...)
	//  eUTF8 -     if DetectInputCodepage tells us that at least one of the encodings is
	//              UTF-8 (it seems to never have 100% confidence even on files *with the
	//              BOM and even if they were saved by Word, for example).
	//  eAnsi -     if it isn't one of the above two (this is simply so we know how to treat
	//              the data; i.e. use CP_ACP for conversion to wide). Even though this is a
	//              huge assumption on my part, in our CS case, we really don't need to
	//              expect data to be in any form besides these three.
	inline TextEncoding Detect(const char* begin, const char* end);
};


TextEncoding EncodeDetector::Detect(const char* begin, const char* end)
{
	//using value_type = typename std::decay< decltype(*begin) >::type;
	//static_assert(std::is_same<char, value_type>::value, "Detect only from bytes");
	assert(begin && end);
	assert(begin <= end);

	DetectEncodingInfo codePages[MaxCodePages] = {};
	auto scores = MaxCodePages;
	auto length = end - begin;

	if (SUCCEEDED(p->DetectInputCodepage(MLDETECTCP_NONE, 0, const_cast<char*>(begin), &length, codePages, &scores)))
	{
		for (auto i = 0; i < scores; ++i)
		{
			switch (codePages[i].nCodePage)
			{
			case 20127: // ASCII
			case CP_UTF8:
				return TextEncoding::UTF8;
			case CP_UTF16_LE:
				return TextEncoding::UTF16LE;
			case CP_UTF16_BE:
				return TextEncoding::UTF16BE;
			case CP_UTF32_LE:
				return TextEncoding::UTF32LE;
			case CP_UTF32_BE:
				return TextEncoding::UTF32BE;
			}
		}
	} else
	{
		try
		{
			std::wstring_convert<deletable_facet<std::codecvt_utf8_utf16<char16_t>>, char16_t> converter;
			return converter.from_bytes(begin, end).empty() ? TextEncoding::Ansi : TextEncoding::UTF8;
		}
		catch (std::exception&)
		{
			return TextEncoding::Ansi;
		}
	}
	return TextEncoding::Ansi;
}

#endif;