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
#include <ranges>
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

	bool IsNotSpace(const unsigned char ch)
	{
		return (not std::isspace(ch));
	}



	void TrimStringLeft(std::string& string)
	{
		string.erase(string.begin(), std::find_if(string.begin(), string.end(), IsNotSpace));
	}



	void TrimStringRight(std::string& string)
	{
		string.erase(std::find_if(string.rbegin(), string.rend(), IsNotSpace).base(), string.end());
	}



	std::string_view TrimViewLeft(std::string_view view)
	{
		view.remove_prefix(std::distance(view.begin(), std::ranges::find_if(view, IsNotSpace)));

		return view;
	}



	std::string_view TrimViewRight(std::string_view view)
	{
		const auto reversed = view | std::views::reverse;
		view.remove_suffix(std::distance(reversed.begin(), std::ranges::find_if(reversed, IsNotSpace)));

		return view;
	}

	

	template <typename T>
	bool ExtractFromString
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
	bool ExtractFromString<std::string>
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

		static bool PreProcessLine(std::string& line) 
		{
			TrimStringLeft(line);

			// Remove (trailing) comment if there is one
			constexpr auto IsComment = [](const char ch) {return (ch == ';');};
			line.erase(std::find_if(line.begin(), line.end(), IsComment), line.end());

			TrimStringRight(line);

			return (not line.empty());
		}


		static bool IsSectionStart(const std::string& line)
		{
			return ((line.front() == '[') and (line.back() == ']'));
		}


		static std::string ExtractName(const std::string& line)
		{
			return line.substr(1, line.length() - 2);
		}


		static std::optional<std::string::size_type> FindUniqueAssign(const std::string& line)
		{
			constexpr char assign = '=';

			const std::string::size_type leftAssign = line.find(assign);

			if (leftAssign == std::string::npos)  return std::nullopt;
			if (leftAssign != line.rfind(assign)) return std::nullopt;

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
				if (not this->PreProcessLine(line)) continue;

				// Check for new section
				if (this->IsSectionStart(line))
				{
					currentSection = &(this->sections[ExtractName(line)]);

					continue;
				}
				else if (not currentSection) continue;

				// Parse key-value pair
				if (const auto foundAssign = this->FindUniqueAssign(line))
				{
					const std::string_view key = TrimViewRight({line.data(), *foundAssign});
					if (key.empty()) continue;

					const std::string_view value = TrimViewLeft({line.data() + (*foundAssign + 1), line.size() - (*foundAssign + 1)});
					if (value.empty()) continue;

					currentSection->try_emplace(std::string(key), std::string(value));
				}
			}
		}


		explicit Ini() = default;

		Ini(std::istream& fileStream)
		{
			this->ParseStream(fileStream);
		}


		template <typename T>
		static bool ExtractFromSection
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
		bool ExtractFromSection
		(
			const std::string_view section,
			const std::string_view key,
			T&                     value
		)
			const
		{
			const auto foundSection = this->sections.find(section);
			if (foundSection == this->sections.end()) return false;

			return this->ExtractFromSection<T>(foundSection->second, key, value);
		}


		template <typename T>
		static size_t ExtractSection
		(
			const Section&            section,
			std::vector<std::string>& keys,
			std::vector<T>&           values
		) {
			T result{};

			size_t numReads = 0;

			keys  .reserve(keys.size()   + section.size());
			values.reserve(values.size() + section.size());

			for (auto const& [key, value] : section)
			{
				if (ExtractFromString<T>(value, result))
				{
					++numReads;

					keys  .push_back(key);
					values.push_back(std::move(result));
				}
			}

			return numReads;
		}


		template <typename T>
		size_t ExtractSection
		(
			const std::string_view    section,
			std::vector<std::string>& keys,
			std::vector<T>&           values
		)
			const
		{
			const auto foundSection = this->sections.find(section);
			if (foundSection == this->sections.end()) return 0;

			return this->ExtractSection<T>(foundSection->second, keys, values);
		}


		void Clear() 
		{
			sections.clear();
		}
	};
}