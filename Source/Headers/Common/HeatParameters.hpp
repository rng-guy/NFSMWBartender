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

#include "Globals.hpp"
#include "ConfigParser.hpp"
#include "PersistentStrings.hpp"

#include "../Utilities/FormatBuffer.hpp"



namespace HeatParameters
{
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Heat-level limit
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

	// Formatting buffer
	RELEASE_CONSTINIT FormatBuffer::Buffer buffer;

	// Logging
	constexpr LogLiteral nameFormat = "{:<24}";
	constexpr LogLiteral logMissing = "(none)";





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





	// Heat-level indices ---------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] consteval auto GenerateHeatLevelIDs()
	{
		HeatLevelArray<size_t> heatLevelIDs = {};

		for (size_t heatLevelID = 0; heatLevelID < maxHeatLevel; ++heatLevelID)
			heatLevelIDs[heatLevelID] = heatLevelID;

		return heatLevelIDs;
	}

	constexpr auto heatLevelIDs = GenerateHeatLevelIDs();





	// HeatState struct -----------------------------------------------------------------------------------------------------------------------------

	struct HeatState
	{
		// Members

		bool   isRace;
		size_t level;
	};





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


		template <class T>
		concept IsLoggable = requires (const T& type)
		{
			{type.Log(LogLiteral())} -> std::same_as<void>;
		};
	}





	// Value-parameter structs ----------------------------------------------------------------------------------------------------------------------
	
	#define HEAT_PARAMETER_VALUE(type, name, ...) HeatParameters::Value<type> name{#name, __VA_ARGS__}

	template <typename T>
	requires Details::IsCopyCompatible<T>
	struct Value
	{
	// Members

		T current;

		HeatLevelArray<T> roam = {};
		HeatLevelArray<T> race = {};

		[[no_unique_address]] const Bounds<T> limits;

		[[no_unique_address]] const LogLiteral name;


	// Methods

		constexpr Value
		(
			const LogLiteral name,
			const T          vanilla,
			const Bounds<T>& limits = {}
		) 
			requires Details::IsBoundsCompatible<T> : name(name), current(vanilla), limits(limits)
		{
		}

		constexpr Value
		(
			const LogLiteral name, 
			const T          vanilla
		)
		requires (not Details::IsBoundsCompatible<T>) : name(name), current(vanilla), limits({}) {}


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


		void SetToHeatStateWithoutLog(const HeatState state)
		{
			this->current = this->GetHeatStateEntry(state);
		}


		void SetToHeatState(const HeatState state) 
		{
			this->SetToHeatStateWithoutLog(state);

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain(buffer.Format(nameFormat, this->name.GetView()), this->current);
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
	};



	#define OPTIONAL_HEAT_PARAMETER_VALUE(type, name, ...) HeatParameters::OptionalValue<type> name{#name, __VA_ARGS__}

	template <typename T>
	requires Details::IsCopyCompatible<T>
	struct OptionalValue
	{
	// Members

		HEAT_PARAMETER_VALUE(bool, isEnabled, false);

		Value<T> value;

		[[no_unique_address]] const LogLiteral name;


	// Methods

		constexpr explicit OptionalValue
		(
			const LogLiteral name, 
			const Bounds<T>& limits = {}
		)
		requires Details::IsBoundsCompatible<T> : name(name), value("value", T(), limits) { }

		constexpr OptionalValue(const LogLiteral name)
		requires (not Details::IsBoundsCompatible<T>) : name(name), value("value", T()) {}


		void SetToHeatStateWithoutLog(const HeatState state)
		{
			this->isEnabled.SetToHeatStateWithoutLog(state);
			this->value    .SetToHeatStateWithoutLog(state);
		}


		void SetToHeatState(const HeatState state) 
		{
			this->SetToHeatStateWithoutLog(state);

			if constexpr (Globals::loggingEnabled)
			{
				const std::string_view name = buffer.Format(nameFormat, this->name.GetView());

				if (this->isEnabled.current)
					Globals::LogPlain(name, this->value.current);

				else Globals::LogPlain(name, logMissing);
			}
		}
	};





	// Pointer-parameter structs --------------------------------------------------------------------------------------------------------------------

	#define HEAT_PARAMETER_POINTER(type, name) HeatParameters::Pointer<type> name{#name}

	template <class T>
	requires (not Details::IsCopyCompatible<T>)
	struct Pointer
	{
	// Members

		const T* current = nullptr;

		HeatLevelArray<T> roam = {};
		HeatLevelArray<T> race = {};

		[[no_unique_address]] const LogLiteral name;


	// Methods

		constexpr explicit Pointer(const LogLiteral name) : name(name) {}


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


		void SetToHeatStateWithoutLog(const HeatState state)
		{
			this->current = &(this->GetHeatStateEntry(state));
		}


		void SetToHeatState(const HeatState state) 
		requires Details::IsLoggable<T>
		{
			this->SetToHeatStateWithoutLog(state);

			if constexpr (Globals::loggingEnabled)
				this->current->Log(this->name);
		}
	};



	#define OPTIONAL_HEAT_PARAMETER_POINTER(type, name) HeatParameters::OptionalPointer<type> name{#name}

	template <class T>
	requires (not Details::IsCopyCompatible<T>)
	struct OptionalPointer
	{
	// Members

		HEAT_PARAMETER_VALUE(bool, isEnabled, false);

		Pointer<T> pointer;

		[[no_unique_address]] const LogLiteral name;


	// Methods

		constexpr explicit OptionalPointer(const LogLiteral name) : name(name), pointer("pointer") {}


		void SetToHeatStateWithoutLog(const HeatState state)
		{
			this->isEnabled.SetToHeatStateWithoutLog(state);
			this->pointer  .SetToHeatStateWithoutLog(state);
		}


		void SetToHeatState(const HeatState state)
		requires Details::IsLoggable<T>
		{
			this->SetToHeatStateWithoutLog(state);

			if constexpr (Globals::loggingEnabled)
			{
				if (this->isEnabled.current)
					this->current->Log(this->name);

				else Globals::LogPlain(buffer.Format(nameFormat, this->name.GetView(), logMissing));
			}
		}
	};





	// Interval-parameter structs -------------------------------------------------------------------------------------------------------------------

	#define HEAT_PARAMETER_INTERVAL(type, name, ...) HeatParameters::Interval<type> name{#name, __VA_ARGS__}

	template <typename T>
	requires Details::IsBoundsCompatible<T>
	struct Interval
	{
	// Members

		Value<T> min;
		Value<T> max;

		[[no_unique_address]] const LogLiteral name;


	// Methods

		constexpr Interval
		(
			const LogLiteral name,
			const T          vanillaMin,
			const T          vanillaMax,
			const Bounds<T>& limits = {}
		) 
			: name(name), min("min", vanillaMin, limits), max("max", vanillaMax, limits)
		{
		}


		void SetToHeatStateWithoutLog(const HeatState state)
		{
			this->min.SetToHeatStateWithoutLog(state);
			this->max.SetToHeatStateWithoutLog(state);
		}


		void SetToHeatState(const HeatState state)
		{
			this->SetToHeatStateWithoutLog(state);

			if constexpr (Globals::loggingEnabled)
			{
				const std::string_view name = buffer.Format(nameFormat, this->name.GetView());
				Globals::LogPlain(name, this->min.current, "to", this->max.current);
			}
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
	};



	#define OPTIONAL_HEAT_PARAMETER_INTERVAL(type, name, ...) HeatParameters::OptionalInterval<type> name{#name, __VA_ARGS__}

	template <typename T>
	requires Details::IsBoundsCompatible<T>
	struct OptionalInterval
	{
	// Members

		HEAT_PARAMETER_VALUE(bool, isEnabled, false);

		Interval<T> interval;

		[[no_unique_address]] const LogLiteral name;


	// Methods

		constexpr explicit OptionalInterval
		(
			const LogLiteral name, 
			const Bounds<T>& limits = {}
		) 
			: name(name), interval("interval", T(), T(), limits) 
		{
		}


		void SetToHeatStateWithoutLog(const HeatState state)
		{
			this->isEnabled.SetToHeatStateWithoutLog(state);
			this->interval .SetToHeatStateWithoutLog(state);
		}


		void SetToHeatState(const HeatState state)
		{
			this->SetToHeatStateWithoutLog(state);

			if constexpr (Globals::loggingEnabled)
			{
				const std::string_view name = buffer.Format(nameFormat, this->name.GetView());

				if (this->isEnabled.current)
					Globals::LogPlain(name, this->interval.min.current, "to", this->interval.max.current);

				else Globals::LogPlain(name, logMissing);
			}
		}
	};





	// Resolution functions -------------------------------------------------------------------------------------------------------------------------

	template <typename T, class Validator>
	requires (Details::IsNonOwningString<T> and std::predicate<Validator, vault>)
	bool ResolveVehicleNames
	(
		Value<T>&       vehicleValue,
		const Validator IsVehicleTypeValid
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
							Globals::LogPlain(vehicleValue.name, (forRaces) ? "(race)" : "(roam)");

						Globals::LogDetail(LogDec(heatLevel), levelVehicleName, "->", vehicleValue.current);
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



	template <typename T>
	requires Details::IsNonOwningString<T>
	bool ResolveCarNames(Value<T>& vehicleValue) 
	{
		return ResolveVehicleNames(vehicleValue, Globals::IsVehicleTypeCar);
	}


	template <typename T>
	requires Details::IsNonOwningString<T>
	bool ResolveHelicopterNames(Value<T>& vehicleValue)
	{
		return ResolveVehicleNames(vehicleValue, Globals::IsVehicleTypeChopper);
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
			const HeatLevelArray<bool> isEnableds = Details::ParseHeatLevelArray<HeatParameters...>(forRaces, parser, section, parameters...);

			if constexpr (Details::AreOptional<HeatParameters...>)
				(..., (parameters.isEnabled.GetHeatLevelArray(forRaces) = isEnableds));
		}

		(..., Details::DoPostProcessing(parameters));
	}
}