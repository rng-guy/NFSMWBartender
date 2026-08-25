#pragma once

#include <span>
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

#include "FlatContainers.hpp"



namespace StreamParser
{
	// Concepts -------------------------------------------------------------------------------------------------------------------------------------

	namespace Concepts
	{
		template <typename T>
		concept IsLegacyString = std::same_as<T, const char*>;

		template <typename T>
		concept IsOwningString = std::same_as<T, std::string>;

		template <typename T>
		concept IsOwningStringOrView = (IsOwningString<T> or std::same_as<T, std::string_view>);

		template <typename T>
		concept IsTerminatedString = (IsLegacyString<T> or IsOwningString<T>);


		template <typename T>
		concept IsAnyStringOrView = (IsLegacyString<T> or IsOwningStringOrView<T>);

		template <typename V>
		concept IsPureEnum = (std::is_enum_v<V> and std::same_as<V, std::remove_cvref_t<V>>);

		template <typename V>
		concept IsPureArithmetic = (std::is_arithmetic_v<V> and std::same_as<V, std::remove_cvref_t<V>>);


		template <typename ...Vs>
		concept AreExtractable = ((sizeof...(Vs) > 0) and ... and (IsAnyStringOrView<Vs> or IsPureEnum<Vs> or IsPureArithmetic<Vs>));

		template <typename S, typename ...Vs>
		concept AreCompatible = (IsTerminatedString<S> or (not (IsTerminatedString<Vs> or ...)));

		template <typename K, typename ...Vs>
		concept AreSectionExtractable = (IsAnyStringOrView<K> and AreExtractable<Vs...>);

		template <typename ...Vs>
		concept AreNonAllocating = ((not IsOwningString<Vs>) and ...);
	}





	// Parsing helpers ------------------------------------------------------------------------------------------------------------------------------

	namespace Details
	{
		inline void SkipByteOrderMark(std::istream& stream)
		{
			if (stream.tellg() != 0) return; // not start of stream

			constexpr size_t markSize = 3; // bytes

			std::array<char, markSize> buffer = {};
			stream.read(buffer.data(), markSize);

			// Skip the first three bytes if the UTF-8 BOM is present
			const size_t numReads = static_cast<size_t>(stream.gcount());
			if (std::string_view(buffer.data(), numReads) == "\xEF\xBB\xBF") return;

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

			const auto IsUniqueNonWhitespace = [&seen](const unsigned char ch) -> bool
			{
				if (IsWhitespace(ch)) return false;
				if (seen[ch])         return false;

				seen[ch] = true;

				return true;
			};

			return (IsUniqueNonWhitespace(chars) and ...);
		}



		[[nodiscard]] inline std::string_view TrimLeft(const std::string_view view) noexcept
		{
			const auto startIt = std::find_if_not(view.begin(), view.end(), IsWhitespace);
			return {startIt, view.end()};
		}


		[[nodiscard]] inline std::string_view TrimRight(const std::string_view view) noexcept
		{
			const auto endIt = std::find_if_not(view.rbegin(), view.rend(), IsWhitespace);
			return {view.begin(), endIt.base()};
		}


		[[nodiscard]] inline std::string_view Trim(const std::string_view view) noexcept
		{
			return TrimRight(TrimLeft(view));
		}
	}





	// String-extraction functions ------------------------------------------------------------------------------------------------------------------

	template <typename V>
	requires Concepts::IsPureArithmetic<V>
	inline bool ExtractFromString
	(
		const std::string_view source,
		V&                     value
	) 
		noexcept
	{
		if (source.empty()) return false;

		auto result = V();

		const char* const viewBegin = source.data();
		const char* const viewEnd   = viewBegin + source.size();

		const auto [readEnd, error] = std::from_chars(viewBegin, viewEnd, result);

		if (error   != std::errc()) return false;
		if (readEnd != viewEnd)     return false;

		value = result;

		return true;
	}


	inline bool ExtractFromString
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
	requires Concepts::IsPureEnum<V>
	inline bool ExtractFromString
	(
		const std::string_view source,
		V&                     value
	)
		noexcept
	{
		auto result = std::underlying_type_t<V>();

		if (not ExtractFromString(source, result)) return false;

		value = static_cast<V>(result);

		return true;
	}



	template <typename V>
	requires Concepts::IsOwningStringOrView<V>
	inline bool ExtractFromString
	(
		const std::string_view source,
		V&                     value
	) 
		noexcept(Concepts::AreNonAllocating<V>)
	{
		value = source;

		return true;
	}


	inline bool ExtractFromString
	(
		const std::string& source,
		const char*&       value
	) 
		noexcept
	{
		value = source.c_str();

		return true;
	}


	inline bool ExtractFromString
	(
		const char* const source,
		const char*&      value
	)
		noexcept
	{
		value = source;

		return true;
	}



	template <typename S, typename ...Vs>
	requires (Concepts::IsAnyStringOrView<S> and Concepts::AreCompatible<S, Vs...> and Concepts::AreExtractable<Vs...>)
	inline bool ExtractFromStrings
	(
		const std::span<const S>    sources,
		Vs&                      ...values
	) 
		noexcept(Concepts::AreNonAllocating<Vs...>)
	{
		constexpr size_t numSegments = sizeof...(Vs);
		if (numSegments != sources.size()) return false;

		const auto ExtractFromSegments = [&]<size_t ...segmentIDs>(const std::index_sequence<segmentIDs...>) -> bool
		{
			std::tuple<Vs...> candidates;

			const bool allExtracted = (ExtractFromString(sources[segmentIDs], std::get<segmentIDs>(candidates)) and ...);

			if (allExtracted)
				(..., (values = std::move(std::get<segmentIDs>(candidates))));

			return allExtracted;
		};

		return ExtractFromSegments(std::make_index_sequence<numSegments>());
	}





	// Stream parser --------------------------------------------------------------------------------------------------------------------------------

	template <char comment = ';', char separator = ',', char assign = '=', char start = '[', char end = ']'>
	requires (Details::AreUniqueNonWhitespace<comment, separator, assign, start, end>())
	class Parser
	{
	protected: // aliases

		using Key    = std::string;
		using Name   = std::string;
		using Values = std::vector<std::string>;


	public: // aliases (for interfaces)

		using Section    = FlatContainers::Map<Key,  Values>;
		using SectionMap = FlatContainers::Map<Name, Section>;


	protected: // members

		SectionMap nameToSection;


	private: // methods

		[[nodiscard]] static std::string_view GetContent(const std::string_view line) noexcept
		{
			return Details::Trim(line.substr(0, line.find(comment)));
		}


		[[nodiscard]] static std::optional<std::string_view> GetSectionName(const std::string_view content) noexcept
		{
			if (not content.starts_with(start)) return std::nullopt;
			if (not content.ends_with  (end))   return std::nullopt;

			return Details::Trim(content.substr(1, content.length() - 2));
		}


		[[nodiscard]] static bool SplitValue
		(
			const std::string_view         value,
			std::vector<std::string_view>& segments
		) {
			segments.clear();

			size_t startPosition = 0;

			while (startPosition <= value.length())
			{
				const size_t endPosition    = value.find(separator, startPosition);
				const bool   isFinalSegment = (endPosition == std::string_view::npos);
				const size_t segmentLength  = (isFinalSegment) ? std::string_view::npos : (endPosition - startPosition);

				const std::string_view segment = Details::Trim(value.substr(startPosition, segmentLength));
				if (segment.empty()) return false;

				segments.push_back(segment);
				if (isFinalSegment) break;

				startPosition = endPosition + 1;
			}

			return true;
		}


		[[nodiscard]] static std::optional<std::string_view> GetKeyAndSplitValue
		(
			const std::string_view         content,
			std::vector<std::string_view>& segments
		) {
			const size_t firstAssign = content.find(assign);
			if (firstAssign == std::string_view::npos) return std::nullopt; // missing delimiter

			const std::string_view key = Details::TrimRight(content.substr(0, firstAssign));
			if (key.empty()) return std::nullopt; // missing key

			const std::string_view value = Details::TrimLeft(content.substr(firstAssign + 1));
			if (value.empty()) return std::nullopt; // missing value(s)

			if (not Parser::SplitValue(value, segments)) return std::nullopt; // empty segment(s)

			return key;
		}


	public: // methods

		constexpr Parser() noexcept = default;


		// Invalidates retrieved views and pointers
		void ParseStream
		(
			std::istream& stream,
			const size_t  sectionCapacity        = 0,
			const size_t  pairCapacityPerSection = 0
		) {
			std::string line;
		
			Section* currentSection = nullptr;

			std::vector<std::string_view> segments;

			this->nameToSection.reserve(this->nameToSection.size() + sectionCapacity);

			Details::SkipByteOrderMark(stream); // man, screw Notepad

			while (std::getline(stream, line))
			{
				if (line.empty()) continue;

				const std::string_view content = this->GetContent(line);
				if (content.empty()) continue; // only whitespace or comment

				// Check whether the line content defines a new section
				if (const auto sectionName = this->GetSectionName(content))
				{
					if (not sectionName->empty())
					{
						const auto [pairIt, isNewName] = this->nameToSection.try_emplace(*sectionName);
						Section&   section             = pairIt->second;

						if (isNewName)
							section.reserve(pairCapacityPerSection);

						currentSection = &section;
					}
					else currentSection = nullptr;

					continue; // section updated
				}
				
				// Attempt to parse the line content as a key-value pair
				if (not currentSection) continue; // no active section

				const auto key = this->GetKeyAndSplitValue(content, segments);
				if (not key) continue; // invalid key or value(s)

				const auto [pairIt, isNewPair] = currentSection->try_emplace(*key);
				if (not isNewPair) continue; // key already exists

				Values& values = pairIt->second;
				values.reserve(segments.size());

				for (const auto& segment : segments)
					values.emplace_back(segment);
			}
		}


		explicit Parser
		(
			std::istream& fileStream,
			const size_t  sectionCapacity        = 0,
			const size_t  pairCapacityPerSection = 0
		) {
			this->ParseStream(fileStream, sectionCapacity, pairCapacityPerSection);
		}


		[[nodiscard]] const Section* GetSection(const std::string_view sectionName) const
		{
			const auto foundName = this->nameToSection.find(sectionName);
			if (foundName == this->nameToSection.end()) return nullptr;

			return &(foundName->second);
		}


		template <typename ...Vs>
		requires Concepts::AreExtractable<Vs...>
		static bool ExtractValues
		(
			const Section* const      section,
			const std::string_view    key,
			Vs&                    ...values
		)
			noexcept(Concepts::AreNonAllocating<Vs...>)
		{
			if (not section) return false;

			const auto foundKey = section->find(key);
			if (foundKey == section->end()) return false;

			return ExtractFromStrings<std::string, Vs...>(foundKey->second, values...);
		}


		template <typename ...Vs>
		requires Concepts::AreExtractable<Vs...>
		static bool ExtractValues
		(
			const Section&            section,
			const std::string_view    key,
			Vs&                    ...values
		) 
			noexcept(Concepts::AreNonAllocating<Vs...>)
		{
			return Parser::ExtractValues<Vs...>(&section, key, values...);
		}


		template <typename ...Vs>
		requires Concepts::AreExtractable<Vs...>
		bool ExtractValues
		(
			const std::string_view    sectionName,
			const std::string_view    key,
			Vs&                    ...values
		) 
			const noexcept(Concepts::AreNonAllocating<Vs...>)
		{
			const Section* const section = this->GetSection(sectionName);
			return this->ExtractValues<Vs...>(section, key, values...);
		}


		template <typename K, typename ...Vs>
		requires Concepts::AreSectionExtractable<K, Vs...>
		static size_t ExtractSection
		(
			const Section* const    section,
			std::vector<K>&         keys,
			std::vector<Vs>&     ...values
		) {
			size_t numExtracted = 0;

			if (not section) return numExtracted;

			keys        .reserve(keys  .size() + section->size());
			(..., values.reserve(values.size() + section->size()));

			const auto ExtractValues = [&](auto&& ...candidates) -> void
			{
				for (const auto& [key, strings] : *section)
				{
					if (not ExtractFromStrings<std::string, Vs...>(strings, candidates...)) continue;

					(..., values.push_back(std::move(candidates))); // safe, as all parsed

					if constexpr (Concepts::IsLegacyString<K>)
						keys.push_back(key.c_str());

					else keys.emplace_back(key);

					++numExtracted;
				}
			};

			std::apply(ExtractValues, std::tuple<Vs...>());

			return numExtracted;
		}


		template <typename K, typename ...Vs>
		requires Concepts::AreSectionExtractable<K, Vs...>
		static size_t ExtractSection
		(
			const Section&      section,
			std::vector<K>&     keys,
			std::vector<Vs>& ...values
		) {
			return Parser::ExtractSection<K, Vs...>(&section, keys, values...);
		}


		template <typename K, typename ...Vs>
		requires Concepts::AreSectionExtractable<K, Vs...>
		size_t ExtractSection
		(
			const std::string_view    sectionName,
			std::vector<K>&           keys,
			std::vector<Vs>&       ...values
		) 
			const
		{
			const Section* const section = this->GetSection(sectionName);
			return this->ExtractSection<K, Vs...>(section, keys, values...);
		}


		[[nodiscard]] const SectionMap& GetSectionMap() const noexcept
		{
			return this->nameToSection;
		}


		// Invalidates retrieved views and pointers
		void ClearSectionMap() noexcept
		{
			this->nameToSection.clear();
		}
	};
}