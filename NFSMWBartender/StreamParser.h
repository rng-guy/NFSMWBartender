#pragma once

#include <tuple>
#include <array>
#include <limits>
#include <vector>
#include <string>
#include <istream>
#include <utility>
#include <concepts>
#include <optional>
#include <algorithm>
#include <string_view>
#include <type_traits>
#include <system_error>

#include "FlatContainers.h"



namespace StreamParser
{

	// Concepts -------------------------------------------------------------------------------------------------------------------------------------

	namespace Concepts
	{

		template <typename T>
		concept IsLegacyString = std::same_as<T, const char*>;

		template <typename T>
		concept IsModernString = std::same_as<T, std::string>;

		template <typename T>
		concept IsModernStringOrView = (IsModernString<T> or std::same_as<T, std::string_view>);

		template <typename T>
		concept IsAnyStringOrView = (IsLegacyString<T> or IsModernStringOrView<T>);

		template <typename V>
		concept IsPureArithmetic = (std::is_arithmetic_v<V> and std::same_as<V, std::remove_cvref_t<V>>);

		template <typename ...Vs>
		concept AreLineParseable = ((sizeof...(Vs) == 1) and ... and (IsPureArithmetic<Vs> or IsAnyStringOrView<Vs>));

		// Cannot convert views of split source strings to C-style strings
		template <typename ...Vs>
		concept AreSeparatorParseable = ((sizeof...(Vs) > 1) and ... and (IsPureArithmetic<Vs> or IsModernStringOrView<Vs>));

		template <typename ...Vs>
		concept AreParseable = (AreLineParseable<Vs...> or AreSeparatorParseable<Vs...>);

		template <typename K, typename... Vs>
		concept AreSectionParseable = (IsAnyStringOrView<K> and AreParseable<Vs...>);

		// For noexcept equlifiers due to potential allocation(s)
		template <typename ...Ts>
		concept AreAllocationFree = ((not IsModernString<Ts>) and ...);
	}





	// Parsing helpers ------------------------------------------------------------------------------------------------------------------------------

	namespace Details
	{

		inline void SkipByteOrderMark(std::istream& stream)
		{
			if (stream.tellg() != 0) return; // not start of stream

			constexpr size_t markSize = 3; // bytes

			char buffer[markSize];
			stream.read(buffer, markSize);

			// Skip the first three bytes if the UTF-8 BOM is present
			const auto numReads = static_cast<size_t>(stream.gcount());
			if (std::string_view(buffer, numReads) == "\xEF\xBB\xBF") return;

			// Reset stream
			stream.clear();
			stream.seekg(0);
		}



		[[nodiscard]] constexpr bool IsWhitespace(const char ch) noexcept
		{
			switch (ch)
			{
			case  ' ': // space
			case '\t': // horizontal tab
			case '\n': // line feed
			case '\v': // vertical tab
			case '\f': // form feed
			case '\r': // carriage return
				return true;
			}

			return false;
		}

		
		template <char ...chars>
		[[nodiscard]] consteval bool AreUniqueNonWhitespace() noexcept
		{
			std::array<bool, std::numeric_limits<unsigned char>::max() + 1> seen = {};

			const auto IsValid = [&seen](const unsigned char ch) -> bool
			{
				return ((not IsWhitespace(ch)) and (not seen[ch]) and (seen[ch] = true));
			};

			return (IsValid(chars) and ...);
		}



		[[nodiscard]] constexpr std::string_view TrimLeft(const std::string_view view) noexcept
		{
			return {std::find_if_not(view.begin(), view.end(), IsWhitespace), view.end()};
		}


		[[nodiscard]] constexpr std::string_view TrimRight(const std::string_view view) noexcept
		{
			return {view.begin(), std::find_if_not(view.rbegin(), view.rend(), IsWhitespace).base()};
		}


		[[nodiscard]] constexpr std::string_view Trim(const std::string_view view) noexcept
		{
			return TrimRight(TrimLeft(view));
		}



		template <size_t numSegments>
		[[nodiscard]] constexpr std::optional<std::array<std::string_view, numSegments>> Split
		(
			const std::string_view source,
			const char             separator
		) 
			noexcept
		{
			size_t segmentID     = 0;
			size_t startPosition = 0;

			std::array<std::string_view, numSegments> segments;

			while (startPosition <= source.length())
			{
				if (segmentID < numSegments)
				{
					const size_t endPosition = source.find(separator, startPosition);

					if (endPosition == std::string_view::npos)
					{
						segments[segmentID++] = Trim(source.substr(startPosition));
						break; // no more segments left to parse in view
					}

					// The static analyser likes to complain about this despite the preceding bounds check
					segments[segmentID++] = Trim(source.substr(startPosition, endPosition - startPosition));

					startPosition = endPosition + 1;
				}
				else return std::nullopt;
			}

			if (segmentID != numSegments) return std::nullopt;

			return segments;
		}
	}





	// String-parsing functions ---------------------------------------------------------------------------------------------------------------------

	template <typename V>
	requires Concepts::IsPureArithmetic<V>
	inline bool ParseFromString
	(
		const std::string_view source,
		V&                     value
	) 
		noexcept
	{
		if (source.empty()) return false;

		auto result = V();

		const auto viewEnd          = source.data() + source.size();
		const auto [readEnd, error] = std::from_chars(source.data(), viewEnd, result);

		if (error   != std::errc()) return false;
		if (readEnd != viewEnd)     return false;

		value = result;

		return true;
	}


	constexpr bool ParseFromString
	(
		const std::string_view source,
		bool&                  value
	) 
		noexcept
	{
		if (source == "true")
		{
			value = true;
			return true;
		}

		if (source == "false")
		{
			value = false;
			return true;
		}

		return false;
	}



	template <typename V>
	requires Concepts::IsModernStringOrView<V>
	constexpr bool ParseFromString
	(
		const std::string_view source,
		V&                     value
	) 
		noexcept(Concepts::AreAllocationFree<V>)
	{
		value = source;
		return true;
	}


	constexpr bool ParseFromString
	(
		const std::string& source,
		const char*&       value
	) 
		noexcept
	{
		value = source.c_str();
		return true;
	}


	constexpr bool ParseFromString
	(
		const char* const source,
		const char*&      value
	)
		noexcept
	{
		value = source;
		return true;
	}



	template <typename ...Vs>
	requires Concepts::AreSeparatorParseable<Vs...>
	inline bool ParseFromString
	(
		const std::string_view    source,
		const char                separator,
		Vs&                    ...values
	) 
		noexcept(Concepts::AreAllocationFree<Vs...>)
	{
		bool             allParsed   = false;
		constexpr size_t numSegments = sizeof...(Vs);

		if (const auto segments = Details::Split<numSegments>(source, separator))
		{
			[&]<size_t... segmentID>(std::index_sequence<segmentID...>)
			{
				std::tuple<Vs...> candidates;

				allParsed = (ParseFromString((*segments)[segmentID], std::get<segmentID>(candidates)) and ...);

				if (allParsed)
					(..., (values = std::move(std::get<segmentID>(candidates))));
			}
			(std::make_index_sequence<numSegments>{});
		}

		return allParsed;
	}





	// Stream parser --------------------------------------------------------------------------------------------------------------------------------

	template <char comment = ';', char separator = ',', char assign = '=', char start = '[', char end = ']'>
	requires (Details::AreUniqueNonWhitespace<comment, separator, assign, start, end>())
	class Parser
	{
	private:

		[[nodiscard]] static constexpr std::string_view GetContent(const std::string_view line) noexcept
		{
			return Details::Trim(line.substr(0, line.find(comment)));
		}


		[[nodiscard]] static constexpr std::optional<std::string_view> GetSectionName(const std::string_view content) noexcept
		{
			if (content.starts_with(start) and content.ends_with(end))
				return Details::Trim(content.substr(1, content.length() - 2));

			return std::nullopt; // not section
		}


		[[nodiscard]] static constexpr std::optional<std::pair<std::string_view, std::string_view>> GetPair(const std::string_view content) noexcept
		{
			const size_t firstAssign = content.find(assign);
			if (firstAssign == std::string_view::npos) return std::nullopt; // missing delimiter

			const std::string_view key = Details::TrimRight(content.substr(0, firstAssign));
			if (key.empty()) return std::nullopt; // missing key

			const std::string_view value = Details::TrimLeft(content.substr(firstAssign + 1));
			if (value.empty()) return std::nullopt; // missing value

			return std::pair(key, value);
		}


		template <typename ...Vs>
		requires Concepts::AreSeparatorParseable<Vs...>
		static bool DispatchToParsing
		(
			const std::string&    source,
			Vs&                ...values
		)
			noexcept(Concepts::AreAllocationFree<Vs...>)
		{
			return ParseFromString<Vs...>(source, separator, values...);
		}


		template <typename V>
		requires Concepts::AreLineParseable<V>
		static bool DispatchToParsing
		(
			const std::string& source,
			V&                 value
		)
			noexcept(Concepts::AreAllocationFree<V>)
		{
			return ParseFromString(source, value);
		}



	protected:

		using Section  = FlatContainers::Map<std::string, std::string>;
		using Sections = FlatContainers::Map<std::string, Section>;

		Sections sections;



	public:

		constexpr Parser() = default;


		// Invalidates retrieved const char* and string_view
		void ParseStream
		(
			std::istream& stream,
			const size_t  sectionCapacity        = 0,
			const size_t  pairCapacityPerSection = 0
		) {
			std::string line;

			Section* currentSection = nullptr;

			Details::SkipByteOrderMark(stream); // seriously, screw Notepad
			this->sections.reserve(this->sections.size() + sectionCapacity);

			while (std::getline(stream, line))
			{
				if (line.empty()) continue;

				const std::string_view content = this->GetContent(line);
				if (content.empty()) continue; // only whitespace or comment

				if (const auto sectionName = this->GetSectionName(content))
				{
					if (not sectionName->empty())
					{
						const auto [pairIt, isNewName] = this->sections.try_emplace(*sectionName);
						Section&   section             = pairIt->second;

						if (isNewName)
							section.reserve(pairCapacityPerSection);

						currentSection = &section;
					}
					else currentSection = nullptr; // empty section name
				}
				else if (currentSection)
				{
					if (const auto pair = this->GetPair(content))
						currentSection->try_emplace(pair->first, pair->second);
				}
			}
		}


		Parser
		(
			std::istream& fileStream,
			const size_t  sectionCapacity        = 0,
			const size_t  pairCapacityPerSection = 0
		) {
			this->ParseStream(fileStream, sectionCapacity, pairCapacityPerSection);
		}


		template <typename ...Vs>
		requires Concepts::AreParseable<Vs...>
		static bool GetValues
		(
			const Section&            section,
			const std::string_view    key,
			Vs&                    ...values
		) 
			noexcept(Concepts::AreAllocationFree<Vs...>)
		{
			const auto foundKey = section.find(key);
			if (foundKey == section.end()) return false;

			return Parser::DispatchToParsing<Vs...>(foundKey->second, values...);
		}


		template <typename ...Vs>
		requires Concepts::AreParseable<Vs...>
		bool GetValues
		(
			const std::string_view    section,
			const std::string_view    key,
			Vs&                    ...values
		) 
			const noexcept(Concepts::AreAllocationFree<Vs...>)
		{
			const auto foundSection = this->sections.find(section);
			if (foundSection == this->sections.end()) return false;

			return this->GetValues<Vs...>(foundSection->second, key, values...);
		}


		template <typename K, typename ...Vs>
		requires Concepts::AreSectionParseable<K, Vs...>
		static size_t GetFullSection
		(
			const Section&      section,
			std::vector<K>&     keys,
			std::vector<Vs>& ...values
		) {
			size_t numReads = 0;

			keys.reserve(keys.size() + section.size());
			(..., values.reserve(values.size() + section.size()));

			constexpr size_t numColumns = sizeof...(Vs);

			[&]<size_t ...columnIDs>(std::index_sequence<columnIDs...>)
			{
				std::tuple<Vs...> candidates;

				for (const auto& [key, value] : section)
				{
					if (Parser::DispatchToParsing<Vs...>(value, std::get<columnIDs>(candidates)...))
					{
						(..., values.push_back(std::move(std::get<columnIDs>(candidates))));

						if constexpr (Concepts::IsLegacyString<K>)
							keys.push_back(key.c_str());

						else
							keys.emplace_back(key);

						++numReads;
					}
				}
			}
			(std::make_index_sequence<numColumns>{});

			return numReads;
		}


		template <typename K, typename ...Vs>
		requires Concepts::AreSectionParseable<K, Vs...>
		size_t GetFullSection
		(
			const std::string_view    section,
			std::vector<K>&           keys,
			std::vector<Vs>&       ...values
		) 
			const
		{
			const auto foundSection = this->sections.find(section);
			if (foundSection == this->sections.end()) return 0;

			return this->GetFullSection<K, Vs...>(foundSection->second, keys, values...);
		}


		[[nodiscard]] const Sections& GetSections() const noexcept
		{
			return this->sections;
		}


		// Invalidates retrieved const char* and string_view
		void ClearParsedStrings() noexcept
		{
			this->sections.clear();
		}
	};
}