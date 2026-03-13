#pragma once

#include <tuple>
#include <array>
#include <ranges>
#include <vector>
#include <string>
#include <istream>
#include <utility>
#include <iterator>
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
		concept IsChar = std::same_as<T, char>;

		template <typename T>
		concept IsModernStringOrView = (std::same_as<T, std::string> or std::same_as<T, std::string_view>);

		template <typename T>
		concept IsAnyStringOrView = (std::same_as<T, const char*> or IsModernStringOrView<T>);

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
		concept AreAllocationFree = ((not std::same_as<Ts, std::string>) and ...);
	}





	// Parsing helpers ------------------------------------------------------------------------------------------------------------------------------

	namespace Details
	{

		constexpr bool IsWhitespace(const char ch) noexcept
		{
			switch (ch)
			{
			case ' ':  // space
			case '\t': // horizontal tab
			case '\n': // line feed
			case '\v': // vertical tab
			case '\f': // form feed
			case '\r': // carriage return
				return true;
			}

			return false;
		}

		
		template <typename T, typename ...Ts>
		requires (Concepts::IsChar<T> and ... and Concepts::IsChar<Ts>)
		consteval bool AreUniqueNonWhitespace
		(
			const T     first,
			const Ts ...rest
		) 
			noexcept
		{
			if (IsWhitespace(first))
				return false;

			else if constexpr (sizeof...(Ts) > 0)
				return (AreUniqueNonWhitespace<Ts...>(rest...) and ... and (first != rest));

			else
				return true;
		}



		constexpr std::string_view GetEnclosed(const std::string_view view) noexcept
		{
			if (view.size() <= 2) return {};

			return view.substr(1, view.length() - 2);
		}



		constexpr std::string_view TrimLeft(const std::string_view view) noexcept
		{
			const size_t numChars = std::distance(view.begin(), std::ranges::find_if_not(view, IsWhitespace));

			return view.substr(numChars);
		}


		constexpr std::string_view TrimRight(const std::string_view view) noexcept
		{
			const auto   reversed = view | std::views::reverse;
			const size_t numChars = std::distance(reversed.begin(), std::ranges::find_if_not(reversed, IsWhitespace));

			return view.substr(0, view.size() - numChars);
		}


		constexpr std::string_view Trim(const std::string_view view) noexcept
		{
			return TrimRight(TrimLeft(view));
		}



		template <size_t numSegments>
		constexpr std::optional<std::array<std::string_view, numSegments>> Split
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

						break;
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

		V result{};

		const auto viewEnd          = source.data() + source.size();
		const auto [readEnd, error] = std::from_chars(source.data(), viewEnd, result);

		if (error != std::errc{}) return false;
		if (readEnd != viewEnd)   return false;

		value = result;

		return true;
	}



	template <>
	constexpr bool ParseFromString<bool>
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
		const char*& value
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
		bool allParsed = false;

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
	requires (Details::AreUniqueNonWhitespace(comment, separator, assign, start, end))
	class Parser
	{
	private:

		static constexpr std::string_view GetContent(const std::string_view line) noexcept
		{
			return Details::Trim(line.substr(0, line.find(comment)));
		}


		template <typename ...Vs>
		requires Concepts::AreParseable<Vs...>
		static bool DispatchToParsing
		(
			const std::string&    source,
			Vs&                ...values
		)
			noexcept(Concepts::AreAllocationFree<Vs...>)
		{
			if constexpr (Concepts::AreSeparatorParseable<Vs...>)
				return ParseFromString<Vs...>(source, separator, values...);

			else
				return ParseFromString(source, values...);
		}



	protected:

		using Section  = FlatContainers::Map<std::string, std::string>;
		using Sections = FlatContainers::Map<std::string, Section>;

		Sections sections;



	public:

		// For external access (e.g. inspection)
		static constexpr char commentStart    = comment;
		static constexpr char valueSeparator  = separator;
		static constexpr char valueAssignment = assign;
		static constexpr char sectionStart    = start;
		static constexpr char sectionEnd      = end;


		// Invalidates retrieved const char* and string_view
		void ParseStream
		(
			std::istream& stream,
			const size_t  sectionCapacity  = 10,
			const size_t  pairCapacityEach = 20
		) {
			std::string line;

			Section* currentSection = nullptr;

			this->sections.reserve(this->sections.size() + sectionCapacity);

			while (std::getline(stream, line))
			{
				if (line.empty()) continue;

				const std::string_view content = this->GetContent(line);
				if (content.empty()) continue;

				// Check for new section
				if (content.starts_with(start))
				{
					if (content.ends_with(end))
					{
						const std::string_view section = Details::Trim(Details::GetEnclosed(content));

						if (not section.empty())
						{
							const auto [pair, wasAdded] = this->sections.try_emplace(section, pairCapacityEach);

							currentSection = &(pair->second);
						}
						else currentSection = nullptr; // empty
					}
					else currentSection = nullptr; // mangled

					continue;
				}
				
				// Check for active section
				if (not currentSection) continue;

				// Parse key-value pair
				const size_t firstAssign = content.find(assign);
				if (firstAssign == std::string_view::npos) continue;

				const std::string_view key = Details::TrimRight(content.substr(0, firstAssign));
				if (key.empty()) continue;
					
				const std::string_view value = Details::TrimLeft(content.substr(firstAssign + 1));
				if (value.empty()) continue;

				currentSection->try_emplace(key, value);
			}
		}


		Parser() = default;

		Parser(std::istream& fileStream)
		{
			this->ParseStream(fileStream);
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

						if constexpr (Concepts::IsModernStringOrView<K>)
							keys.emplace_back(key);

						else
							keys.push_back(key.c_str());

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


		const auto& GetSections() const noexcept
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