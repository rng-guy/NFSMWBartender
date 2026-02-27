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
#include <filesystem>

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

	class Parser : protected inipp::Ini
	{
	private:

		std::filesystem::path currentPath;
		
		std::map<std::filesystem::path, inipp::Ini::Sections> pathToSections;


		static std::string TrimNewColumn(std::string&& string)
		{
			inipp::TrimStringLeft (string);
			inipp::TrimStringRight(string);

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

					// The static analyser likes to complain here, even though this is perfectly safe
					columns[columnID++] = this->TrimNewColumn(std::string(iterator.begin(), iterator.end()));
				}
				else return std::nullopt;
			}

			if (columnID != numColumns) return std::nullopt;

			return columns;
		}


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

		size_t formatIndexOffset = 1;

		std::string columnSeparator  = ",";
		std::string formatDefaultKey = "default";
		

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

			this->Clear();

			// Attempt to create new cache entry
			const auto& [pair, wasAdded] = this->pathToSections.try_emplace(this->currentPath);

			// Check against cached paths
			if (not wasAdded)
			{
				this->sections = pair->second;
				
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Load:", file);

				return true;
			}

			// Attempt to open new file
			std::ifstream fileStream(this->currentPath);

			if (fileStream.is_open())
			{
				this->ParseStream(fileStream);
				pair->second = this->sections;

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
			return this->sections;
		}


		void ClearCachedPaths()
		{
			this->pathToSections.clear();
		}


		// Single value from string
		template <typename T>
		static bool ParseFromString
		(
			const std::string& source,
			Parameter<T>&&     parameter
		) {
			const bool isValid = inipp::ExtractFromString<T>(source, parameter.value);

			parameter.limits.Enforce(parameter.value);

			return isValid;
		}


		// Multiple values from delimited string
		template <typename ...Ts>
		requires (sizeof...(Ts) > 1)
		bool ParseFromString
		(
			const std::string&    source,
			Parameter<Ts>&&    ...parameters
		) {
			bool isValid = false;

			constexpr size_t numColumns = sizeof...(Ts);
	
			if (const auto columns = this->ParseColumns<numColumns>(source))
			{
				[&]<size_t... columnIDs>(std::index_sequence<columnIDs...>)
				{
					isValid = (this->ParseFromString<Ts>((*columns)[columnIDs], std::move(parameters)) and ...);
				}
				(std::make_index_sequence<numColumns>{});
			}

			return isValid;
		}


		// Single value from parsed file
		template <typename T>
		bool ParseFromFile
		(
			const std::string_view section,
			const std::string_view key,
			Parameter<T>&&         parameter
		) {
			const bool isValid = this->ExtractFromSection<T>(section, key, parameter.value);

			parameter.limits.Enforce(parameter.value);

			return isValid;
		}


		// Multi-value row from parsed file
		template <typename ...Ts>
		requires (sizeof...(Ts) > 1)
		bool ParseFromFile
		(
			const std::string_view    section,
			const std::string_view    key,
			Parameter<Ts>&&        ...parameters
		) {
			std::string row;

			if (this->ParseFromFile<std::string>(section, key, {row}))
				return this->ParseFromString<Ts...>(row, std::move(parameters)...);

			return false;
		}


		// Single-column, fixed-format values from parsed file
		template <size_t numRows, typename T>
		std::array<bool, numRows> ParseFormat
		(
			const std::string_view section,
			const std::string_view format,
			Format<T, numRows>&&   parameter
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
		template <size_t numRows, typename ...Ts>
		requires (sizeof...(Ts) > 1)
		std::array<bool, numRows> ParseFormat
		(
			const std::string_view    section,
			const std::string_view    format,
			Format<Ts, numRows>&&  ...parameters
		) {
			bool hasInvalidRows = false;

			std::array<std::string, numRows> rows;

			// Parse parameters as-is first
			std::array<bool, numRows> isValidRows = this->ParseFormat<numRows, std::string>(section, format, {rows});

			for (size_t rowID = 0; rowID < numRows; ++rowID)
			{
				if (isValidRows[rowID])
					isValidRows[rowID] = this->ParseFromString<Ts...>(rows[rowID], {parameters.values[rowID], parameters.limits}...);

				hasInvalidRows |= (not isValidRows[rowID]);
			}

			// Parse and apply defaults if necessary (and available)
			if ((hasInvalidRows and ... and parameters.defaultValue))
			{
				this->ParseFromFile<Ts...>(section, this->formatDefaultKey, {*(parameters.defaultValue), parameters.limits}...);

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
			const std::string_view    section,
			std::vector<std::string>& keys,
			User<T>&&                 parameter
		) {
			keys            .clear();
			parameter.values.clear();

			const size_t numReads = this->ExtractSection<T>(section, keys, parameter.values);

			// Fuck the std::vector<bool> specialisation
			if constexpr (not std::is_same_v<T, bool>)
			{
				for (T& value : parameter.values)
					parameter.limits.Enforce(value);
			}
		
			return numReads;
		}


		// Multi-column, user-defined key-value pairs from parsed file
		template <typename ...Ts>
		requires (sizeof...(Ts) > 1)
		size_t ParseUser
		(
			const std::string_view       section,
			std::vector<std::string>&    keys,
			User<Ts>&&                ...parameters
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

				constexpr size_t numColumns = sizeof...(Ts);

				[&]<size_t ...columnIDs>(std::index_sequence<columnIDs...>)
				{
					std::tuple<Ts...> candidates;

					for (size_t rowID = 0; rowID < numRows; ++rowID)
					{
						if (this->ParseFromString<Ts...>(rows[rowID], {std::get<columnIDs>(candidates), parameters.limits}...))
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