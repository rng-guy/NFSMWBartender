#pragma once

#include <span>
#include <array>
#include <vector>
#include <format>
#include <utility>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <concepts>
#include <filesystem>
#include <string_view>
#include <type_traits>

#include "Globals.hpp"

#include "../Utilities/StreamParser.hpp"
#include "../Utilities/FormatBuffer.hpp"
#include "../Utilities/FlatContainers.hpp"



namespace ConfigParser
{
	// Concepts -------------------------------------------------------------------------------------------------------------------------------------

	namespace Concepts
	{
		template <typename V>
		concept IsPureEnum = StreamParser::Concepts::IsPureEnum<V>;

		template <typename V>
		concept IsPureArithmetic = StreamParser::Concepts::IsPureArithmetic<V>;
		

		template <typename V>
		concept IsBoundsCompatible = (IsPureArithmetic<V> and (not std::same_as<V, bool>));


		template <typename ...Vs>
		concept AreExtractable = (StreamParser::Concepts::AreExtractable<Vs...>);

		template <typename K, typename ...Vs>
		concept AreSectionExtractable = (StreamParser::Concepts::AreSectionExtractable<K, Vs...>);
	}

	



	// Bounds class ---------------------------------------------------------------------------------------------------------------------------------

	template <typename T>
	class Bounds 
	{
	public: // methods

		void Enforce(auto&) const {}
	};



	template <typename T>
	requires Concepts::IsBoundsCompatible<T>
	class Bounds<T>
	{
	private: // members

		std::optional<T> lower;
		std::optional<T> upper;


	public: // methods

		constexpr Bounds
		(
			const std::optional<T>& lower = {},
			const std::optional<T>& upper = {}
		)
			: lower(lower), upper(upper)
		{
			if (not (this->lower and this->upper)) return;
			if (*(this->lower) <= *(this->upper))  return;

			if (not std::is_constant_evaluated())
				ASSERT_UNREACHABLE_THEN(this->lower = this->upper);
			
			else std::abort(); // fails to compile instead
		}


		void Enforce(T& value) const
		{
			if (this->lower and (value < *(this->lower))) value = *(this->lower);
			if (this->upper and (value > *(this->upper))) value = *(this->upper);
		}


		void Enforce(const std::span<T> values) const
		{
			for (T& value : values)
				this->Enforce(value);
		}
	};





	// Parser helpers -------------------------------------------------------------------------------------------------------------------------------

	template <typename T>
	requires Concepts::AreExtractable<T>
	struct ScalarField
	{
	// Members

		T& value;

		[[no_unique_address]] const Bounds<T> limits = {};
	};



	template <typename T, size_t numRows>
	requires Concepts::AreExtractable<T>
	struct ArrayField
	{
	// Members

		std::array<T, numRows>& values;

		std::optional<T> defaultValue; // mutated in "ExtractArrays"

		[[no_unique_address]] const Bounds<T> limits = {};
	};



	template <typename T>
	requires Concepts::AreExtractable<T>
	struct VectorField
	{
	// Members

		std::vector<T>& values;

		[[no_unique_address]] const Bounds<T> limits = {};
	};





	// Parser class ---------------------------------------------------------------------------------------------------------------------------------

	class Parser : protected StreamParser::Parser<>
	{
	private: // aliases

		using Base = StreamParser::Parser<>;


	public: // aliases (for interfaces)

		using Section    = Base::Section;   
		using SectionMap = Base::SectionMap;


	private: // members

		std::filesystem::path currentFilePath;

		FlatContainers::Map<std::filesystem::path, Parser::SectionMap> pathToSectionMap;


	private: // methods

		bool UpdateCurrentFilePath(std::filesystem::path&& newFilePath) 
		{
			if (this->currentFilePath == newFilePath) return false;

			// Return currently parsed file to cache
			if (not this->currentFilePath.empty())
			{
				const auto pairIt = this->pathToSectionMap.find(this->currentFilePath);

				if (pairIt != this->pathToSectionMap.end())
					pairIt->second.swap(this->nameToSection); // section map now empty

				else ASSERT_UNREACHABLE_THEN(this->nameToSection.clear());
			}

			this->currentFilePath = std::move(newFilePath);

			return true;
		}


	public: // members

		size_t sectionCapacityPerFile;
		size_t pairCapacityPerSection;


	public: // methods

		using Base::GetSection;
		using Base::GetSectionMap;


		explicit Parser
		(
			const size_t fileCapacity           = 0,
			const size_t sectionCapacityPerFile = 0,
			const size_t pairCapacityPerSection = 0
		) 
			: sectionCapacityPerFile(sectionCapacityPerFile), pairCapacityPerSection(pairCapacityPerSection)
		{
			this->pathToSectionMap.reserve(fileCapacity);
		}


		// May invalidate retrieved views and pointers
		bool ParseFile
		(
			const std::filesystem::path& root,
			const std::string_view       fileName
		) {
			// Update and check new file path
			if (not this->UpdateCurrentFilePath(root / fileName))
			{
				if constexpr (Globals::loggingEnabled)
				{
					if (not this->currentFilePath.empty())
						Globals::LogPlain("Keep:", fileName);
				}

				return true; // file already loaded
			}

			if (this->currentFilePath.empty()) return true;
	
			// Attempt to create new (empty) cache entry for new file
			const auto [pairIt, isNewPath] = this->pathToSectionMap.try_emplace(this->currentFilePath);

			// Check cache for new file 
			if (not isNewPath)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogPlain("Load:", fileName);

				this->nameToSection.swap(pairIt->second); // existing cache entry now empty

				return true; // file loaded from cache
			}

			// Attempt to open new file
			std::ifstream fileStream(this->currentFilePath);

			if (not fileStream.is_open())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogPlain("Skip:", fileName);

				return false; // file doesn't exist
			}

			// Parse new file
			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain("Parse:", fileName);

			this->ParseStream(fileStream, this->sectionCapacityPerFile, this->pairCapacityPerSection);

			return true; // file exists
		}


		[[nodiscard]] const auto& GetCurrentFilePath() const
		{
			return this->currentFilePath;
		}


		// Invalidates retrieved views and pointers
		void ClearFiles()
		{
			this->currentFilePath .clear();
			this->pathToSectionMap.clear();
			this->nameToSection   .clear();
		}


		// A single row of values
		template <typename ...Vs>
		requires Concepts::AreExtractable<Vs...>
		static bool ExtractScalars
		(
			const Section* const      section,
			const std::string_view    key,
			const ScalarField<Vs>  ...scalars
		) {
			const bool allExtracted = (section and Parser::ExtractValues<Vs...>(section, key, scalars.value...));

			(..., scalars.limits.Enforce(scalars.value));

			return allExtracted;
		}


		// A single row of values
		template <typename ...Vs>
		requires Concepts::AreExtractable<Vs...>
		static bool ExtractScalars
		(
			const Section&            section,
			const std::string_view    key,
			const ScalarField<Vs>  ...scalars
		) {
			return Parser::ExtractScalars<Vs...>(&section, key, scalars...);
		}


		// A single row of values
		template <typename ...Vs>
		requires Concepts::AreExtractable<Vs...>
		bool ExtractScalars
		(
			const std::string_view    sectionName,
			const std::string_view    key,
			const ScalarField<Vs>  ...scalars
		) 
			const
		{
			const Section* const section = this->GetSection(sectionName);
			return this->ExtractScalars<Vs...>(section, key, scalars...);
		}


		// Row(s) with fixed-format key(s)
		template <size_t numRows, typename ...Vs>
		requires Concepts::AreExtractable<Vs...>
		static std::array<bool, numRows> ExtractArrays
		(
			const Section* const                section,
			const std::string_view              defaultKey,
			const std::format_string<size_t>    keyFormat,
			const size_t                        keyStartIndex,
			ArrayField<Vs, numRows>          ...arrays
		) {
			FormatBuffer::Buffer buffer;

			const bool hasFullDefaultRow = (arrays.defaultValue.has_value() and ...);

			// Attempt default-value extraction(s)
			if (section and hasFullDefaultRow and (not defaultKey.empty()))
				Parser::ExtractValues<Vs...>(section, defaultKey, {*(arrays.defaultValue)}...);

			// Attempt row extraction(s)
			std::array<bool, numRows> rowExtracteds = {};

			for (size_t rowID = 0; rowID < numRows; ++rowID)
			{
				if (section)
				{
					const auto key       = buffer.Format(keyFormat, keyStartIndex + rowID);
					rowExtracteds[rowID] = Parser::ExtractValues<Vs...>(section, key, arrays.values[rowID]...);
				}

				if (rowExtracteds[rowID])  continue;
				if (not hasFullDefaultRow) continue;

				// Apply default(s) if extraction failed
				(..., (arrays.values[rowID] = *(arrays.defaultValue)));

				rowExtracteds[rowID] = true; // row now valid
			}

			(..., arrays.limits.Enforce(arrays.values));

			return rowExtracteds;
		}


		// Row(s) with fixed-format key(s)
		template <size_t numRows, typename ...Vs>
		requires Concepts::AreExtractable<Vs...>
		static std::array<bool, numRows> ExtractArrays
		(
			const Section&                      section,
			const std::string_view              defaultKey,
			const std::format_string<size_t>    keyFormat,
			const size_t                        keyStartIndex,
			const ArrayField<Vs, numRows>    ...arrays
		) {
			return Parser::ExtractArrays<numRows, Vs...>(&section, defaultKey, keyFormat, keyStartIndex, arrays...);
		}


		// Row(s) with fixed-format key(s)
		template <size_t numRows, typename ...Vs>
		requires Concepts::AreExtractable<Vs...>
		std::array<bool, numRows> ExtractArrays
		(
			const std::string_view              sectionName,
			const std::string_view              defaultKey,
			const std::format_string<size_t>    keyFormat,
			const size_t                        keyStartIndex,
			const ArrayField<Vs, numRows>    ...arrays
		) 
			const 
		{
			const Section* const section = this->GetSection(sectionName);
			return this->ExtractArrays<numRows, Vs...>(section, defaultKey, keyFormat, keyStartIndex, arrays...);
		}

		
		// All keys and rows of a given section
		template <typename K, typename ...Vs>
		requires Concepts::AreSectionExtractable<K, Vs...>
		static size_t ExtractVectors
		(
			const Section* const     section,
			std::vector<K>&          keys,
			const VectorField<Vs> ...vectors
		) {
			keys                .clear();
			(..., vectors.values.clear());

			const size_t numExtracted = (section) ? Parser::ExtractSection<K, Vs...>(section, keys, vectors.values...) : 0;

			(..., vectors.limits.Enforce(vectors.values));

			return numExtracted;
		}


		// All keys and rows of a given section
		template <typename K, typename ...Vs>
		requires Concepts::AreSectionExtractable<K, Vs...>
		static size_t ExtractVectors
		(
			const Section&           section,
			std::vector<K>&          keys,
			const VectorField<Vs> ...vectors
		) {
			return Parser::ExtractVectors<K, Vs...>(&section, keys, vectors...);
		}


		// All keys and rows of a given section
		template <typename K, typename ...Vs>
		requires Concepts::AreSectionExtractable<K, Vs...>
		size_t ExtractVectors
		(
			const std::string_view    sectionName,
			std::vector<K>&           keys,
			const VectorField<Vs>  ...vectors
		) 
			const
		{
			const Section* const section = this->GetSection(sectionName);
			return this->ExtractVectors<K, Vs...>(section, keys, vectors...);
		}
	};
}