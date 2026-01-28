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
		const std::optional<T> lower = {};
		const std::optional<T> upper = {};


		void Enforce(T& value) const
		{
			if (this->lower and (value < this->lower.value()))
				value = this->lower.value();

			if (this->upper and (value > this->upper.value()))
				value = this->upper.value();
		}
	};



	template <typename T>
	struct Parameter
	{
		T& value;

		const Bounds<T> limits = {};
	};



	template <typename T, size_t numRows>
	struct FormatParameter
	{
		std::array<T, numRows>& values;

		std::optional<T> defaultValue = {};
		const Bounds<T>  limits       = {};
	};



	template <typename T>
	struct UserParameter
	{
		std::vector<T>& values;

		const Bounds<T> limits = {};
	};





	// Configuration-file parser --------------------------------------------------------------------------------------------------------------------

	class Parser
	{
	private:

		std::string iniFile;

		inipp::Ini<char> parser;
		

		void ExtractColumns
		(
			std::string               row,
			std::vector<std::string>& columns
		) 
			const
		{
			size_t position;
			columns.clear();

			while ((position = row.find(this->columnSeparator)) != std::string::npos)
			{
				columns.push_back(row.substr(0, position));
				row.erase(0, position + this->columnSeparator.length());
			}

			columns.push_back(row);
		}



	public:

		size_t formatIndexOffset = 1;

		std::string columnSeparator = ",";
		std::string formatDefaultKey = "default";
		

		bool LoadFile
		(
			const std::string& path,
			const std::string& file
		) {
			const std::string fullPath = path + file;

			if (this->iniFile == fullPath) return true;

			std::ifstream fileStream(fullPath);
			if (not fileStream.is_open()) return false;

			this->parser.clear();
			this->iniFile = fullPath;
			this->parser.parse(fileStream);
			this->parser.strip_trailing_comments();

			return true;
		}


		bool LoadFileWithLog
		(
			const std::string& path, 
			const std::string& file
		) {
			const bool hasLoaded = this->LoadFile(path, file);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent((hasLoaded) ? "Loaded:" : "Missing:", file);

			return hasLoaded;
		}


		const auto& GetSections() const
		{
			return this->parser.sections;
		}


		// For single values from a string
		template <typename T>
		static bool ParseFromString
		(
			const std::string& source,
			T&                 value,
			const Bounds<T>&   limits  = {}
		) {
			const bool readSuccess = inipp::extract<char, T>(source, value);

			limits.Enforce(value);

			return readSuccess;
		}


		// For single values from parsed file
		template <typename T>
		bool Parse
		(
			const std::string& section,
			const std::string& key,
			T&                 value,
			const Bounds<T>&   limits   = {}
		) {
			const bool readSuccess = inipp::get_value<char>(this->parser.sections[section], key, value);

			limits.Enforce(value);

			return readSuccess;
		}


		template <typename T>
		bool Parse
		(
			const std::string& section,
			const std::string& key,
			Parameter<T>&&     parameter
		) {
			return this->Parse<T>(section, key, parameter.value, parameter.lowerBound, parameter.upperBound);
		}


		// For fixed-format keys from parsed file
		template <typename T, size_t numRows>
		size_t ParseFormat
		(
			const std::string&      section,
			const std::string&      format,
			std::array<T, numRows>& values,
			std::optional<T>        defaultValue = {},
			const Bounds<T>&        limits       = {}
		) {
			std::string key;

			size_t numReads    = 0;
			size_t formatIndex = this->formatIndexOffset;

			if (defaultValue)
				this->Parse<T>(section, this->formatDefaultKey, defaultValue.value(), limits);

			for (auto& value : values)
			{
				if (defaultValue) 
					value = defaultValue.value();

				key       = std::vformat(format, std::make_format_args(formatIndex));
				numReads += this->Parse<T>(section, key, value, limits);

				++formatIndex;
			}

			return numReads;
		}


		template <typename T, size_t numRows>
		size_t ParseFormat
		(
			const std::string&            section,
			const std::string&            format,
			FormatParameter<T, numRows>&& parameter
		) {
			return this->ParseFormat<T, numRows>(section, format, parameter.values,parameter.defaultValue, parameter.limits);
		}


		// For user-defined keys from parsed file
		template <typename T>
		size_t ParseUser
		(
			const std::string&        section,
			std::vector<std::string>& keys,
			std::vector<T>&           values,
			const Bounds<T>&          limits   = {}
		) {
			keys  .clear();
			values.clear();

			const size_t numReads = inipp::get_value<char, T>(this->parser.sections[section], keys, values);

			// Fuck the std::vector<bool> specialisation
			if constexpr (not std::is_same_v<T, bool>)
			{
				for (T& value : values)
					limits.Enforce(value);
			}

			return numReads;
		}


		template <typename T>
		size_t ParseUser
		(
			const std::string&        section,
			std::vector<std::string>& keys,
			UserParameter<T>&&        parameter
		) {
			return this->ParseUser<T>(section, keys, parameter.values, parameter.limits);
		}


		// For single parameter rows
		template <typename ...T>
		bool ParseRow
		(
			const std::string&    section,
			const std::string&    key,
			Parameter<T>&&     ...parameters
		) {
			constexpr size_t numColumns = sizeof...(T);

			std::string              row;
			std::vector<std::string> columns;

			this->Parse<std::string>(section, key, row);
			this->ExtractColumns(row, columns);

			bool isValid = false;

			if (columns.size() == numColumns)
			{
				[&]<size_t... columnIDs>(std::index_sequence<columnIDs...>)
				{
					isValid = (this->ParseFromString<T>(columns[columnIDs], parameters.value, parameters.limits) and ...);
				}
				(std::make_index_sequence<numColumns>{});
			}

			return isValid;
		}


		// For user-defined tables from parsed file
		template <typename ...T>
		size_t ParseTable
		(
			const std::string&           section,
			std::vector<std::string>&    keys,
			UserParameter<T>&&        ...parameters
		) {
			constexpr size_t numColumns = sizeof...(T);

			std::vector<std::string> rows;
			std::vector<std::string> columns;
			std::vector<std::string> rawKeys;

			this->ParseUser<std::string>(section, rawKeys, rows);

			keys.clear();
			(parameters.values.clear(), ...);

			for (size_t rowID = 0; rowID < rows.size(); ++rowID)
			{
				this->ExtractColumns(rows[rowID], columns);

				if (columns.size() != numColumns) continue;

				size_t columnID   = 0;
				bool   isValidRow = true;

				(
					[&]
					{
						T value = T();

						isValidRow &= this->ParseFromString<T>(columns[columnID], value, parameters.limits);

						parameters.values.push_back(std::move(value));
					}
				(), ...);

				if (isValidRow)
					keys.push_back(rawKeys[rowID]);
			
				else 
					(parameters.values.pop_back(), ...);	
			}

			return keys.size();
		}


		// For fixed-format tables from parsed file
		template <size_t numRows, typename ...T>
		std::array<bool, numRows> ParseTable
		(
			const std::string&               section,
			const std::string&               format,
			FormatParameter<T, numRows>&& ...parameters
		) {
			constexpr size_t numColumns = sizeof...(T);

			std::array<std::string, numRows> tableRows;
			std::array<bool,        numRows> isValidRow;

			std::vector<std::string> columns;
			
			// Parse parameters without defaults first
			this->ParseFormat<std::string>(section, format, tableRows);

			for (size_t rowID = 0; rowID < numRows; ++rowID)
			{
				this->ExtractColumns(tableRows[rowID], columns);

				isValidRow[rowID] = (columns.size() == numColumns);

				if (isValidRow[rowID])
				{
					[&]<size_t... columnIDs>(std::index_sequence<columnIDs...>)
					{
						isValidRow[rowID] = (this->ParseFromString<T>(columns[columnIDs], parameters.values[rowID], parameters.limits) and ...);
					}
					(std::make_index_sequence<numColumns>{});
				}
			}

			const size_t numDefaultValues = (static_cast<bool>(parameters.defaultValue) + ...);
	
			if (numDefaultValues == numColumns)
			{
				std::string defaults;

				bool defaultsValid = this->Parse<std::string>(section, this->formatDefaultKey, defaults);

				if (defaultsValid)
				{
					std::tuple<T...> newValues;

					this->ExtractColumns(defaults, columns);

					// Attempt to read custom defaults
					[&]<size_t... columnIDs>(std::index_sequence<columnIDs...>)
					{
						defaultsValid = (this->ParseFromString<T>(columns[columnIDs], std::get<columnIDs>(newValues), parameters.limits) and ...);
					}
					(std::make_index_sequence<numColumns>{});

					// Update defaults
					if (defaultsValid)
					{
						[&]<size_t... columnIDs>(std::index_sequence<columnIDs...>)
						{
							((parameters.defaultValue.value() = std::move(std::get<columnIDs>(newValues))), ...);
						}
						(std::make_index_sequence<numColumns>{});
					}
				}

				// Apply defaults
				for (size_t rowID = 0; rowID < numRows; ++rowID)
				{
					if (not isValidRow[rowID])
					{
						((parameters.values[rowID] = parameters.defaultValue.value()), ...);

						isValidRow[rowID] = true;
					}
				}
			}

			return isValidRow;
		}
	};
}
