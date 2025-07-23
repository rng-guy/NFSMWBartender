#pragma once

#include <fstream>
#include <string>
#include <format>
#include <optional>

#include "inipp.h"



namespace ConfigParser
{

	// Parameter data structures --------------------------------------------------------------------------------------------------------------------

	template <typename T, size_t size>
	struct FormatParameter
	{
		std::array<T, size>&   values;
		std::optional<T>       defaultValue;
		const std::optional<T> lowerBound;
		const std::optional<T> upperBound;
	};



	template <typename T>
	struct UserParameter
	{
		std::vector<T>&        values;
		const std::optional<T> lowerBound;
		const std::optional<T> upperBound;
	};





	// Configuration-file parser --------------------------------------------------------------------------------------------------------------------

	class Parser
	{
	private:

		inipp::Ini<char> parser;


		template <typename T>
		static void EnforceBounds
		(
			T&                      parameterValue,
			const std::optional<T>& lowerBound = {},
			const std::optional<T>& upperBound = {}
		) {
			if (lowerBound and (parameterValue < lowerBound.value())) parameterValue = lowerBound.value();
			if (upperBound and (parameterValue > upperBound.value())) parameterValue = upperBound.value();
		}


		std::vector<std::string> SplitTableRow (std::string source) const
		{
			std::vector<std::string> column;
			size_t                   position = 0;

			while ((position = source.find(this->tableColumnSeparator)) != std::string::npos)
			{
				column.push_back(source.substr(0, position));
				source.erase(0, position + this->tableColumnSeparator.length());
			}
			column.push_back(source);

			return column;
		}



	public:

		size_t      formatIndexOffset     = 1;
		std::string defaultFormatValueKey = "default";
		std::string tableColumnSeparator  = ",";


		bool LoadFile(const std::string& iniFile)
		{
			std::ifstream fileStream(iniFile);
			if (not fileStream) return false;

			this->parser.clear();
			this->parser.parse(fileStream);
			this->parser.strip_trailing_comments();
			fileStream.close();

			return true;
		}


		// For single values from a string
		template <typename T>
		static bool ParseParameterFromString
		(
			const std::string&     source,
			T&                     parameterValue,
			const std::optional<T> lowerBound      = {},
			const std::optional<T> upperBound      = {}
		) {
			const bool readSuccess = inipp::extract<char, T>(source, parameterValue);
			Parser::EnforceBounds<T>(parameterValue, lowerBound, upperBound);
			return readSuccess;
		}


		// For single values from parsed file
		template <typename T>
		bool ParseParameterFromFile
		(
			const std::string&     section,
			const std::string&     parameterKey,
			T&                     parameterValue,
			const std::optional<T> lowerBound      = {},
			const std::optional<T> upperBound      = {}
		) {
			const bool readSuccess = inipp::get_value<char>
			(
				this->parser.sections[section],
				parameterKey, 
				parameterValue
			);

			this->EnforceBounds<T>
			(
				parameterValue, 
				lowerBound, 
				upperBound
			);

			return readSuccess;
		}


		// For fixed-format keys from parsed file
		template <typename T, size_t size>
		size_t ParseFormatParameter
		(
			const std::string&     section,
			const std::string&     format,
			std::array<T, size>&   parameterValues,
			std::optional<T>       defaultValue     = {},
			const std::optional<T> lowerBound       = {},
			const std::optional<T> upperBound       = {}
		) {
			std::string parameterKey;

			size_t formatIndex   = this->formatIndexOffset;
			size_t numTotalReads = 0;

			if (defaultValue)
				this->ParseParameterFromFile<T>
				(
					section,
					this->defaultFormatValueKey,
					defaultValue.value(),
					lowerBound,
					upperBound
				);

			for (auto& parameterValue : parameterValues)
			{
				if (defaultValue) parameterValue = defaultValue.value();
				parameterKey = std::vformat(format, std::make_format_args(formatIndex));

				numTotalReads += this->ParseParameterFromFile<T>
				(
					section,
					parameterKey,
					parameterValue,
					lowerBound,
					upperBound
				);

				formatIndex++;
			}

			return numTotalReads;
		}


		template <typename T, size_t size>
		size_t ParseFormatParameter
		(
			const std::string&         section,
			const std::string&         format,
			FormatParameter<T, size>&& parameter
		) {
			return this->ParseFormatParameter<T, size>
			(
				section,
				format,
				parameter.values,
				parameter.defaultValue,
				parameter.lowerBound,
				parameter.upperBound
			);
		}


		// For user-defined keys from parsed file
		template <typename T>
		size_t ParseUserParameter
		(
			const std::string&        section,
			std::vector<std::string>& parameterKeys,
			std::vector<T>&           parameterValues,
			const std::optional<T>    lowerBound       = {},
			const std::optional<T>    upperBound       = {}
		) {
			parameterKeys.clear();
			parameterValues.clear();

			const size_t numTotalReads = inipp::get_value<char, T>
				(this->parser.sections[section], parameterKeys, parameterValues);

			for (auto& parameterValue : parameterValues)
				this->EnforceBounds<T>(parameterValue, lowerBound, upperBound);

			return numTotalReads;
		}


		template <typename T>
		size_t ParseUserParameter
		(
			const std::string&        section,
			std::vector<std::string>& parameterKeys,
			UserParameter<T>&&        parameter
		) {
			return this->ParseUserParameter<T>
			(
				section,
				parameterKeys,
				parameter.values,
				parameter.lowerBound,
				parameter.upperBound
			);
		}


		// For user-defined tables from parsed file
		template <typename ...T>
		size_t ParseParameterTable
		(
			const std::string&           section,
			std::vector<std::string>&    parameterKeys,
			UserParameter<T>&&        ...parameters
		) {
			constexpr size_t numColumns = sizeof...(T);

			std::array<bool, numColumns> pushedValue;
			std::vector<std::string>     unverifiedKeys;
			std::vector<std::string>     tableRows;
			std::vector<std::string>     columnStrings;

			bool isValidRow   = true;
			bool wasValidRow;

			size_t columnID;
			
			this->ParseUserParameter<std::string>(section, unverifiedKeys, tableRows);

			parameterKeys.clear();
			([&]{parameters.values.clear();}(), ...);

			for (size_t rowID = 0; rowID < tableRows.size(); rowID++)
			{
				columnStrings = this->SplitTableRow(tableRows[rowID]);
				if (columnStrings.size() != numColumns) continue;

				wasValidRow = isValidRow;
				isValidRow  = true;
				columnID    = 0;

				(
					[&]
					{
						T parameterValue = T();

						if ((not wasValidRow) and pushedValue[columnID]) 
							parameters.values.pop_back();

						if (isValidRow) 
							isValidRow &= this->ParseParameterFromString<T>
							(
								columnStrings[columnID], 
								parameterValue,
								parameters.lowerBound, 
								parameters.upperBound
							);

						if (isValidRow) parameters.values.push_back(parameterValue);
						pushedValue[columnID++] = isValidRow;
					}
				(), ...);

				if (isValidRow) parameterKeys.push_back(unverifiedKeys[rowID]);
			}

			columnID = 0;

			(
				[&]
				{
					if ((not isValidRow) and (pushedValue[columnID++])) 
						parameters.values.erase(parameters.values.end());
				}
			(), ...);

			return parameterKeys.size();
		}


		// For fixed-format tables from parsed file
		template <typename ...T, size_t size>
		std::array<bool, size> ParseParameterTable
		(
			const std::string&            section,
			const std::string&            format,
			FormatParameter<T, size>&& ...parameters
		) {
			constexpr size_t numColumns = sizeof...(T);

			std::array<std::string, size> tableRows;
			std::array<bool, size>        isValidRow;

			std::vector<std::string> columnStrings;
			std::string              defaultRow;

			size_t numDefaultValues = 0;
			size_t columnID;
	
			(
				[&]
				{
					if (parameters.defaultValue)
						numDefaultValues++;
				}
			(), ...);

			if (numDefaultValues == numColumns)
			{
				// Attempt to read new defaults from file
				bool isValidDefaultRow = this->ParseParameterFromFile<std::string>
				(
					section,
					this->defaultFormatValueKey,
					defaultRow
				);

				if (isValidDefaultRow) // new defaults in file
				{
					columnStrings = this->SplitTableRow(defaultRow);

					columnID = 0;

					// Check new defaults for validity
					(
						[&]
						{
							T defaultValue = T();

							if (isValidDefaultRow)
								isValidDefaultRow &= this->ParseParameterFromString<T>
								(
									columnStrings[columnID],
									defaultValue
								);

							columnID++;
						}
					(), ...);
				}

				columnID = 0;

				if (isValidDefaultRow) // new defaults are valid
				{
					// Overwrite old defaults with new ones
					(
						[&]
						{
							this->ParseParameterFromString<T>
							(
								columnStrings[columnID],
								parameters.defaultValue.value(),
								parameters.lowerBound,
								parameters.upperBound
							);

							columnID++;
						}
					(), ...);
				}
				else // new defaults either invalid or not in file
				{
					defaultRow = std::string();

					(
						[&]
						{
							this->EnforceBounds<T>
							(
								parameters.defaultValue.value(),
								parameters.lowerBound,
								parameters.upperBound
							);

							if (columnID > 0) defaultRow.append(this->tableColumnSeparator);

							if constexpr (std::is_convertible_v<T, std::string>)
								defaultRow.append(parameters.defaultValue.value());

							else
								defaultRow.append(std::to_string(parameters.defaultValue.value()));

							columnID++;
						}
					(), ...);
				}

				this->ParseFormatParameter<std::string>(section, format, tableRows, defaultRow);
			}

			else
				this->ParseFormatParameter<std::string>(section, format, tableRows);

			for (size_t rowID = 0; rowID < size; rowID++)
			{
				columnStrings     = this->SplitTableRow(tableRows[rowID]);
				isValidRow[rowID] = (columnStrings.size() == numColumns);
				if (not isValidRow[rowID]) continue;

				columnID = 0;
			
				(
					[&]
					{
						if (isValidRow[rowID])
							isValidRow[rowID] &= this->ParseParameterFromString<T>
							(
								columnStrings[columnID],
								parameters.values[rowID],
								parameters.lowerBound,
								parameters.upperBound
							);

						columnID++;
					}
				(), ...);
			}

			if (numDefaultValues == numColumns)
			{
				for (size_t rowID = 0; rowID < size; rowID++)
				{
					if (isValidRow[rowID]) continue;
					([&]{parameters.values[rowID] = parameters.defaultValue.value();}(), ...);
					isValidRow[rowID] = true;
				}
			}

			return isValidRow;
		}
	};
}
