#pragma once

#include <array>
#include <tuple>
#include <limits>
#include <string>
#include <format>
#include <utility>
#include <optional>
#include <concepts>
#include <filesystem>
#include <string_view>
#include <type_traits>

#include "Globals.h"
#include "ConfigParser.h"
#include "ModContainers.h"



namespace HeatParameters
{
	
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Heat parameters
	constexpr size_t maxHeatLevel = 10;
	constexpr float  maxHeat      = static_cast<float>(maxHeatLevel);

	// Configuration files
	constexpr std::string_view configDefaultKey         = "default";
	constexpr vault            configDefaultVaultHash   = Globals::GetVaultHash (configDefaultKey);
	constexpr binary           configDefaultBinarytHash = Globals::GetBinaryHash(configDefaultKey);
	
	constexpr size_t                     configFormatStart = 1;
	constexpr std::format_string<size_t> configFormatRoam  = "heat{:02}";
	constexpr std::format_string<size_t> configFormatRace  = "race{:02}";

	const std::filesystem::path configPathMain     = "scripts/BartenderSettings";
	const std::filesystem::path configPathBasic    = configPathMain / "Basic";
	const std::filesystem::path configPathAdvanced = configPathMain / "Advanced";

	// Shared persistent storage for (e.g.) vehicle-name Heat parameters
	RELEASE_CONSTINIT ModContainers::StableVaultMap<const std::string> vaultHashToPersistentString;





	// Scoped aliases -------------------------------------------------------------------------------------------------------------------------------

	using Parser = ConfigParser::Parser;

	template <typename T>
	using Bounds = ConfigParser::Bounds<T>;

	template <typename T>
	using HeatLevelArray = std::array<T, maxHeatLevel>;

	template <typename T>
	using Format = ConfigParser::Format<T, maxHeatLevel>;





	// Heat-level indices ---------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] consteval auto GenerateHeatLevelIDs()
	{
		HeatLevelArray<size_t> heatLevelIDs = {};

		for (size_t heatLevelID = 0; heatLevelID < maxHeatLevel; ++heatLevelID)
			heatLevelIDs[heatLevelID] = heatLevelID;

		return heatLevelIDs;
	}

	constexpr auto heatLevelIDs = GenerateHeatLevelIDs();





	// Concepts -------------------------------------------------------------------------------------------------------------------------------------

	namespace Concepts
	{

		template <typename T>
		concept IsBoundsCompatible = ConfigParser::Concepts::IsBoundsCompatible<T>;

		template <typename T>
		concept IsPureArithmetic = (std::is_arithmetic_v<T> and std::same_as<T, std::remove_cvref_t<T>>);
		
		template <typename T>
		concept IsNonOwningString = (std::same_as<T, const char*> or std::same_as<T, std::string_view>);

		template <typename T>
		concept IsCopyCompatible = (IsPureArithmetic<T> or IsNonOwningString<T>);
	}





	// String management ----------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] const std::string* GetPersistentStringByVaultHash(const vault hash)
	{
		const auto foundHash = vaultHashToPersistentString.find(hash);
		if (foundHash == vaultHashToPersistentString.end()) return nullptr;

		return foundHash->second.get();
	}



	[[nodiscard]] const std::string& CreatePersistentString
	(
		const vault            hash,
		const std::string_view string
	) {
		const auto [pairIt, isNewHash] = vaultHashToPersistentString.try_emplace(hash, string);
		return *(pairIt->second); // guaranteed to stay valid until game process terminates
	}


	[[nodiscard]] const std::string& CreatePersistentString(const std::string_view string)
	{
		return CreatePersistentString(Globals::GetVaultHash(string), string);
	}

	

	void MakePersistentString
	(
		const vault       hash, 
		std::string_view& string
	) {
		string = CreatePersistentString(hash, string);
	}


	void MakePersistentString(std::string_view& string)
	{
		MakePersistentString(Globals::GetVaultHash(string), string);
	}



	void MakePersistentString
	(
		const vault  hash,
		const char*& string
	) {
		if (not string) return; // UB with std::string_view constructor
		string = CreatePersistentString(hash, string).c_str();
	}


	void MakePersistentString(const char*& string)
	{
		if (not string) return; // UB with std::string_view constructor
		MakePersistentString(Globals::GetVaultHash(string), string);
	}





	// HeatParameter structs ------------------------------------------------------------------------------------------------------------------------

	template <typename T>
	requires Concepts::IsCopyCompatible<T>
	struct Value
	{
		T current;

		HeatLevelArray<T> roam = {};
		HeatLevelArray<T> race = {};

		Bounds<T> limits;


		constexpr explicit Value
		(
			const T         vanilla, 
			const Bounds<T> limits = {}
		) 
			requires (Concepts::IsBoundsCompatible<T>) 
			: current(vanilla), limits(limits) 
		{
		}

		constexpr explicit Value(const T vanilla) 
		requires (not Concepts::IsBoundsCompatible<T>) : current(vanilla), limits({}) {}


		[[nodiscard]] auto& GetHeatLevelArray(const bool forRaces)
		{
			return (forRaces) ? this->race : this->roam;
		}


		[[nodiscard]] const auto& GetHeatLevelArray(const bool forRaces) const
		{
			return (forRaces) ? this->race : this->roam;
		}


		void SetToHeatState
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->current = this->GetHeatLevelArray(forRaces)[heatLevel - 1];
		}


		[[nodiscard]] T GetMinimum() const
		requires Concepts::IsBoundsCompatible<T>
		{
			T minimum = std::numeric_limits<T>::max();

			for (const bool forRaces : {false, true})
			{
				for (const T value : this->GetHeatLevelArray(forRaces))
					minimum = std::min<T>(minimum, value);
			}

			return minimum;
		}


		[[nodiscard]] T GetMaximum() const
		requires Concepts::IsBoundsCompatible<T>
		{
			T maximum = std::numeric_limits<T>::min();

			for (const bool forRaces : {false, true})
			{
				for (const T value : this->GetHeatLevelArray(forRaces))
					maximum = std::max<T>(maximum, value);
			}

			return maximum;
		}


		[[nodiscard]] bool AnyTrue() const
		requires std::same_as<T, bool>
		{
			for (const bool forRaces : {false, true})
			{
				for (const T value : this->GetHeatLevelArray(forRaces))
					if (value) return true;
			}

			return false;
		}


		[[nodiscard]] bool AllTrue() const
		requires std::same_as<T, bool>
		{
			for (const bool forRaces : {false, true})
			{
				for (const T value : this->GetHeatLevelArray(forRaces))
					if (not value) return false;
			}

			return true;
		}


		void MakePersistent()
		requires Concepts::IsNonOwningString<T>
		{
			MakePersistentString(this->current);

			for (const bool forRaces : {false, true})
			{
				for (T& value : this->GetHeatLevelArray(forRaces))
					MakePersistentString(value);
			}
		}


		void Log(const std::string_view valueName) const
		{
			Globals::logger.Log<2>(valueName, this->current);
		}
	};



	template <typename T>
	requires Concepts::IsCopyCompatible<T>
	struct OptionalValue
	{
		Value<bool> isEnabled{false};

		Value<T> value;


		constexpr explicit OptionalValue(const Bounds<T> limits = {})
		requires (Concepts::IsBoundsCompatible<T>) : value(T(), limits) {}

		constexpr OptionalValue()
		requires (not Concepts::IsBoundsCompatible<T>) : value(T()) {}


		void SetToHeatState
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->isEnabled.SetToHeatState(forRaces, heatLevel);
			this->value    .SetToHeatState(forRaces, heatLevel);
		}


		void Log(const std::string_view valueName) const
		{
			if (this->isEnabled.current)
				Globals::logger.Log<2>(valueName, this->value.current);
		}
	};



	template <typename T>
	requires (not Concepts::IsCopyCompatible<T>)
	struct Pointer
	{
		const T* current = nullptr;

		HeatLevelArray<T> roam = {};
		HeatLevelArray<T> race = {};


		[[nodiscard]] auto& GetHeatLevelArray(const bool forRaces)
		{
			return (forRaces) ? this->race : this->roam;
		}


		[[nodiscard]] const auto& GetHeatLevelArray(const bool forRaces) const
		{
			return (forRaces) ? this->race : this->roam;
		}


		void SetToHeatState
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->current = &(this->GetHeatLevelArray(forRaces)[heatLevel - 1]);
		}
	};

	

	template <typename T>
	requires (not Concepts::IsCopyCompatible<T>)
	struct OptionalPointer
	{
		Value<bool> isEnabled{false};

		Pointer<T> pointer;


		void SetToHeatState
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->isEnabled.SetToHeatState(forRaces, heatLevel);
			this->pointer  .SetToHeatState(forRaces, heatLevel);
		}
	};

	

	template <typename T>
	requires Concepts::IsBoundsCompatible<T>
	struct Interval
	{
		Value<T> min;
		Value<T> max;


		constexpr explicit Interval
		(
			const T         vanillaMin,
			const T         vanillaMax,
			const Bounds<T> limits = {}
		) 
			: min(vanillaMin, limits),
			  max(vanillaMax, limits)
		{
		}


		void SetToHeatState
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->min.SetToHeatState(forRaces, heatLevel);
			this->max.SetToHeatState(forRaces, heatLevel);
		}


		[[nodiscard]] T GetMinimum() const
		{
			return this->min.GetMinimum();
		}


		[[nodiscard]] T GetMaximum() const
		{
			return this->max.GetMaximum();
		}


		[[nodiscard]] T GetRandomValue() const
		{
			return Globals::prng.GenerateNumber<T>(this->min.current, this->max.current);
		}


		void Log(const std::string_view intervalName) const
		{
			Globals::logger.Log<2>(intervalName, this->min.current, "to", this->max.current);
		}
	};



	template <typename T>
	requires Concepts::IsBoundsCompatible<T>
	struct OptionalInterval
	{
		Value<bool> isEnabled{false};

		Value<T> min;
		Value<T> max;


		constexpr explicit OptionalInterval(const Bounds<T> limits = {}) : min(T(), limits), max(T(), limits) {}


		void SetToHeatState
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->isEnabled.SetToHeatState(forRaces, heatLevel);
			this->min      .SetToHeatState(forRaces, heatLevel);
			this->max      .SetToHeatState(forRaces, heatLevel);
		}


		[[nodiscard]] T GetRandomValue() const
		{
			return Globals::prng.GenerateNumber<T>(this->min.current, this->max.current);
		}


		void Log(const std::string_view intervalName) const
		{
			if (this->isEnabled.current)
				Globals::logger.Log<2>(intervalName, this->min.current, "to", this->max.current);
		}
	};





	// Resolution functions -------------------------------------------------------------------------------------------------------------------------

	template <typename T, class Validator>
	requires (Concepts::IsNonOwningString<T> and std::predicate<Validator, vault>)
	bool ResolveVehicleNames
	(
		const std::string_view valueName,
		Value<T>&              vehicleValue,
		const Validator        IsVehicleTypeValid
	) {
		bool allTypesValid = true;

		MakePersistentString(vehicleValue.current); // vanilla value by default

		for (const bool forRaces : {false, true})
		{
			size_t heatLevel = 1;

			for (T& vehicleName : vehicleValue.GetHeatLevelArray(forRaces))
			{
				const vault vehicleType = Globals::GetVaultHash(vehicleName);

				if (not IsVehicleTypeValid(vehicleType))
				{
					if constexpr (Globals::loggingEnabled)
					{
						if (allTypesValid)
							Globals::logger.Log<2>(valueName, (forRaces) ? "(race)" : "(roam)");

						Globals::logger.Log<3>(DecFormat(heatLevel), vehicleName, "->", vehicleValue.current);
					}

					vehicleName   = vehicleValue.current; // already persistent
					allTypesValid = false;
				}
				else MakePersistentString(vehicleType, vehicleName);

				++heatLevel;
			}
		}

		return allTypesValid;
	}





	// Parsing helpers ------------------------------------------------------------------------------------------------------------------------------

	namespace Details
	{

		template <typename T> struct IsRegular              : std::false_type {};
		template <typename T> struct IsRegular<Value   <T>> : std::true_type  {};
		template <typename T> struct IsRegular<Interval<T>> : std::true_type  {};

		template <typename ...T>
		concept AreRegular = (IsRegular<T>::value and ...);

		template <typename T> struct IsOptional                      : std::false_type {};
		template <typename T> struct IsOptional<OptionalValue   <T>> : std::true_type  {};
		template <typename T> struct IsOptional<OptionalInterval<T>> : std::true_type  {};

		template <typename ...T>
		concept AreOptional = (IsOptional<T>::value and ...);



		template <typename T>
		[[nodiscard]] auto CreateFormatTuple
		(
			const bool forRaces,
			Value<T>&  value
		) {
			// Regular values have one array per setting (race / roam), a default value, and limits
			auto defaultValue = (forRaces) ? std::nullopt : std::optional<T>(value.current);
			return std::tuple(Format<T>(value.GetHeatLevelArray(forRaces), defaultValue, value.limits));
		}


		template <typename T>
		[[nodiscard]] auto CreateFormatTuple
		(
			const bool        forRaces,
			OptionalValue<T>& optionalValue
		) {
			// Optional values have no default value
			auto& value = optionalValue.value;
			return std::tuple(Format<T>(value.GetHeatLevelArray(forRaces), std::nullopt, value.limits));
		}


		template <typename T>
		[[nodiscard]] auto CreateFormatTuple
		(
			const bool   forRaces,
			Interval<T>& interval
		) {
			// Regular intervals consist of two regular values
			return std::tuple_cat
			(
				CreateFormatTuple<T>(forRaces, interval.min),
				CreateFormatTuple<T>(forRaces, interval.max)
			);
		}


		template <typename T>
		[[nodiscard]] auto CreateFormatTuple
		(
			const bool           forRaces,
			OptionalInterval<T>& optionalInterval
		) {
			// Optional intervals consist of two regular values without default values
			auto& min = optionalInterval.min;
			auto& max = optionalInterval.max;

			return std::tuple
			(
				Format<T>(min.GetHeatLevelArray(forRaces), std::nullopt, min.limits),
				Format<T>(max.GetHeatLevelArray(forRaces), std::nullopt, max.limits)
			);
		}



		template <typename T>
		void CopyRoamToRaceArray(Value<T>& value)
		{
			// Regular race arrays fall back to their roam counterparts
			value.race = value.roam;
		}


		template <typename T>
		void CopyRoamToRaceArray(Interval<T>& interval)
		{
			// The race arrays of regular intervals fall back to their roam counterparts
			interval.min.race = interval.min.roam;
			interval.max.race = interval.max.roam;
		}



		template <class ...HeatParameters>
		requires (AreRegular<HeatParameters...> or AreOptional<HeatParameters...>)
		HeatLevelArray<bool> ParseHeatLevelArray
		(
			const bool                forRaces,
			const Parser&             parser,
			const std::string_view    section,
			HeatParameters&        ...parameters
		) {
			auto formats = std::tuple_cat(CreateFormatTuple(forRaces, parameters)...);

			return [&]<size_t ...formatIDs>(std::index_sequence<formatIDs...>) -> HeatLevelArray<bool>
			{
				return parser.ParseFormat<maxHeatLevel>
				(
					section,
					configDefaultKey,
					(forRaces) ? configFormatRace : configFormatRoam,
					configFormatStart,
					std::move(std::get<formatIDs>(formats))...
				);
			}
			(std::make_index_sequence<std::tuple_size_v<decltype(formats)>>{});
		}



		template <typename T>
		void OrderIntervalValues
		(
			Value<T>&       minValue,
			const Value<T>& maxValue
		) {
			for (const bool forRaces : {false, true})
			{
				auto&       lowers = minValue.GetHeatLevelArray(forRaces);
				const auto& uppers = maxValue.GetHeatLevelArray(forRaces);

				for (const size_t heatLevelID : heatLevelIDs)
					lowers[heatLevelID] = std::min<T>(lowers[heatLevelID], uppers[heatLevelID]);
			}
		}



		void DoPostProcessing(const auto&) {}


		template <typename T>
		void DoPostProcessing(Interval<T>& interval)
		{
			// Regular intervals must have correctly ordered values
			OrderIntervalValues<T>(interval.min, interval.max);
		}


		template <typename T>
		void DoPostProcessing(OptionalInterval<T>& optionalInterval)
		{
			// Optional intervals must have correctly ordered values
			OrderIntervalValues<T>(optionalInterval.min, optionalInterval.max);
		}
	}





	// Generic parsing functions --------------------------------------------------------------------------------------------------------------------

	template <class ...HeatParameters>
	requires Details::AreRegular<HeatParameters...>
	void Parse
	(
		const Parser&             parser,
		const std::string_view    section,
		HeatParameters&        ...parameters
	) {
		// Parse roam arrays, using internal default values as fallback
		Details::ParseHeatLevelArray<HeatParameters...>(/* forRaces = */ false, parser, section, parameters...);

		// Use roam arrays as initial race arrays
		(..., Details::CopyRoamToRaceArray(parameters));

		// Parse race arrays, using copied roam arrays as fallback
		Details::ParseHeatLevelArray<HeatParameters...>(/* forRaces = */ true, parser, section, parameters...);

		// Enforce proper value-ordering in intervals
		(..., Details::DoPostProcessing(parameters));
	}



	template <class ...HeatParameters>
	requires Details::AreOptional<HeatParameters...>
	void Parse
	(
		const Parser&             parser,
		const std::string_view    section,
		HeatParameters&        ...parameters
	) {
		for (const bool forRaces : {false, true})
		{
			// Parse roam / race arrays without fallbacks, storing only those we can parse successfully
			const HeatLevelArray<bool> isEnableds = Details::ParseHeatLevelArray<HeatParameters...>(forRaces, parser, section, parameters...);

			// Mark successfully parsed Heat levels as enabled
			(..., (parameters.isEnabled.GetHeatLevelArray(forRaces) = isEnableds));
		}

		// Enforce proper value-ordering in intervals
		(..., Details::DoPostProcessing(parameters));
	}
}