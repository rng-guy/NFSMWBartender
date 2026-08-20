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

#include "..\Utilities\StreamParser.hpp"
#include "..\Utilities\FormatBuffer.hpp"
#include "..\Utilities\FlatContainers.hpp"



namespace ConfigParser
{
	// Concepts -------------------------------------------------------------------------------------------------------------------------------------

	namespace Concepts
	{
		template <typename V>
		concept IsPureArithmetic = StreamParser::Concepts::IsPureArithmetic<V>; // templated to suppress transient includes
		
		template <typename V>
		concept IsBoundsCompatible = (IsPureArithmetic<V> and (not std::same_as<V, bool>));


		using StreamParser::Concepts::AreNonAllocating;

		template <typename ...Vs>
		concept AreParseable = (StreamParser::Concepts::AreParseable<Vs...> and AreNonAllocating<Vs...>);

		template <typename K, typename ...Vs>
		concept AreSectionParseable = (StreamParser::Concepts::AreSectionParseable<K, Vs...> and AreNonAllocating<Vs...>);
	}

	



	// Parser helpers -------------------------------------------------------------------------------------------------------------------------------

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



	template <typename T>
	requires Concepts::AreParseable<T>
	struct Parameter
	{
	// Members

		T& value;

		[[no_unique_address]] const Bounds<T> limits = {};
	};


	template <typename T, size_t numRows>
	requires Concepts::AreParseable<T>
	struct Format
	{
	// Members

		std::array<T, numRows>& values;

		std::optional<T> defaultValue; // mutated in "ParseFormat"

		[[no_unique_address]] const Bounds<T> limits = {};
	};


	template <typename T>
	requires Concepts::AreParseable<T>
	struct User
	{
	// Members

		std::vector<T>& values;

		[[no_unique_address]] const Bounds<T> limits = {};
	};





	// Config-file parser with bounds checking and fixed-format support -----------------------------------------------------------------------------

	class Parser : protected StreamParser::Parser<>
	{
	private: // members

		mutable FormatBuffer::Buffer buffer;

		std::filesystem::path currentFilePath;

		FlatContainers::Map<std::filesystem::path, Parser::Sections> pathToSections;


	private: // methods

		bool UpdateCurrentFilePath(std::filesystem::path&& newFilePath) 
		{
			if (this->currentFilePath == newFilePath) return false;

			// Return currently parsed file to cache
			if (not this->currentFilePath.empty())
			{
				const auto pairIt = this->pathToSections.find(this->currentFilePath);

				if (pairIt != this->pathToSections.end())
					pairIt->second.swap(this->sections); // sections now empty

				else ASSERT_UNREACHABLE_THEN(this->sections.clear());
			}

			this->currentFilePath = std::move(newFilePath);

			return true;
		}


	public: // members

		size_t sectionCapacityPerFile;
		size_t pairCapacityPerSection;


	public: // methods

		explicit Parser
		(
			const size_t fileCapacity           = 0,
			const size_t sectionCapacityPerFile = 0,
			const size_t pairCapacityPerSection = 0
		) 
			: sectionCapacityPerFile(sectionCapacityPerFile), pairCapacityPerSection(pairCapacityPerSection)
		{
			this->pathToSections.reserve(fileCapacity);
		}


		// May invalidate retrieved const char* and string_view
		bool LoadFile
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
			const auto [pairIt, isNewPath] = this->pathToSections.try_emplace(this->currentFilePath);

			// Check cache for new file 
			if (not isNewPath)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogPlain("Load:", fileName);

				this->sections.swap(pairIt->second); // existing cache entry now empty

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


		[[nodiscard]] const auto& GetSections() const
		{
			return this->sections;
		}


		// Invalidates retrieved const char* and string_view
		void ClearFiles()
		{
			this->currentFilePath.clear();
			this->pathToSections .clear();
			this->sections       .clear();
		}


		// Value(s) from parsed file
		template <typename ...Vs>
		requires Concepts::AreParseable<Vs...>
		bool ParseFromFile
		(
			const std::string_view    section,
			const std::string_view    key,
			const Parameter<Vs>    ...parameters
		) 
			const
		{
			const bool areValid = this->GetValues<Vs...>(section, key, parameters.value...);
			(..., parameters.limits.Enforce(parameters.value));

			return areValid;
		}


		// Fixed-format value(s) from parsed file
		template <size_t numRows, typename ...Vs>
		requires Concepts::AreParseable<Vs...>
		std::array<bool, numRows> ParseFormat
		(
			const std::string_view              section,
			const std::string_view              defaultKey,
			const std::format_string<size_t>    keyFormat,
			const size_t                        keyStartIndex,
			Format<Vs, numRows>              ...parameters
		) 
			const 
		{
			const auto foundSection      = this->sections.find(section);
			const bool hasFullDefaultRow = (parameters.defaultValue.has_value() and ...);

			// Parse default value(s)
			if (hasFullDefaultRow and (not defaultKey.empty()) and (foundSection != this->sections.end()))
				this->GetValues<Vs...>(foundSection->second, defaultKey, {*(parameters.defaultValue)}...);

			// Parse each row column-wise
			std::array<bool, numRows> isValidRows = {};

			for (size_t rowID = 0; rowID < numRows; ++rowID)
			{
				// Parse row without default(s) first
				if (foundSection != this->sections.end())
				{
					const auto format  = this->buffer.Format(keyFormat, keyStartIndex + rowID);
					isValidRows[rowID] = this->GetValues<Vs...>(foundSection->second, format, parameters.values[rowID]...);
				}

				// Apply default(s) to invalid row
				if (isValidRows[rowID])    continue;
				if (not hasFullDefaultRow) continue;

				(..., (parameters.values[rowID] = *(parameters.defaultValue)));

				isValidRows[rowID] = true; // row now valid
			}

			(..., parameters.limits.Enforce(parameters.values));

			return isValidRows;
		}

		
		// User-defined key-value pair(s) from parsed file
		template <typename K, typename ...Vs>
		requires Concepts::AreSectionParseable<K, Vs...>
		size_t ParseUser
		(
			const std::string_view    section,
			std::vector<K>&           keys,
			const User <Vs>        ...parameters
		) 
			const
		{
			keys                   .clear();
			(..., parameters.values.clear());

			const size_t numReads = this->GetFullSection<K, Vs...>(section, keys, parameters.values...);
			(..., parameters.limits.Enforce(parameters.values));

			return numReads;
		}
	};
}