#pragma once

#include <map>
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



namespace ConfigParser
{

	// Concepts -------------------------------------------------------------------------------------------------------------------------------------

	namespace Concepts
	{

		// Bool doesn't require bounds-checking
		template <typename V>
		concept IsBoundsCompatible = (StreamParser::Concepts::IsPureArithmetic<V> and (not std::same_as<V, bool>));

		template <typename V>
		concept IsSingleParseable = StreamParser::Concepts::IsLineParseable<V>;

		template <typename ...Vs>
		concept AreColumnParseable = StreamParser::Concepts::AreSeparatorParseable<Vs...>;

		template <typename K, typename V>
		concept IsUserParseable = StreamParser::Concepts::IsSectionParseable<K, V>;

		template <typename K, typename ...Vs>
		concept AreColumnUserParseable = StreamParser::Concepts::AreSectionSeparatorParseable<K, Vs...>;
	}

	



	// Auxiliary data structures --------------------------------------------------------------------------------------------------------------------

	template <typename T>
	struct Bounds 
	{
		constexpr void Enforce(const auto& values) const noexcept {}
	};


	template <typename T>
	requires Concepts::IsBoundsCompatible<T>
	struct Bounds<T>
	{
		std::optional<T> lower;
		std::optional<T> upper;


		constexpr void Enforce(T& value) const noexcept
		{
			if (this->lower and (value < *(this->lower)))
				value = *(this->lower);

			if (this->upper and (value > *(this->upper)))
				value = *(this->upper);
		}


		constexpr void Enforce(std::span<T> values) const noexcept
		{
			for (T& value : values)
				this->Enforce(value);
		}
	};



	template <typename T>
	struct Parameter
	{
		T& value;

		const Bounds<T> limits = {};
	};


	template <typename T, size_t numRows>
	struct Format
	{
		std::array<T, numRows>& values;

		std::optional<T> defaultValue;

		const Bounds<T> limits = {};
	};


	template <typename T>
	struct User
	{
		std::vector<T>& values;

		const Bounds<T> limits = {};
	};





	// Full config-file parser with bounds checking and fixed-format support ------------------------------------------------------------------------

	class Parser : protected StreamParser::Parser<>
	{
	private:

		std::filesystem::path currentPath;

		std::map<std::filesystem::path, Parser::Sections> pathToSections;


		bool UpdatePath
		(
			const std::filesystem::path& root,
			const std::string_view       file
		) {
			std::filesystem::path newPath = root / file;
			if (this->currentPath == newPath) return false;

			this->currentPath = std::move(newPath);

			return true;
		}



	public:

		bool LoadFile
		(
			const std::filesystem::path& root,
			const std::string_view       file
		) {
			// Check against current path
			if (not this->UpdatePath(root, file))
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Keep:", file);

				return true;
			}

			this->ClearParsedStrings();

			// Attempt to retrieve file from cache
			const auto pathLocation = this->pathToSections.lower_bound(this->currentPath);

			if ((pathLocation != this->pathToSections.end()) and (pathLocation->first == this->currentPath))
			{
				this->sections = pathLocation->second;
				
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Load:", file);

				return true;
			}
			
			// Attempt to parse new file
			std::ifstream fileStream(this->currentPath);

			if (fileStream.is_open())
			{
				this->ParseStream(fileStream);
					
				// Defers path and map construction as much as possible
				this->pathToSections.emplace_hint(pathLocation, this->currentPath, this->sections);

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Open:", file);

				return true;
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>("Skip:", file);

			return false;
		}


		const auto& GetSections() const noexcept
		{
			return this->sections;
		}


		void ClearCachedPaths() noexcept
		{
			this->pathToSections.clear();
		}


		// Single value from string
		template <typename V>
		requires Concepts::IsSingleParseable<V>
		static bool ParseFromString
		(
			const std::string_view source,
			Parameter<V>&&         parameter
		) {
			const bool isValid = StreamParser::ParseFromString(source, parameter.value);

			parameter.limits.Enforce(parameter.value);

			return isValid;
		}


		// Multiple values from delimited string
		template <typename ...Vs>
		requires Concepts::AreColumnParseable<Vs...>
		bool ParseFromString
		(
			const std::string_view    source,
			Parameter<Vs>&&        ...parameters
		) 
			const
		{
			const bool areValid = StreamParser::ParseFromString<Vs...>(source, this->valueSeparator, parameters.value...);

			(..., parameters.limits.Enforce(parameters.value));

			return areValid;
		}


		// Single value from parsed file
		template <typename V>
		requires Concepts::IsSingleParseable<V>
		bool ParseFromFile
		(
			const std::string_view section,
			const std::string_view key,
			Parameter<V>&&         parameter
		) 
			const
		{
			const bool isValid = this->GetValue<V>(section, key, parameter.value);

			parameter.limits.Enforce(parameter.value);

			return isValid;
		}


		// Multi-value row from parsed file
		template <typename ...Vs>
		requires Concepts::AreColumnParseable<Vs...>
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


		// Single-column, fixed-format values from parsed file
		template <size_t numRows, typename V>
		requires Concepts::IsSingleParseable<V>
		std::array<bool, numRows> ParseFormat
		(
			const std::string_view section,
			const std::string_view defaultKey,
			const std::string_view formatKey,
			const size_t           keyStart,
			Format<V, numRows>&&   parameter
		) 
			const
		{
			std::array<bool, numRows> isValids = {};

			size_t     keyIndex     = keyStart;
			const auto foundSection = this->sections.find(section);

			// Parse default (if available)
			if (parameter.defaultValue and (not defaultKey.empty()) and (foundSection != this->sections.end()))
				this->GetValue<V>(foundSection->second, defaultKey, *(parameter.defaultValue));

			for (size_t rowID = 0; rowID < numRows; ++rowID)
			{
				// Parse row without default first
				if (foundSection != this->sections.end())
				{
					isValids[rowID] = this->GetValue<V>
					(
						foundSection->second,
						std::vformat(formatKey, std::make_format_args(keyIndex)),
						parameter.values[rowID]
					);
				}

				++keyIndex;

				// Apply default to invalid row (if available)
				if ((not isValids[rowID]) and parameter.defaultValue)
				{
					parameter.values[rowID] = *(parameter.defaultValue);

					isValids[rowID] = true;
				}
			}

			parameter.limits.Enforce(parameter.values);

			return isValids;
		}


		// Multi-column, fixed-format values from parsed file
		template <size_t numRows, typename ...Vs>
		requires Concepts::AreColumnParseable<Vs...>
		std::array<bool, numRows> ParseFormat
		(
			const std::string_view    section,
			const std::string_view    defaultKey,
			const std::string_view    formatKey,
			const size_t              keyStart,
			Format<Vs, numRows>&&  ...parameters
		) 
			const 
		{
			std::array<std::string_view, numRows> rawRows;

			const bool hasFullDefaultRow = (parameters.defaultValue and ...);

			// Parse default (if available)
			if (hasFullDefaultRow and (not defaultKey.empty()))
				this->ParseFromFile<Vs...>(section, defaultKey, {*(parameters.defaultValue)}...);

			// Retrieve raw row strings by format
			std::array<bool, numRows> isValidRows = this->ParseFormat<numRows, std::string_view>(section, {}, formatKey, keyStart, {rawRows});

			// Parse each row column-wise
			for (size_t rowID = 0; rowID < numRows; ++rowID)
			{
				isValidRows[rowID] = (isValidRows[rowID] and this->ParseFromString<Vs...>(rawRows[rowID], {parameters.values[rowID]}...));

				// Apply default to invalid row (if available)
				if ((not isValidRows[rowID]) and hasFullDefaultRow)
				{
					(..., (parameters.values[rowID] = *(parameters.defaultValue)));

					isValidRows[rowID] = true;
				}
			}

			(..., parameters.limits.Enforce(parameters.values));

			return isValidRows;
		}


		// Single-column, user-defined key-value pairs from parsed file
		template <typename K, typename V>
		requires Concepts::IsUserParseable<K, V>
		size_t ParseUser
		(
			const std::string_view section,
			std::vector<K>&        keys,
			User<V>&&              parameter
		) 
			const
		{
			keys            .clear();
			parameter.values.clear();

			const size_t numReads = this->GetFullSection<K, V>(section, keys, parameter.values);

			parameter.limits.Enforce(parameter.values);

			return numReads;
		}


		// Multi-column, user-defined key-value pairs from parsed file
		template <typename K, typename ...Vs>
		requires Concepts::AreColumnUserParseable<K, Vs...>
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