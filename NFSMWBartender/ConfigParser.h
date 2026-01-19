#pragma once

#include <fstream>
#include <string>
#include <format>
#include <optional>

#include "inipp.h"



namespace ConfigParser
{

	// Parameter data structures --------------------------------------------------------------------------------------------------------------------

	template <typename T>
	struct Parameter
	{
		T& value;

		std::optional<T> lowerBound;
		std::optional<T> upperBound;
	};



	template <typename T, size_t size>
	struct FormatParameter
	{
		std::array<T, size>& values;

		std::optional<T> defaultValue;
		std::optional<T> lowerBound;
		std::optional<T> upperBound;
	};



	template <typename T>
	struct UserParameter
	{
		std::vector<T>& values;

		std::optional<T> lowerBound;
		std::optional<T> upperBound;
	};





	// Configuration-file parser --------------------------------------------------------------------------------------------------------------------

	class Parser
	{
	private:

		inipp::Ini<char> parser;
		std::string      iniFile;


		template <typename T>
		static void EnforceBounds
		(
			T&                      parameterValue,
			const std::optional<T>& lowerBound      = {},
			const std::optional<T>& upperBound      = {}
		) {
			if (lowerBound and (parameterValue < lowerBound.value())) 
				parameterValue = lowerBound.value();

			if (upperBound and (parameterValue > upperBound.value())) 
				parameterValue = upperBound.value();
		}


		void ExtractColumns
		(
			std::string               row,
			std::vector<std::string>& columns
		) 
			const
		{
			size_t position;
			columns.clear();

			while ((position = row.find(this->tableColumnSeparator)) != std::string::npos)
			{
				columns.push_back(row.substr(0, position));
				row.erase(0, position + this->tableColumnSeparator.length());
			}

			columns.push_back(row);
		}



	public:

		size_t      formatIndexOffset     = 1;
		std::string defaultFormatValueKey = "default";
		std::string tableColumnSeparator  = ",";


		bool LoadFile(const std::string& iniFile)
		{
			if (this->iniFile == iniFile) return true;

			std::ifstream fileStream(iniFile);
			if (not fileStream.is_open()) return false;

			this->parser.clear();
			this->iniFile = iniFile;
			this->parser.parse(fileStream);
			this->parser.strip_trailing_comments();

			return true;
		}


		const auto& GetSections() const
		{
			return this->parser.sections;
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
		bool ParseParameter
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


		template <typename T>
		bool ParseParameter
		(
			const std::string& section,
			const std::string& parameterKey,
			Parameter<T>&&     parameter
		) {
			return this->ParseParameter<T>
			(
				section, 
				parameterKey, 
				parameter.value, 
				parameter.lowerBound, 
				parameter.upperBound
			);
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
				this->ParseParameter<T>
				(
					section,
					this->defaultFormatValueKey,
					defaultValue.value(),
					lowerBound,
					upperBound
				);

			for (auto& parameterValue : parameterValues)
			{
				if (defaultValue) 
					parameterValue = defaultValue.value();

				parameterKey = std::vformat(format, std::make_format_args(formatIndex));

				numTotalReads += this->ParseParameter<T>
				(
					section,
					parameterKey,
					parameterValue,
					lowerBound,
					upperBound
				);

				++formatIndex;
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
			parameterKeys  .clear();
			parameterValues.clear();

			const size_t numTotalReads = inipp::get_value<char, T>
				(this->parser.sections[section], parameterKeys, parameterValues);

			for (auto& parameterValue : parameterValues)
				this->EnforceBounds<T>(parameterValue, lowerBound, upperBound);

			return numTotalReads;
		}


		// Seriously, fuck the std::vector<bool> "optimisation"
		size_t ParseUserParameter
		(
			const std::string&        section,
			std::vector<std::string>& parameterKeys,
			std::vector<bool>&        parameterValues
		) {
			parameterKeys  .clear();
			parameterValues.clear();

			const size_t numTotalReads = inipp::get_value<char, bool>
				(this->parser.sections[section], parameterKeys, parameterValues);

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


		// For single parameter rows
		template <typename ...T>
		bool ParseParameterRow
		(
			const std::string&    section,
			const std::string&    parameterKey,
			Parameter<T>&&     ...parameters
		) {
			constexpr size_t numColumns = sizeof...(T);

			std::string              rowString;
			std::vector<std::string> columnStrings;

			this->ParseParameter<std::string>(section, parameterKey, rowString);
			this->ExtractColumns(rowString, columnStrings);

			if (columnStrings.size() != numColumns) return false;

			size_t columnID   = 0;
			bool   isValidRow = true;

			(
				[&]
				{
					if (isValidRow)
					{
						isValidRow &= this->ParseParameterFromString<T>
						(
							columnStrings[columnID++],
							parameters.value,
							parameters.lowerBound,
							parameters.upperBound
						);
					}
				}
			(), ...);

			return isValidRow;
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

			std::array<bool, numColumns> pushedValue = {};

			std::vector<std::string> unverifiedKeys;
			std::vector<std::string> tableRows;
			std::vector<std::string> columnStrings;

			bool isValidRow = true;
			
			this->ParseUserParameter<std::string>(section, unverifiedKeys, tableRows);

			parameterKeys.clear();
			([&]{parameters.values.clear();}(), ...);

			for (size_t rowID = 0; rowID < tableRows.size(); ++rowID)
			{
				this->ExtractColumns(tableRows[rowID], columnStrings);
				if (columnStrings.size() != numColumns) continue;

				const bool wasValidRow = isValidRow;
				size_t     columnID    = 0;

				isValidRow = true;

				(
					[&]
					{
						if ((not wasValidRow) and pushedValue[columnID]) 
							parameters.values.pop_back();

						if (isValidRow)
						{
							T parameterValue = T();

							isValidRow &= this->ParseParameterFromString<T>
							(
								columnStrings[columnID],
								parameterValue,
								parameters.lowerBound,
								parameters.upperBound
							);

							if (isValidRow)
								parameters.values.push_back(parameterValue);
						}

						pushedValue[columnID++] = isValidRow;
					}
				(), ...);

				if (isValidRow) 
					parameterKeys.push_back(unverifiedKeys[rowID]);
			}

			// Check final row separately
			if (not isValidRow)
			{
				size_t columnID = 0;

				(
					[&]
					{
						if (pushedValue[columnID++])
							parameters.values.pop_back();
					}
				(), ...);
			}

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
	
			(
				[&]
				{
					if (parameters.defaultValue)
						++numDefaultValues;
				}
			(), ...);

			if (numDefaultValues == numColumns)
			{
				// Attempt to read new defaults from file
				bool isValidDefaultRow = this->ParseParameter<std::string>
				(
					section,
					this->defaultFormatValueKey,
					defaultRow
				);

				if (isValidDefaultRow) // new defaults in file
				{
					this->ExtractColumns(defaultRow, columnStrings);

					size_t columnID = 0;

					// Check new defaults for validity
					(
						[&]
						{
							if (isValidDefaultRow)
							{
								T dummyDefaultValue = T();

								isValidDefaultRow &= this->ParseParameterFromString<T>
								(
									columnStrings[columnID], 
									dummyDefaultValue
								);
							}
						
							++columnID;
						}
					(), ...);
				}

				if (isValidDefaultRow) // new defaults are valid
				{
					size_t columnID = 0;

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

							++columnID;
						}
					(), ...);
				}
				else // new defaults either invalid or not in file
				{
					size_t columnID = 0;

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

							if (columnID > 0) 
								defaultRow.append(this->tableColumnSeparator);

							if constexpr (std::is_convertible_v<T, std::string>)
								defaultRow.append(parameters.defaultValue.value());

							else
								defaultRow.append(std::to_string(parameters.defaultValue.value()));

							++columnID;
						}
					(), ...);
				}

				this->ParseFormatParameter<std::string>(section, format, tableRows, defaultRow);
			}
			else this->ParseFormatParameter<std::string>(section, format, tableRows);

			for (size_t rowID = 0; rowID < size; ++rowID)
			{
				this->ExtractColumns(tableRows[rowID], columnStrings);
				isValidRow[rowID] = (columnStrings.size() == numColumns);
				if (not isValidRow[rowID]) continue;

				size_t columnID = 0;
			
				(
					[&]
					{
						if (isValidRow[rowID])
						{
							isValidRow[rowID] &= this->ParseParameterFromString<T>
							(
								columnStrings[columnID],
								parameters.values[rowID],
								parameters.lowerBound,
								parameters.upperBound
							);
						}

						++columnID;
					}
				(), ...);
			}

			if (numDefaultValues == numColumns)
			{
				for (size_t rowID = 0; rowID < size; ++rowID)
				{
					if (not isValidRow[rowID])
					{
						([&]{parameters.values[rowID] = parameters.defaultValue.value();}(), ...);

						isValidRow[rowID] = true;
					}
				}
			}

			return isValidRow;
		}
	};
}
