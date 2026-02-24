#pragma once

// Based on "inipp" parsing library by Matthias C. M. Troffaes
// Copyright (c) 2017-2020 Matthias C. M. Troffaes (original code)

/*
	ORIGINAL LICENSE

	MIT License

	Copyright (c) 2017-2020 Matthias C. M. Troffaes

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <optional>
#include <iostream>
#include <algorithm>
#include <string_view>



namespace inipp
{

	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	inline bool IsNotSpace(const unsigned char ch)
	{
		return (not std::isspace(ch));
	}



	inline void TrimLeft(std::string& string)
	{
		string.erase(string.begin(), std::find_if(string.begin(), string.end(), IsNotSpace));
	}



	inline void TrimRight(std::string& string)
	{
		string.erase(std::find_if(string.rbegin(), string.rend(), IsNotSpace).base(), string.end());
	}



	template <typename T>
	inline bool ExtractFromString
	(
		const std::string& string,
		T&                 value
	) {
		if (string.empty()) return false;

		char tail  {};
		T    result{};

		std::istringstream stream(string);

		// String must contain convertible char sequence
		if (not (stream >> std::boolalpha >> result)) return false;

		// Remainder of string must be empty or whitespace
		if (stream >> tail) return false;

		value = std::move(result);
		return true;
	}



	template <>
	inline bool ExtractFromString<std::string>
	(
		const std::string& string,
		std::string&       value
	) {
		value = string;
		return true;
	}





	// Base file parser -----------------------------------------------------------------------------------------------------------------------------

	class Ini
	{
	private:

		static bool IsSectionStart(const char ch) {return (ch == '[');}
		static bool IsSectionEnd  (const char ch) {return (ch == ']');}


		static void TrimComment(std::string& string) 
		{
			constexpr auto IsComment = [](const char ch) {return (ch == ';');};
			string.erase(std::find_if(string.begin(), string.end(), IsComment), string.end());
		}


		static std::optional<std::string::size_type> FindUniqueAssign(const std::string& string)
		{
			constexpr char assign = '=';

			const std::string::size_type leftAssign = string.find(assign);

			if (leftAssign == std::string::npos)    return std::nullopt;
			if (leftAssign != string.rfind(assign)) return std::nullopt;

			return leftAssign;
		}



	public:

		using Section  = std::map<std::string, std::string, std::less<>>;
		using Sections = std::map<std::string, Section,     std::less<>>;


		Sections sections;


		void ParseStream(std::istream& stream) 
		{
			std::string line;

			Section* currentSection = nullptr;

			while (std::getline(stream, line))
			{
				TrimLeft (line);
				this->TrimComment(line);
				TrimRight(line);

				if (line.empty()) continue;

				// If line contains valid section, update current section
				if (this->IsSectionStart(line.front()))
				{
					if (this->IsSectionEnd(line.back()))
						currentSection = &(this->sections[line.substr(1, line.length() - 2)]);

					continue;
				}
				else if (not currentSection) continue;

				// If line contains valid key-value pair, append to section
				if (const auto foundAssign = this->FindUniqueAssign(line))
				{
					// Most lines should be valid, so we construct stings right away
					std::string key   = line.substr(0, *foundAssign);
					std::string value = line.substr(*foundAssign + 1);

					TrimRight(key);
					TrimLeft (value);

					if ((not key.empty()) and (not value.empty()))
						currentSection->try_emplace(std::move(key), std::move(value));
				}
			}
		}


		explicit Ini() = default;

		Ini(std::istream& fileStream)
		{
			this->ParseStream(fileStream);
		}


		template <typename T>
		static bool ExtractByKey
		(
			const Section&         section,
			const std::string_view key,
			T&                     value
		) {
			const auto foundKey = section.find(key);
			if (foundKey == section.end()) return false;

			return ExtractFromString<T>(foundKey->second, value);
		}


		template <typename T>
		static size_t ExtractContents
		(
			const Section&            section,
			std::vector<std::string>& keys,
			std::vector<T>&           values
		) {
			T result{};

			keys  .reserve(keys.size()   + section.size());
			values.reserve(values.size() + section.size());

			for (auto const& [key, value] : section)
			{
				if (ExtractFromString<T>(value, result))
				{
					keys  .push_back(key);
					values.push_back(std::move(result));
				}
			}

			return values.size();
		}


		void Clear() 
		{
			sections.clear();
		}
	};
}