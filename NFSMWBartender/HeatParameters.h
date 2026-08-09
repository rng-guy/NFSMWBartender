#pragma once

#include <array>
#include <tuple>
#include <limits>
#include <format>
#include <optional>
#include <concepts>
#include <algorithm>
#include <filesystem>
#include <string_view>
#include <type_traits>

#include "Globals.h"
#include "ConfigParser.h"
#include "PersistentStrings.h"



namespace HeatParameters
{
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Heat parameters
	constexpr size_t maxHeatLevel = 10;
	constexpr float  maxHeat      = static_cast<float>(maxHeatLevel);

	// Configuration files
	constexpr std::string_view configDefaultKey = "default";
	
	constexpr size_t                     configFormatStart = 1;
	constexpr std::format_string<size_t> configFormatRoam  = "heat{:02}";
	constexpr std::format_string<size_t> configFormatRace  = "race{:02}";

	const std::filesystem::path configPathMain     = "scripts/BartenderSettings";
	const std::filesystem::path configPathBasic    = configPathMain / "Basic";
	const std::filesystem::path configPathAdvanced = configPathMain / "Advanced";





	// Heat clamps ----------------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] float ClampHeat(const float heat)
	{
		return std::clamp<float>(heat, 1.f, maxHeat);
	}



	[[nodiscard]] size_t ClampHeatLevel(const size_t heatLevel)
	{
		return std::clamp<size_t>(heatLevel, 1, maxHeatLevel);
	}





	// Scoped aliases -------------------------------------------------------------------------------------------------------------------------------

	using Parser = ConfigParser::Parser; // aliased to suppress transient includes

	template <typename T>
	using Bounds = ConfigParser::Bounds<T>; // templated to suppress transient includes

	template <typename T>
	using HeatLevelArray = std::array<T, maxHeatLevel>;

	template <typename T>
	using Format = ConfigParser::Format<T, maxHeatLevel>;





	// Parameter concepts ---------------------------------------------------------------------------------------------------------------------------

	namespace Details
	{
		using ConfigParser::Concepts::IsPureArithmetic;
		using ConfigParser::Concepts::IsBoundsCompatible;

		template <typename T>
		concept IsNonOwningString = (std::same_as<T, const char*> or std::same_as<T, std::string_view>);

		template <typename T>
		concept IsCopyCompatible = (IsPureArithmetic<T> or IsNonOwningString<T>);

		template <typename T>
		concept IsBoolean = std::same_as<T, bool>;
	}





	// Heat-level indices ---------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] consteval auto GenerateHeatLevelIDs()
	{
		HeatLevelArray<size_t> heatLevelIDs = {};

		for (size_t heatLevelID = 0; heatLevelID < maxHeatLevel; ++heatLevelID)
			heatLevelIDs[heatLevelID] = heatLevelID;

		return heatLevelIDs;
	}

	constexpr auto heatLevelIDs = GenerateHeatLevelIDs();





	// HeatState struct  ----------------------------------------------------------------------------------------------------------------------------

	struct HeatState
	{
	// Members

		bool   isRace;
		size_t level;


	// Methods

		explicit HeatState
		(
			const bool   isRace,
			const size_t level
		) 
			: isRace(isRace), level(ClampHeatLevel(level))
		{
			if constexpr (Globals::loggingEnabled)
			{
				if (this->level != level)
					Globals::logger.Log("WARNING: [HPA] Heat level", DecFormat(level), "out of range");
			}
		}
	};





	// Parameter structs ----------------------------------------------------------------------------------------------------------------------------
	
	template <typename T>
	requires Details::IsCopyCompatible<T>
	struct Value
	{
	// Members

		T current;

		HeatLevelArray<T> roam = {};
		HeatLevelArray<T> race = {};

		[[no_unique_address]] const Bounds<T> limits;


	// Methods

		constexpr explicit Value
		(
			const T          vanillaValue,
			const Bounds<T>& limits = {}
		) 
			requires Details::IsBoundsCompatible<T> : current(vanillaValue), limits(limits)
		{
		}

		constexpr explicit Value(const T vanillaValue) 
		requires (not Details::IsBoundsCompatible<T>) : current(vanillaValue), limits({}) {}


		[[nodiscard]] auto& GetHeatLevelArray(const bool forRaces)
		{
			return (forRaces) ? this->race : this->roam;
		}


		[[nodiscard]] const auto& GetHeatLevelArray(const bool forRaces) const
		{
			return (forRaces) ? this->race : this->roam;
		}


		[[nodiscard]] T GetHeatStateEntry(const HeatState state) const
		{
			return this->GetHeatLevelArray(state.isRace)[state.level - 1];
		}


		void SetToHeatState(const HeatState state) 
		{
			this->current = this->GetHeatStateEntry(state);
		}


		[[nodiscard]] T GetMinimum() const
		requires Details::IsBoundsCompatible<T>
		{
			T minimum = std::numeric_limits<T>::max();

			for (const bool forRaces : {false, true})
			{
				for (const T levelValue : this->GetHeatLevelArray(forRaces))
					minimum = std::min<T>(minimum, levelValue);
			}

			return minimum;
		}


		[[nodiscard]] T GetMaximum() const
		requires Details::IsBoundsCompatible<T>
		{
			T maximum = std::numeric_limits<T>::lowest(); // in case of floats

			for (const bool forRaces : {false, true})
			{
				for (const T levelValue : this->GetHeatLevelArray(forRaces))
					maximum = std::max<T>(maximum, levelValue);
			}

			return maximum;
		}


		[[nodiscard]] bool AnyTrue() const
		requires Details::IsBoolean<T>
		{
			for (const bool forRaces : {false, true})
			{
				for (const bool levelFlag : this->GetHeatLevelArray(forRaces))
					if (levelFlag) return true;
			}

			return false;
		}


		[[nodiscard]] bool AllTrue() const
		requires Details::IsBoolean<T>
		{
			for (const bool forRaces : {false, true})
			{
				for (const bool levelFlag : this->GetHeatLevelArray(forRaces))
					if (not levelFlag) return false;
			}

			return true;
		}


		void MakePersistent()
		requires Details::IsNonOwningString<T>
		{
			PersistentStrings::Make(this->current);

			for (const bool forRaces : {false, true})
			{
				for (T& levelString : this->GetHeatLevelArray(forRaces))
					PersistentStrings::Make(levelString);
			}
		}


		void Log(const std::string_view valueName) const
		{
			Globals::logger.Log<2>(valueName, this->current);
		}
	};



	template <typename T>
	requires Details::IsCopyCompatible<T>
	struct OptionalValue
	{
	// Members

		Value<bool> isEnabled{false};

		Value<T> value;


	// Methods

		constexpr explicit OptionalValue(const Bounds<T>& limits = {})
		requires Details::IsBoundsCompatible<T> : value(T(), limits) {}

		constexpr OptionalValue()
		requires (not Details::IsBoundsCompatible<T>) : value(T()) {}


		void SetToHeatState(const HeatState state) 
		{
			this->isEnabled.SetToHeatState(state);
			this->value    .SetToHeatState(state);
		}


		void Log(const std::string_view valueName) const
		{
			if (not this->isEnabled.current) return;

			this->value.Log(valueName);
		}
	};



	template <typename T>
	requires (not Details::IsCopyCompatible<T>)
	struct Pointer
	{
	// Members

		const T* current = nullptr;

		HeatLevelArray<T> roam = {};
		HeatLevelArray<T> race = {};


	// Methods

		[[nodiscard]] auto& GetHeatLevelArray(const bool forRaces)
		{
			return (forRaces) ? this->race : this->roam;
		}


		[[nodiscard]] const auto& GetHeatLevelArray(const bool forRaces) const
		{
			return (forRaces) ? this->race : this->roam;
		}


		[[nodiscard]] const T& GetHeatStateEntry(const HeatState state) const
		{
			return this->GetHeatLevelArray(state.isRace)[state.level - 1];
		}


		void SetToHeatState(const HeatState state) 
		{
			this->current = &(this->GetHeatStateEntry(state));
		}
	};

	

	template <typename T>
	requires (not Details::IsCopyCompatible<T>)
	struct OptionalPointer
	{
	// Members

		Value<bool> isEnabled{false};

		Pointer<T> pointer;


	// Methods

		void SetToHeatState(const HeatState state)
		{
			this->isEnabled.SetToHeatState(state);
			this->pointer  .SetToHeatState(state);
		}
	};

	

	template <typename T>
	requires Details::IsBoundsCompatible<T>
	struct Interval
	{
	// Members

		Value<T> min;
		Value<T> max;


	// Methods

		constexpr explicit Interval
		(
			const T          vanillaMin,
			const T          vanillaMax,
			const Bounds<T>& limits = {}
		) 
			: min(vanillaMin, limits), max(vanillaMax, limits)
		{
		}


		void SetToHeatState(const HeatState state)
		{
			this->min.SetToHeatState(state);
			this->max.SetToHeatState(state);
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
	requires Details::IsBoundsCompatible<T>
	struct OptionalInterval
	{
	// Members

		Value<bool> isEnabled{false};

		Interval<T> interval;


	// Methods

		constexpr explicit OptionalInterval(const Bounds<T>& limits = {}) : interval(T(), T(), limits) {}


		void SetToHeatState(const HeatState state)
		{
			this->isEnabled.SetToHeatState(state);
			this->interval .SetToHeatState(state);
		}
		

		void Log(const std::string_view intervalName) const
		{
			if (not this->isEnabled.current) return;

			this->interval.Log(intervalName);
		}
	};





	// Resolution functions -------------------------------------------------------------------------------------------------------------------------

	template <typename T, class Validator>
	requires (Details::IsNonOwningString<T> and std::predicate<Validator, vault>)
	bool ResolveVehicleNames
	(
		const std::string_view valueName,
		Value<T>&              vehicleValue,
		const Validator        IsVehicleTypeValid
	) {
		bool allTypesValid = true;

		PersistentStrings::Make(vehicleValue.current); // vanilla value by default

		for (const bool forRaces : {false, true})
		{
			size_t heatLevel = 1;

			for (T& levelVehicleName : vehicleValue.GetHeatLevelArray(forRaces))
			{
				const vault vehicleType = Globals::GetVaultHash(levelVehicleName);

				if (not IsVehicleTypeValid(vehicleType))
				{
					if constexpr (Globals::loggingEnabled)
					{
						if (allTypesValid)
							Globals::logger.Log<2>(valueName, (forRaces) ? "(race)" : "(roam)");

						Globals::logger.Log<3>(DecFormat(heatLevel), levelVehicleName, "->", vehicleValue.current);
					}

					levelVehicleName = vehicleValue.current; // already persistent
					allTypesValid    = false;
				}
				else PersistentStrings::Make(vehicleType, levelVehicleName);

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
		[[nodiscard]] Format<T> ToFormatWithDefault
		(
			const bool forRaces,
			Value<T>&  value
		) {
			return {value.GetHeatLevelArray(forRaces), value.current, value.limits};
		}


		template <typename T>
		[[nodiscard]] Format<T> ToFormatWithoutDefault
		(
			const bool forRaces,
			Value<T>&  value
		) {
			return {value.GetHeatLevelArray(forRaces), std::nullopt, value.limits};
		}



		template <typename T>
		[[nodiscard]] auto CreateFormatTuple
		(
			const bool forRaces,
			Value<T>&  value
		) {
			return std::tuple
			(
				ToFormatWithDefault<T>(forRaces, value)
			);
		}


		template <typename T>
		[[nodiscard]] auto CreateFormatTuple
		(
			const bool        forRaces,
			OptionalValue<T>& optionalValue
		) {
			return std::tuple
			(
				ToFormatWithoutDefault<T>(forRaces, optionalValue.value)
			);
		}



		template <typename T>
		[[nodiscard]] auto CreateFormatTuple
		(
			const bool   forRaces,
			Interval<T>& interval
		) {
			return std::tuple
			(
				ToFormatWithDefault<T>(forRaces, interval.min),
				ToFormatWithDefault<T>(forRaces, interval.max)
			);
		}


		template <typename T>
		[[nodiscard]] auto CreateFormatTuple
		(
			const bool           forRaces,
			OptionalInterval<T>& optionalInterval
		) {
			return std::tuple
			(
				ToFormatWithoutDefault<T>(forRaces, optionalInterval.interval.min),
				ToFormatWithoutDefault<T>(forRaces, optionalInterval.interval.max)
			);
		}



		template <class ...HeatParameters>
		HeatLevelArray<bool> ParseHeatLevelArray
		(
			const bool                forRaces,
			const Parser&             parser,
			const std::string_view    section,
			HeatParameters&        ...parameters
		) {
			return std::apply([&](auto&& ...formats) -> HeatLevelArray<bool>
			{
				return parser.ParseFormat<maxHeatLevel>
				(
					section,
					configDefaultKey,
					(forRaces) ? configFormatRace : configFormatRoam,
					configFormatStart,
					formats...
				);
			},
			std::tuple_cat(CreateFormatTuple(forRaces, parameters)...));
		}



		template <typename T>
		void OrderIntervalValues(Interval<T>& interval) 
		{
			for (const bool forRaces : {false, true})
			{
				auto&       minArray = interval.min.GetHeatLevelArray(forRaces);
				const auto& maxArray = interval.max.GetHeatLevelArray(forRaces);

				for (const size_t heatLevelID : heatLevelIDs)
					minArray[heatLevelID] = std::min<T>(minArray[heatLevelID], maxArray[heatLevelID]);
			}
		}



		void DoPostProcessing(const auto&) {}


		template <typename T>
		void DoPostProcessing(Interval<T>& interval)
		{
			OrderIntervalValues<T>(interval);
		}


		template <typename T>
		void DoPostProcessing(OptionalInterval<T>& optionalInterval)
		{
			OrderIntervalValues<T>(optionalInterval.interval);
		}
	}





	// Generic parsing function --------------------------------------------------------------------------------------------------------------------

	template <class ...HeatParameters>
	requires (Details::AreRegular<HeatParameters...> or Details::AreOptional<HeatParameters...>)
	void Parse
	(
		const Parser&             parser,
		const std::string_view    section,
		HeatParameters&        ...parameters
	) {
		for (const bool forRaces : {false, true})
		{
			const auto isEnableds = Details::ParseHeatLevelArray<HeatParameters...>(forRaces, parser, section, parameters...);

			if constexpr (Details::AreOptional<HeatParameters...>)
				(..., (parameters.isEnabled.GetHeatLevelArray(forRaces) = isEnableds));
		}

		(..., Details::DoPostProcessing(parameters));
	}
}