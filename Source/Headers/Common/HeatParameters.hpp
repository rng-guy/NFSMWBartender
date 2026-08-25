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





	// Heat clamps ----------------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] float ClampHeat(const float heat)
	{
		return std::clamp<float>(heat, 1.f, maxHeat);
	}



	[[nodiscard]] size_t ClampHeatLevel(const size_t heatLevel)
	{
		return std::clamp<size_t>(heatLevel, 1, maxHeatLevel);
	}





	// Logging functions ----------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] std::string_view PadParameterName(const LogLiteral name)
	{
		static RELEASE_CONSTINIT FormatBuffer::Buffer buffer;

		return buffer.Format("{:<24}", name.GetView());
	}



	template <typename ...Ts>
	void LogParameter
	(
		const LogLiteral    name, 
		Ts&&             ...segments
	) {
		Globals::LogPlain(PadParameterName(name), std::forward<Ts>(segments)...);
	}


	void LogMissingParameter(const LogLiteral name)
	{
		Globals::LogPlain(PadParameterName(name), "(missing)");
	}





	// Scoped aliases -------------------------------------------------------------------------------------------------------------------------------

	template <typename T>
	using HeatLevelArray = std::array<T, maxHeatLevel>;

	template <typename T>
	using Bounds = ConfigParser::Bounds<T>; // templated to suppress transient includes





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
		using ConfigParser::Concepts::IsBoundsCompatible;


		template <typename T>
		concept IsNonOwningString = (std::same_as<T, const char*> or std::same_as<T, std::string_view>);

		template <typename T>
		concept IsCopyCompatible = (ConfigParser::Concepts::AreExtractable<T> and std::is_trivially_copyable_v<T>);

		template <typename T>
		concept IsPureBoolean = std::same_as<T, bool>;


		template <class T>
		concept IsLoggable = requires (const T& type)
		{
			{type.Log(LogLiteral())} -> std::same_as<void>;
		};
	}





	// Value-parameter classes ----------------------------------------------------------------------------------------------------------------------
	
	#define HEAT_PARAMETER_VALUE(type, name, ...) HeatParameters::Value<type> name{#name, __VA_ARGS__}

	template <typename T>
	requires Details::IsCopyCompatible<T>
	class Value
	{
	private: // members

		[[no_unique_address]] Bounds<T> limits;

		[[no_unique_address]] LogLiteral name;


	public: // members (for ASM access)

		T current;

		HeatLevelArray<T> roam = {};
		HeatLevelArray<T> race = {};


	public: // methods

		constexpr Value
		(
			const LogLiteral name,
			const T          initial,
			const Bounds<T>& limits = {}
		) 
			requires Details::IsBoundsCompatible<T>
			: name(name), current(initial), limits(limits)
		{
		}


		constexpr Value
		(
			const LogLiteral name, 
			const T          initial
		)
			requires (not Details::IsBoundsCompatible<T>)
			: name(name), current(initial), limits()
		{
		}


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


		void SetToHeatStateSilently(const HeatState state)
		{
			this->current = this->GetHeatStateEntry(state);
		}


		void SetToHeatState(const HeatState state) 
		{
			this->SetToHeatStateSilently(state);

			if constexpr (Globals::loggingEnabled)
				LogParameter(this->name, this->current);
		}
		

		[[nodiscard]] Bounds<T> GetLimits() const
		{
			return this->limits;
		}


		[[nodiscard]] LogLiteral GetName() const
		{
			return this->name;
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
		requires Details::IsPureBoolean<T>
		{
			for (const bool forRaces : {false, true})
			{
				for (const bool levelFlag : this->GetHeatLevelArray(forRaces))
					if (levelFlag) return true;
			}

			return false;
		}


		[[nodiscard]] bool AllTrue() const 
		requires Details::IsPureBoolean<T>
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
	class OptionalValue
	{
	private: // members

		[[no_unique_address]] LogLiteral name;


	public: // members (for ASM access)

		HEAT_PARAMETER_VALUE(bool, isEnabled, false);

		Value<T> value;


	public: // methods

		constexpr explicit OptionalValue
		(
			const LogLiteral name, 
			const Bounds<T>& limits = {}
		)
			requires Details::IsBoundsCompatible<T>
			: name(name), value("value", T(), limits) 
		{
		}


		constexpr OptionalValue(const LogLiteral name)
			requires (not Details::IsBoundsCompatible<T>)
			: name(name), value("value", T()) 
		{
		}


		void SetToHeatStateSilently(const HeatState state)
		{
			this->isEnabled.SetToHeatStateSilently(state);
			this->value    .SetToHeatStateSilently(state);
		}


		void SetToHeatState(const HeatState state) 
		{
			this->SetToHeatStateSilently(state);

			if constexpr (Globals::loggingEnabled)
			{
				if (this->isEnabled.current)
					LogParameter(this->name, this->value.current);

				else LogMissingParameter(this->name);
			}
		}


		[[nodiscard]] LogLiteral GetName() const
		{
			return this->name;
		}
	};





	// Pointer-parameter classes --------------------------------------------------------------------------------------------------------------------

	#define HEAT_PARAMETER_POINTER(type, name) HeatParameters::Pointer<type> name{#name}

	template <class T>
	requires (not Details::IsCopyCompatible<T>)
	class Pointer
	{
	private: // members

		[[no_unique_address]] LogLiteral name;


	public: // members (for ASM access)

		const T* current = nullptr;

		HeatLevelArray<T> roam = {};
		HeatLevelArray<T> race = {};


	public: // methods

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


		void SetToHeatStateSilently(const HeatState state)
		{
			this->current = &(this->GetHeatStateEntry(state));
		}


		void SetToHeatState(const HeatState state) 
		requires Details::IsLoggable<T>
		{
			this->SetToHeatStateSilently(state);

			if constexpr (Globals::loggingEnabled)
				this->current->Log(this->name);
		}


		[[nodiscard]] LogLiteral GetName() const
		{
			return this->name;
		}
	};



	#define OPTIONAL_HEAT_PARAMETER_POINTER(type, name) HeatParameters::OptionalPointer<type> name{#name}

	template <class T>
	requires (not Details::IsCopyCompatible<T>)
	class OptionalPointer
	{
	private: // members

		[[no_unique_address]] LogLiteral name;


	public: // members (for ASM access)

		HEAT_PARAMETER_VALUE(bool, isEnabled, false);

		Pointer<T> pointer;


	public: // methods

		constexpr explicit OptionalPointer(const LogLiteral name) : name(name), pointer("pointer") {}


		void SetToHeatStateSilently(const HeatState state)
		{
			this->isEnabled.SetToHeatStateSilently(state);
			this->pointer  .SetToHeatStateSilently(state);
		}


		void SetToHeatState(const HeatState state) 
		requires Details::IsLoggable<T>
		{
			this->SetToHeatStateSilently(state);

			if constexpr (Globals::loggingEnabled)
			{
				if (this->isEnabled.current)
					this->current->Log(this->name);

				else LogMissingParameter(this->name);
			}
		}


		[[nodiscard]] LogLiteral GetName() const
		{
			return this->name;
		}
	};





	// Interval-parameter classes -------------------------------------------------------------------------------------------------------------------

	#define HEAT_PARAMETER_INTERVAL(type, name, ...) HeatParameters::Interval<type> name{#name, __VA_ARGS__}

	template <typename T>
	requires Details::IsBoundsCompatible<T>
	class Interval
	{
	private: // members

		[[no_unique_address]] LogLiteral name;


	public: // members (for ASM access)

		Value<T> min;
		Value<T> max;


	public: // methods

		constexpr Interval
		(
			const LogLiteral name,
			const T          initialMin,
			const T          initialMax,
			const Bounds<T>& limits = {}
		) 
			: name(name), min("min", initialMin, limits), max("max", initialMax, limits)
		{
		}


		void SetToHeatStateSilently(const HeatState state)
		{
			this->min.SetToHeatStateSilently(state);
			this->max.SetToHeatStateSilently(state);
		}


		void SetToHeatState(const HeatState state)
		{
			this->SetToHeatStateSilently(state);

			if constexpr (Globals::loggingEnabled)
				LogParameter(this->name, this->min.current, "to", this->max.current);
		}


		[[nodiscard]] LogLiteral GetName() const
		{
			return this->name;
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
	class OptionalInterval
	{
	private: // members

		[[no_unique_address]] LogLiteral name;


	public: // members (for ASM access)

		HEAT_PARAMETER_VALUE(bool, isEnabled, false);

		Interval<T> interval;


	public: // methods

		constexpr explicit OptionalInterval
		(
			const LogLiteral name, 
			const Bounds<T>& limits = {}
		) 
			: name(name), interval("interval", T(), T(), limits) 
		{
		}


		void SetToHeatStateSilently(const HeatState state)
		{
			this->isEnabled.SetToHeatStateSilently(state);
			this->interval .SetToHeatStateSilently(state);
		}


		void SetToHeatState(const HeatState state)
		{
			this->SetToHeatStateSilently(state);

			if constexpr (Globals::loggingEnabled)
			{
				if (this->isEnabled.current)
					LogParameter(this->name, this->interval.min.current, "to", this->interval.max.current);

				else LogMissingParameter(this->name);
			}
		}


		[[nodiscard]] LogLiteral GetName() const
		{
			return this->name;
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

		PersistentStrings::Make(vehicleValue.current); // initial (i.e. vanilla) value by default

		for (const bool forRaces : {false, true})
		{
			size_t heatLevel = 0;

			for (T& levelVehicleName : vehicleValue.GetHeatLevelArray(forRaces))
			{
				++heatLevel;

				const vault vehicleType = Globals::GetVaultHash(levelVehicleName);

				if (IsVehicleTypeValid(vehicleType))
				{
					PersistentStrings::Make(vehicleType, levelVehicleName);

					continue; // vehicle valid
				}

				if constexpr (Globals::loggingEnabled)
				{
					if (allTypesValid)
						Globals::LogPlain(vehicleValue.GetName(), (forRaces) ? "(race)" : "(roam)");

					Globals::LogDetail(LogDec(heatLevel), levelVehicleName, "->", vehicleValue.current);
				}

				levelVehicleName = vehicleValue.current; // already persistent

				allTypesValid = false;
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
		template <typename T>
		using ArrayField = ConfigParser::ArrayField<T, maxHeatLevel>;


		template <typename T> struct IsRegular              : std::false_type {};
		template <typename T> struct IsRegular<Value   <T>> : std::true_type  {};
		template <typename T> struct IsRegular<Interval<T>> : std::true_type  {};

		template <typename ...Ts>
		concept AreRegular = ((sizeof...(Ts) > 0) and ... and IsRegular<Ts>::value);


		template <typename T> struct IsOptional                      : std::false_type {};
		template <typename T> struct IsOptional<OptionalValue   <T>> : std::true_type  {};
		template <typename T> struct IsOptional<OptionalInterval<T>> : std::true_type  {};

		template <typename ...Ts>
		concept AreOptional = ((sizeof...(Ts) > 0) and ... and IsOptional<Ts>::value);


		template <class ...HeatParameters>
		concept AreExtractable = (AreRegular<HeatParameters...> or AreOptional<HeatParameters...>);



		template <typename T>
		[[nodiscard]] ArrayField<T> AsFieldWithDefault
		(
			const bool forRaces,
			Value<T>&  value
		) {
			return {value.GetHeatLevelArray(forRaces), value.current, value.GetLimits()};
		}


		template <typename T>
		[[nodiscard]] ArrayField<T> AsFieldWithoutDefault
		(
			const bool forRaces,
			Value<T>&  value
		) {
			return {value.GetHeatLevelArray(forRaces), std::nullopt, value.GetLimits()};
		}



		template <typename T>
		[[nodiscard]] auto GetFields
		(
			const bool forRaces,
			Value<T>&  value
		) {
			return std::tuple
			(
				AsFieldWithDefault<T>(forRaces, value)
			);
		}


		template <typename T>
		[[nodiscard]] auto GetFields
		(
			const bool        forRaces,
			OptionalValue<T>& optionalValue
		) {
			return std::tuple
			(
				AsFieldWithoutDefault<T>(forRaces, optionalValue.value)
			);
		}



		template <typename T>
		[[nodiscard]] auto GetFields
		(
			const bool   forRaces,
			Interval<T>& interval
		) {
			return std::tuple
			(
				AsFieldWithDefault<T>(forRaces, interval.min),
				AsFieldWithDefault<T>(forRaces, interval.max)
			);
		}


		template <typename T>
		[[nodiscard]] auto GetFields
		(
			const bool           forRaces,
			OptionalInterval<T>& optionalInterval
		) {
			return std::tuple
			(
				AsFieldWithoutDefault<T>(forRaces, optionalInterval.interval.min),
				AsFieldWithoutDefault<T>(forRaces, optionalInterval.interval.max)
			);
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



		void DoPostProcessing(auto&) {}


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





	// Parameter-extraction function ----------------------------------------------------------------------------------------------------------------

	template <class ...HeatParameters>
	requires Details::AreExtractable<HeatParameters...>
	void Extract
	(
		const ConfigParser::Parser::Section* const    section,
		HeatParameters&                            ...parameters
	) {
		for (const bool forRaces : {false, true})
		{
			// Extract arrays for free-roam / race Heat-levels separately
			const auto ExtractArrays = [forRaces, section](auto&& ...fields) -> HeatLevelArray<bool>
			{
				return ConfigParser::Parser::ExtractArrays
				(
					section,
					configDefaultKey,
					(forRaces) ? configFormatRace : configFormatRoam,
					configFormatStart,
					fields...
				);
			};

			const auto isEnableds = std::apply(ExtractArrays, std::tuple_cat(Details::GetFields(forRaces, parameters)...));

			// For optionals, successful exraction enables them
			if constexpr (Details::AreOptional<HeatParameters...>)
				(..., (parameters.isEnabled.GetHeatLevelArray(forRaces) = isEnableds));
		}

		(..., Details::DoPostProcessing(parameters));
	}



	template <class ...HeatParameters>
	requires Details::AreExtractable<HeatParameters...>
	void Extract
	(
		const ConfigParser::Parser::Section&    section,
		HeatParameters&                      ...parameters
	) {
		Extract<HeatParameters...>(&section, parameters...);
	}



	template <class ...HeatParameters>
	requires Details::AreExtractable<HeatParameters...>
	void Extract
	(
		const ConfigParser::Parser&    parser,
		const std::string_view         sectionName,
		HeatParameters&             ...parameters
	) {
		const auto* const section = parser.GetSection(sectionName);
		Extract<HeatParameters...>(section, parameters...);
	}
}