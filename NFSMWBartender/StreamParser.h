#pragma once

#include <map>
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
#include <functional>
#include <string_view>
#include <type_traits>
#include <system_error>



namespace StreamParser
{

	// Concepts -------------------------------------------------------------------------------------------------------------------------------------

	namespace Concepts
	{

		template <typename T>
		concept IsChar = std::same_as<T, char>;

		template <typename T>
		concept IsLegacyString = std::same_as<T, const char*>;

		template <typename T>
		concept IsModernStringOrView = (std::same_as<T, std::string> or std::same_as<T, std::string_view>);

		template <typename T>
		concept IsAnyStringOrView = (IsLegacyString<T> or IsModernStringOrView<T>);

		template <typename V>
		concept IsPureArithmetic = (std::is_arithmetic_v<V> and std::same_as<V, std::remove_cvref_t<V>>);

		template <typename V>
		concept IsLineParseable = (IsAnyStringOrView<V> or IsPureArithmetic<V>);

		template <typename K, typename V>
		concept IsSectionParseable = (IsAnyStringOrView<K> and IsLineParseable<V>);

		// Cannot convert views of split source strings to C-style strings
		template <typename ...Vs>
		concept AreSeparatorParseable = ((sizeof...(Vs) > 1) and ... and (IsLineParseable<Vs> and (not IsLegacyString<Vs>)));

		template <typename K, typename ...Vs>
		concept AreSectionSeparatorParseable = (IsAnyStringOrView<K> and AreSeparatorParseable<Vs...>);

		// For noexcept equlifiers due to potential allocation(s)
		template <typename ...Ts>
		concept AreAllocationFree = ((not std::same_as<Ts, std::string>) and ...);
	}





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

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
			size_t segmentID = 0;

			std::array<std::string_view, numSegments> segments;

			auto splits = source | std::views::split(separator);

			for (const auto& subrange : splits)
			{
				if (segmentID < numSegments)
				{
					// Convert to regular iterator for view construction
					const auto iterator = subrange | std::views::common;

					// The static analyser likes to complain about this despite the bounds check
					segments[segmentID++] = Trim({iterator.begin(), iterator.end()});
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

		const auto viewEnd = source.data() + source.size();

		const auto [readEnd, error] = std::from_chars(source.data(), viewEnd, result);
		if ((error != std::errc{}) or (readEnd != viewEnd)) return false;

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
			return Details::Trim(line.substr(0, line.find_first_of(comment)));
		}


		static constexpr std::optional<std::string_view> GetSectionName(const std::string_view content) noexcept
		{
			if (not content.starts_with(start)) return std::nullopt;
			if (not content.ends_with  (end))   return std::nullopt;

			return content.substr(1, content.length() - 2);
		}


		static constexpr std::optional<size_t> FindUniqueAssign(const std::string_view content) noexcept
		{
			const size_t leftAssign = content.find(assign);

			if (leftAssign == std::string_view::npos) return std::nullopt;
			if (leftAssign != content.rfind(assign))  return std::nullopt;

			return leftAssign;
		}


		template <typename K>
		requires Concepts::IsAnyStringOrView<K>
		static constexpr void PushSectionKey
		(
			const std::string& key, 
			std::vector<K>&    keys
		) {
			keys.emplace_back(key);
		}

	
		template <>
		static constexpr void PushSectionKey<const char*>
		(
			const std::string&        key,
			std::vector<const char*>& keys
		) {
			keys.push_back(key.c_str());
		}



	protected:

		using Section  = std::map<std::string, std::string, std::less<>>;
		using Sections = std::map<std::string, Section,     std::less<>>;

		Sections sections;



	public:

		// For external access (e.g. inspection)
		static constexpr char commentStart    = comment;
		static constexpr char valueSeparator  = separator;
		static constexpr char valueAssignment = assign;
		static constexpr char sectionStart    = start;
		static constexpr char sectionEnd      = end;


		void ParseStream(std::istream& stream) 
		{
			std::string line;

			Section* currentSection = nullptr;

			while (std::getline(stream, line))
			{
				const std::string_view content = this->GetContent(line);
				if (content.empty()) continue;

				// Check for new section
				if (const auto section = this->GetSectionName(content))
				{
					// emplace_hint avoids construction of temporary strings
					auto sectionLocation = this->sections.lower_bound(*section);

					if ((sectionLocation == this->sections.end()) or (sectionLocation->first != *section))
						sectionLocation = this->sections.emplace_hint(sectionLocation, *section, Section{});

					currentSection = &(sectionLocation->second);

					continue;
				}
				else if (not currentSection) continue;

				// Parse key-value pair
				if (const auto foundAssign = this->FindUniqueAssign(content))
				{
					const std::string_view key = Details::TrimRight(content.substr(0, *foundAssign));
					if (key.empty()) continue;
					
					const std::string_view value = Details::TrimLeft(content.substr(*foundAssign + 1));
					if (value.empty()) continue;

					// emplace_hint avoids construction of temporary strings
					const auto keyLocation = currentSection->lower_bound(key);

					if ((keyLocation == currentSection->end()) or (keyLocation->first != key))
						currentSection->emplace_hint(keyLocation, key, value);
				}
			}
		}


		explicit Parser() = default;

		Parser(std::istream& fileStream)
		{
			this->ParseStream(fileStream);
		}


		template <typename V>
		requires Concepts::IsLineParseable<V>
		static bool GetValue
		(
			const Section&         section,
			const std::string_view key,
			V&                     value
		) 
			noexcept(Concepts::AreAllocationFree<V>)
		{
			const auto foundKey = section.find(key);
			if (foundKey == section.end()) return false;

			return ParseFromString(foundKey->second, value);
		}


		template <>
		static bool GetValue<const char*>
		(
			const Section&         section,
			const std::string_view key,
			const char*&           value
		) 
			noexcept
		{
			const auto foundKey = section.find(key);
			if (foundKey == section.end()) return false;

			value = (foundKey->second).c_str();

			return true;
		}


		template <typename V>
		requires Concepts::IsLineParseable<V>
		bool GetValue
		(
			const std::string_view section,
			const std::string_view key,
			V&                     value
		)
			const noexcept(Concepts::AreAllocationFree<V>)
		{
			const auto foundSection = this->sections.find(section);
			if (foundSection == this->sections.end()) return false;

			return this->GetValue<V>(foundSection->second, key, value);
		}
		

		template <typename ...Vs>
		requires Concepts::AreSeparatorParseable<Vs...>
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

			return ParseFromString<Vs...>(foundKey->second, separator, values...);
		}


		template <typename ...Vs>
		requires Concepts::AreSeparatorParseable<Vs...>
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


		template <typename K, typename V>
		requires Concepts::IsSectionParseable<K, V>
		static size_t GetFullSection
		(
			const Section&  section,
			std::vector<K>& keys,
			std::vector<V>& values
		) {
			size_t numReads = 0;

			keys  .reserve(keys.size()   + section.size());
			values.reserve(values.size() + section.size());

			for (auto const& [key, value] : section)
			{
				V result{};

				if (ParseFromString(value, result))
				{
					values.push_back(std::move(result));
					Parser::PushSectionKey(key, keys);

					++numReads;
				}
			}

			return numReads;
		}


		template <typename K, typename V>
		requires Concepts::IsSectionParseable<K, V>
		size_t GetFullSection
		(
			const std::string_view section,
			std::vector<K>&        keys,
			std::vector<V>&        values
		)
			const
		{
			const auto foundSection = this->sections.find(section);
			if (foundSection == this->sections.end()) return 0;

			return this->GetFullSection<K, V>(foundSection->second, keys, values);
		}


		template <typename K, typename ...Vs>
		requires Concepts::AreSectionSeparatorParseable<K, Vs...>
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
					if (ParseFromString<Vs...>(value, separator, std::get<columnIDs>(candidates)...))
					{
						(..., values.push_back(std::move(std::get<columnIDs>(candidates))));
						Parser::PushSectionKey(key, keys);

						++numReads;
					}
				}
			}
			(std::make_index_sequence<numColumns>{});

			return numReads;
		}


		template <typename K, typename ...Vs>
		requires Concepts::AreSectionSeparatorParseable<K, Vs...>
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


		// Invalidates any retrieved const char* and string_view values
		void ClearParsedStrings() noexcept
		{
			this->sections.clear();
		}
	};
}