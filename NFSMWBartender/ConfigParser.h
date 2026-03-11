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

		// Bool doesn't require bounds-checking
		template <typename V>
		concept IsBoundsCompatible = (StreamParser::Concepts::IsPureArithmetic<V> and (not std::same_as<V, bool>));

		template <typename ...Vs>
		concept AreParseable = StreamParser::Concepts::AreParseable<Vs...>;

		template <typename K, typename ...Vs>
		concept AreUserParseable = StreamParser::Concepts::AreSectionParseable<K, Vs...>;
	}

	



	// Auxiliary data structures --------------------------------------------------------------------------------------------------------------------

	template <typename T>
	struct Bounds 
	{
		constexpr void Enforce(const auto&) const noexcept {}
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

		FlatContainers::Map<std::filesystem::path, Parser::Sections> pathToSections;


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

		Parser() = default;

		explicit Parser(const size_t fileCapacity) : pathToSections(fileCapacity) {}


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
			const auto [pair, wasAdded] = this->pathToSections.try_emplace(this->currentPath);

			if (not wasAdded)
			{
				this->sections = pair->second;
				
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Load:", file);

				return true;
			}
			
			// Attempt to parse new file
			std::ifstream fileStream(this->currentPath);

			if (fileStream.is_open())
			{
				this->ParseStream(fileStream);
				pair->second = this->sections;

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Open:", file);

				return true;
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>("Skip:", file);

			return false;
		}


		const auto& GetCurrentPath() const noexcept
		{
			return this->currentPath;
		}


		const auto& GetSections() const noexcept
		{
			return this->sections;
		}


		void ClearCachedPaths() noexcept
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
			const std::string_view    section,
			const std::string_view    defaultKey,
			const std::string_view    formatKey,
			const size_t              keyStart,
			Format<Vs, numRows>&&  ...parameters
		) 
			const 
		{
			const auto foundSection      = this->sections.find(section);
			const bool hasFullDefaultRow = (parameters.defaultValue.has_value() and ...);

			// Parse default (if available)
			if ((foundSection != this->sections.end()) and hasFullDefaultRow and (not defaultKey.empty()))
				this->GetValues<Vs...>(foundSection->second, defaultKey, {*(parameters.defaultValue)}...);

			// Parse each row column-wise
			size_t keyIndex = keyStart;

			std::array<bool, numRows> isValidRows = {};

			for (size_t rowID = 0; rowID < numRows; ++rowID)
			{
				// Parse row without default first
				if (foundSection != this->sections.end())
				{
					isValidRows[rowID] = this->GetValues<Vs...>
					(
						foundSection->second,
						std::vformat(formatKey, std::make_format_args(keyIndex)),
						parameters.values[rowID]...
					);

					++keyIndex;
				}

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

		
		// User-defined key-value pair(s) from parsed file
		template <typename K, typename ...Vs>
		requires Concepts::AreUserParseable<K, Vs...>
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