#pragma once

#include <tuple>
#include <string>
#include <format>
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

		std::string openedFile;

		inipp::Ini<char> parser;
		

		template <size_t numColumns>
		bool ExtractColumns
		(
			const std::string&                   row, 
			std::array<std::string, numColumns>& columns
		) 
			const
		{
			if (this->columnSeparator.empty()) return false;

			size_t end      = 0;
			size_t start    = 0;
			size_t columnID = 0;

			while ((end = row.find(this->columnSeparator, start)) != std::string::npos) 
			{
				if (columnID < numColumns)
				{
					columns[columnID++] = row.substr(start, end - start);
					start = end + this->columnSeparator.length();
				}
				else return false;
			}

			if (columnID < numColumns)
			{
				columns[columnID++] = row.substr(start);
				return (columnID == numColumns);
			}
			else return false;
		}



	public:

		size_t formatIndexOffset = 1;

		std::string columnSeparator  = ",";
		std::string formatDefaultKey = "default";
		

		bool LoadFile
		(
			const std::string& path,
			const std::string& file
		) {
			const std::string fullPath = path + file;

			// Check parsed file
			if (this->openedFile == fullPath)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Cached:", file);

				return true;
			}

			// Attempt to load file
			std::ifstream fileStream(fullPath);

			if (fileStream.is_open())
			{
				this->openedFile = fullPath;

				this->parser.clear();
				this->parser.parse(fileStream);
				this->parser.strip_trailing_comments();

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Loaded:", file);

				return true;
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>("Missing:", file);

			return false;
		}


		const auto& GetSections() const
		{
			return this->parser.sections;
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
			std::array<std::string, numColumns> columns;

			if (this->ExtractColumns<numColumns>(source, columns))
			{
				[&]<size_t... columnIDs>(std::index_sequence<columnIDs...>)
				{
					isValid = (this->ParseFromString<T>(columns[columnIDs], std::move(parameters)) and ...);
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

			constexpr size_t numColumns = sizeof...(T);

			std::array<std::string, numRows>    rows    = {};
			std::array<std::string, numColumns> columns = {};

			// Parse parameters as-is first
			auto isValidRows = this->ParseFormat<numRows, std::string>(section, format, {rows});

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

			if (this->ParseUser<std::string>(section, allKeys, {rows}) > 0)
			{
				constexpr size_t numColumns = sizeof...(T);

				[&]<size_t ...columnIDs>(std::index_sequence<columnIDs...>)
				{
					std::tuple<T...> candidates;

					for (size_t rowID = 0; rowID < rows.size(); ++rowID)
					{
						if (this->ParseFromString<T...>(rows[rowID], {std::get<columnIDs>(candidates), parameters.limits}...))
						{
							(..., parameters.values.push_back(std::get<columnIDs>(candidates)));

							keys.push_back(allKeys[rowID]);
						}
					}
				}
				(std::make_index_sequence<numColumns>{});
			}

			return keys.size();
		}
	};
}
