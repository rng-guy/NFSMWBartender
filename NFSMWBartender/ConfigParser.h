#pragma once

#include <span>
#include <array>
#include <vector>
#include <format>
#include <utility>
#include <fstream>
#include <optional>
#include <concepts>
#include <filesystem>
#include <string_view>

#include "Globals.h"
#include "StreamParser.h"
#include "FlatContainers.h"



namespace ConfigParser
{
	// Concepts -------------------------------------------------------------------------------------------------------------------------------------

	namespace Concepts
	{
		template <typename T>
		concept IsPureArithmetic = StreamParser::Concepts::IsPureArithmetic<T>; // templated to suppress transient includes
		
		template <typename V>
		concept IsBoundsCompatible = (IsPureArithmetic<V> and (not std::same_as<V, bool>));

		using StreamParser::Concepts::AreParseable;
		using StreamParser::Concepts::AreSectionParseable;
	}

	



	// Auxiliary data structures --------------------------------------------------------------------------------------------------------------------

	template <typename T>
	struct Bounds 
	{
	// Methods

		void Enforce(auto&) const {}
	};


	template <typename T>
	requires Concepts::IsBoundsCompatible<T>
	struct Bounds<T>
	{
	// Members

		std::optional<T> lower;
		std::optional<T> upper;


	// Methods

		void Enforce(T& value) const
		{
			const auto& lower = this->lower;
			const auto& upper = this->upper;

			if (lower and (value < *lower)) value = *lower;
			if (upper and (value > *upper)) value = *upper;
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

		std::optional<T> defaultValue;

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





	// Full config-file parser with bounds checking and fixed-format support ------------------------------------------------------------------------

	class Parser : protected StreamParser::Parser<>
	{
	private: // members

		std::filesystem::path currentFile;

		FlatContainers::Map<std::filesystem::path, Parser::Sections> pathToSections;


	private: // methods

		bool UpdateFilePath
		(
			const std::filesystem::path& root,
			const std::string_view       fileName
		) {
			std::filesystem::path newPath = root / fileName;
			if (this->currentFile == newPath) return false;

			this->currentFile = std::move(newPath);

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
			// Check against current path
			if (not this->UpdateFilePath(root, fileName))
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Keep:", fileName);

				return true; // file already loaded
			}

			this->ClearParsedStrings();

			// Attempt to retrieve file from cache
			const auto        [pairIt, isNewPath] = this->pathToSections.try_emplace(this->currentFile);
			Parser::Sections& cachedFileSections  = pairIt->second;

			if (not isNewPath)
			{
				this->sections = cachedFileSections;
				
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Load:", fileName);

				return true; // file already cached
			}
			
			// Attempt to parse new file
			std::ifstream fileStream(this->currentFile);

			if (fileStream.is_open())
			{
				this->ParseStream(fileStream, this->sectionCapacityPerFile, this->pairCapacityPerSection);

				cachedFileSections = this->sections;

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Open:", fileName);

				return true; // new file exists
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>("Skip:", fileName);

			return false; // new file doesn't exist
		}


		[[nodiscard]] const auto& GetCurrentFilePath() const
		{
			return this->currentFile;
		}


		[[nodiscard]] const auto& GetSections() const
		{
			return this->sections;
		}


		void ClearCachedPaths()
		{
			this->pathToSections.clear();
		}


		// Value(s) from parsed file
		template <typename ...Vs>
		requires Concepts::AreParseable<Vs...>
		bool ParseFromFile
		(
			const std::string_view    section,
			const std::string_view    key,
			Parameter<Vs>&&        ...parameters
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
			Format<Vs, numRows>&&            ...parameters
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
					isValidRows[rowID] = this->GetValues<Vs...>
					(
						foundSection->second,
						std::format(keyFormat, keyStartIndex + rowID),
						parameters.values[rowID]...
					);
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
			User<Vs>&&             ...parameters
		) 
			const
		{
			keys.clear();
			(..., parameters.values.clear());

			const size_t numReads = this->GetFullSection<K, Vs...>(section, keys, parameters.values...);
			(..., parameters.limits.Enforce(parameters.values));

			return numReads;
		}
	};
}