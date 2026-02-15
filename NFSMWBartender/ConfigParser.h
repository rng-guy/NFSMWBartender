#pragma once

#include <map>
#include <tuple>
#include <string>
#include <ranges>
#include <format>
#include <cctype>
#include <utility>
#include <fstream>
#include <optional>
#include <concepts>
#include <algorithm>

#include "Globals.h"
#include "inipp.h"



namespace ConfigParser
{

	// Parameter data structures --------------------------------------------------------------------------------------------------------------------

	template <typename T>
	struct Bounds
	{
		std::optional<T> lower;
		std::optional<T> upper;


		void Enforce(T& value) const
		{
			if (this->lower and (value < *(this->lower)))
				value = *(this->lower);

			if (this->upper and (value > *(this->upper)))
				value = *(this->upper);
		}
	};



	template <typename T>
	struct Parameter
	{
		T& value;

		const Bounds<T> limits;
	};



	template <typename T, size_t numRows>
	struct Format
	{
		std::array<T, numRows>& values;

		std::optional<T> defaultValue;

		const Bounds<T> limits;
	};



	template <typename T>
	struct User
	{
		std::vector<T>& values;

		const Bounds<T> limits;
	};





	// Configuration-file parser --------------------------------------------------------------------------------------------------------------------

	class Parser
	{
	private:

		inipp::Ini<char> parser;

		std::string currentFullPath;
		
		std::map<std::string, decltype(parser.sections)> fullPathToSections;
		

		static std::string Trim(std::string&& string) 
		{
			// Force conversion to unsigned char to avoid potential UB with certain characters
			const auto IsSpace = [](const unsigned char c) -> bool {return std::isspace(c);};

			// Remove leading whitespace
			string.erase(string.begin(), std::ranges::find_if_not(string, IsSpace));

			// Remove trailing whitespace using reversed iterator
			string.erase(std::ranges::find_if_not(string | std::views::reverse, IsSpace).base(), string.end());

			return std::move(string);
		}


		template <size_t numColumns>
		std::optional<std::array<std::string, numColumns>> ParseColumns(const std::string& row) const
		{
			size_t columnID = 0;

			std::array<std::string, numColumns> columns;

			auto splits = row | std::views::split(this->columnSeparator);

			for (const auto& subrange : splits)
			{
				if (columnID < numColumns)
				{
					// Convert to regular iterator for string construction
					const auto iterator = subrange | std::views::common;

					columns[columnID++] = this->Trim(std::string(iterator.begin(), iterator.end()));
				}
				else return std::nullopt;
			}

			if (columnID != numColumns) return std::nullopt;

			return columns;
		}



	public:

		size_t formatIndexOffset = 1;

		std::string columnSeparator  = ",";
		std::string formatDefaultKey = "default";
		

		bool LoadFile
		(
			const std::string_view path,
			const std::string_view file
		) {
			const std::string fullPath = std::format("{}/{}", path, file);

			// Check current file
			if (this->currentFullPath == fullPath)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Keep:", file);

				return true;
			}

			this->parser.clear();
			this->currentFullPath = fullPath;

			// Attempt to create new cache entry
			const auto& [pair, wasAdded] = this->fullPathToSections.try_emplace(fullPath);

			// Check cached files
			if (not wasAdded)
			{
				this->parser.sections = pair->second;
				
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Load:", file);

				return true;
			}

			// Attempt to open new file
			std::ifstream fileStream(fullPath);

			if (fileStream.is_open())
			{
				this->parser.parse(fileStream);
				this->parser.strip_trailing_comments();

				pair->second = this->parser.sections;

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Open:", file);

				return true;
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>("Skip:", file);

			return false;
		}


		const auto& GetSections() const
		{
			return this->parser.sections;
		}


		void ClearCachedFilePaths()
		{
			this->fullPathToSections.clear();
		}


		// Single value from string
		template <typename T>
		static bool ParseFromString
		(
			const std::string& source,
			Parameter<T>&&     parameter
		) {
			const bool isValid = inipp::extract<char, T>(source, parameter.value);

			parameter.limits.Enforce(parameter.value);

			return isValid;
		}


		// Multiple values from delimited string
		template <typename ...T>
		requires (sizeof...(T) > 1)
		bool ParseFromString
		(
			const std::string&    source,
			Parameter<T>&&     ...parameters
		) {
			bool isValid = false;

			constexpr size_t numColumns = sizeof...(T);
	
			if (const auto columns = this->ParseColumns<numColumns>(source))
			{
				[&]<size_t... columnIDs>(std::index_sequence<columnIDs...>)
				{
					isValid = (this->ParseFromString<T>((*columns)[columnIDs], std::move(parameters)) and ...);
				}
				(std::make_index_sequence<numColumns>{});
			}

			return isValid;
		}


		// Single value from parsed file
		template <typename T>
		bool ParseFromFile
		(
			const std::string& section,
			const std::string& key,
			Parameter<T>&&     parameter
		) {
			bool isValid = false;

			const auto& sections     = this->GetSections();
			const auto  foundSection = sections.find(section);

			if (foundSection != sections.end())
				isValid = inipp::get_value<char>(foundSection->second, key, parameter.value);

			parameter.limits.Enforce(parameter.value);

			return isValid;
		}


		// Multi-value row from parsed file
		template <typename ...T>
		requires (sizeof...(T) > 1)
		bool ParseFromFile
		(
			const std::string&    section,
			const std::string&    key,
			Parameter<T>&&     ...parameters
		) {
			std::string row;

			if (this->ParseFromFile<std::string>(section, key, {row}))
				return this->ParseFromString<T...>(row, std::move(parameters)...);

			return false;
		}


		// Single-column, fixed-format values from parsed file
		template <size_t numRows, typename T>
		std::array<bool, numRows> ParseFormat
		(
			const std::string&   section,
			const std::string&   format,
			Format<T, numRows>&& parameter
		) {
			std::array<bool, numRows> isValids = {};

			size_t formatIndex = this->formatIndexOffset;

			// Parse default (if available)
			if (parameter.defaultValue)
				this->ParseFromFile<T>(section, this->formatDefaultKey, {*(parameter.defaultValue), parameter.limits});

			for (size_t rowID = 0; rowID < numRows; ++rowID)
			{
				// Parse row without default first
				isValids[rowID] = this->ParseFromFile<T>
				(
					section, 
					std::vformat(format, std::make_format_args(formatIndex)), 
					{parameter.values[rowID], parameter.limits}
				);

				// Apply default to invalid row (if available)
				if ((not isValids[rowID]) and parameter.defaultValue)
				{
					parameter.values[rowID] = *(parameter.defaultValue);

					isValids[rowID] = true;
				}

				++formatIndex;
			}

			return isValids;
		}


		// Multi-column, fixed-format values from parsed file
		template <size_t numRows, typename ...T>
		requires (sizeof...(T) > 1)
		std::array<bool, numRows> ParseFormat
		(
			const std::string&      section,
			const std::string&      format,
			Format<T, numRows>&& ...parameters
		) {
			bool hasInvalidRows = false;

			std::array<std::string, numRows> rows;

			// Parse parameters as-is first
			std::array<bool, numRows> isValidRows = this->ParseFormat<numRows, std::string>(section, format, {rows});

			for (size_t rowID = 0; rowID < numRows; ++rowID)
			{
				if (isValidRows[rowID])
					isValidRows[rowID] = this->ParseFromString<T...>(rows[rowID], {parameters.values[rowID], parameters.limits}...);

				hasInvalidRows |= (not isValidRows[rowID]);
			}

			// Parse and apply defaults if necessary (and available)
			if ((hasInvalidRows and ... and parameters.defaultValue))
			{
				this->ParseFromFile<T...>(section, this->formatDefaultKey, {*(parameters.defaultValue), parameters.limits}...);

				// Apply defaults to invalid rows
				for (size_t rowID = 0; rowID < numRows; ++rowID)
				{
					if (not isValidRows[rowID])
					{
						(..., (parameters.values[rowID] = *(parameters.defaultValue)));

						isValidRows[rowID] = true;
					}
				}
			}

			return isValidRows;
		}


		// Single-column, user-defined key-value pairs from parsed file
		template <typename T>
		size_t ParseUser
		(
			const std::string&        section,
			std::vector<std::string>& keys,
			User<T>&&                 parameter
		) {
			keys.clear();
			parameter.values.clear();

			const auto& sections     = this->GetSections();
			const auto  foundSection = sections.find(section);

			if (foundSection != sections.end())
			{
				inipp::get_value<char, T>(foundSection->second, keys, parameter.values);

				// Fuck the std::vector<bool> specialisation
				if constexpr (not std::is_same_v<T, bool>)
				{
					for (T& value : parameter.values)
						parameter.limits.Enforce(value);
				}
			}
		
			return keys.size();
		}


		// Multi-column, user-defined key-value pairs from parsed file
		template <typename ...T>
		requires (sizeof...(T) > 1)
		size_t ParseUser
		(
			const std::string&           section,
			std::vector<std::string>&    keys,
			User<T>&&                 ...parameters
		) {
			keys.clear();
			(..., parameters.values.clear());

			std::vector<std::string> rows;
			std::vector<std::string> allKeys;

			const size_t numRows = this->ParseUser<std::string>(section, allKeys, {rows});

			if (numRows > 0)
			{
				keys.reserve(numRows);
				(..., parameters.values.reserve(numRows));

				constexpr size_t numColumns = sizeof...(T);

				[&]<size_t ...columnIDs>(std::index_sequence<columnIDs...>)
				{
					std::tuple<T...> candidates;

					for (size_t rowID = 0; rowID < numRows; ++rowID)
					{
						if (this->ParseFromString<T...>(rows[rowID], {std::get<columnIDs>(candidates), parameters.limits}...))
						{
							(..., parameters.values.push_back(std::move(std::get<columnIDs>(candidates))));

							keys.push_back(std::move(allKeys[rowID]));
						}
					}
				}
				(std::make_index_sequence<numColumns>{});
			}

			return keys.size();
		}
	};
}