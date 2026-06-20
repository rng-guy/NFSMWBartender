#pragma once

#include <array>
#include <tuple>
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



namespace HeatParameters
{
	
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Heat parameters
	constexpr size_t maxHeatLevel = 10;
	constexpr float  maxHeat      = static_cast<float>(maxHeatLevel);

	// Configuration files
	constexpr const char* configDefaultKey  = "default"; // C-style for game compatibility
	constexpr size_t      configFormatStart = 1;

	constexpr std::format_string<size_t> configFormatRoam = "heat{:02}";
	constexpr std::format_string<size_t> configFormatRace = "race{:02}";

	const std::filesystem::path configPathMain     = "scripts/BartenderSettings";
	const std::filesystem::path configPathBasic    = configPathMain / "Basic";
	const std::filesystem::path configPathAdvanced = configPathMain / "Advanced";





	// Scoped aliases -------------------------------------------------------------------------------------------------------------------------------

	using Parser = ConfigParser::Parser;

	template <typename T>
	using Bounds = ConfigParser::Bounds<T>;

	template <typename T>
	using Values = std::array<T, maxHeatLevel>;

	template <typename T>
	using Format = ConfigParser::Format<T, maxHeatLevel>;





	// Heat-level indices ---------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] consteval auto GenerateHeatLevelIDs()
	{
		Values<size_t> heatLevelIDs = {};

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
		concept IsPureArithmetic = (std::is_arithmetic_v<T> and std::same_as<T, std::remove_cvref_t<T>>);;

		template <typename T>
		concept IsIntervalCompatible = (IsPureArithmetic<T> and IsBoundsCompatible<T>);

		template <typename T>
		concept IsCopyPairCompatible = (IsPureArithmetic<T> or std::same_as<T, const char*> or std::same_as<T, std::string_view>);
	}





	// HeatParameter structs ------------------------------------------------------------------------------------------------------------------------

	template <typename T>
	struct BasePair
	{
	protected:

		constexpr BasePair() = default;



	public:

		Values<T> roam = {};
		Values<T> race = {};


		[[nodiscard]] Values<T>& GetValues(const bool forRaces)
		{
			return (forRaces) ? this->race : this->roam;
		}


		[[nodiscard]] const Values<T>& GetValues(const bool forRaces) const
		{
			return (forRaces) ? this->race : this->roam;
		}
	};



	template <typename T>
	requires Concepts::IsCopyPairCompatible<T>
	struct Pair : public BasePair<T>
	{
		T current;

		Bounds<T> limits;


		constexpr explicit Pair
		(
			const T         original, 
			const Bounds<T> limits = {}
		) 
		requires (Concepts::IsBoundsCompatible<T>) : current(original), limits(limits) {}


		constexpr explicit Pair(const T original) 
		requires (not Concepts::IsBoundsCompatible<T>) : current(original), limits({}) {}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->current = this->GetValues(forRaces)[heatLevel - 1];
		}


		[[nodiscard]] T GetMinimum() const
		{
			T minimum = this->roam[0];

			for (const bool forRaces : {false, true})
			{
				for (const T value : this->GetValues(forRaces))
					minimum = std::min<T>(minimum, value);
			}

			return minimum;
		}


		[[nodiscard]] T GetMaximum() const
		{
			T maximum = this->roam[0];

			for (const bool forRaces : {false, true})
			{
				for (const T value : this->GetValues(forRaces))
					maximum = std::max<T>(maximum, value);
			}

			return maximum;
		}


		void Log(const std::string_view pairName) const
		{
			Globals::logger.Log<2>(pairName, this->current);
		}
	};



	template <typename T>
	requires (not Concepts::IsCopyPairCompatible<T>)
	struct PointerPair : public BasePair<T>
	{
		const T* current = &(this->roam[0]);


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->current = &(this->GetValues(forRaces)[heatLevel - 1]);
		}
	};



	template <>
	struct PointerPair<std::string> : public BasePair<std::string>
	{
		const char* current;


		constexpr explicit PointerPair(const char* const original) : current(original) {}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->current = (this->GetValues(forRaces)[heatLevel - 1]).c_str();
		}


		void Log(const std::string_view pairName) const
		{
			Globals::logger.Log<2>(pairName, this->current);
		}
	};



	template <typename T>
	requires Concepts::IsCopyPairCompatible<T>
	struct OptionalPair
	{
		Pair<bool> isEnableds{false};

		Pair<T> values;


		constexpr explicit OptionalPair(const Bounds<T> limits = {})
		requires (Concepts::IsBoundsCompatible<T>) : values(T(), limits) {}


		constexpr OptionalPair() 
		requires (not Concepts::IsBoundsCompatible<T>) : values(T()) {}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->isEnableds.SetToHeat(forRaces, heatLevel);
			this->values    .SetToHeat(forRaces, heatLevel);
		}


		void Log(const std::string_view optionalName) const
		{
			if (this->isEnableds.current)
				Globals::logger.Log<2>(optionalName, this->values.current);
		}
	};



	template <typename T>
	requires (not Concepts::IsCopyPairCompatible<T>)
	struct OptionalPointerPair
	{
		Pair<bool> isEnableds{false};

		PointerPair<T> values;


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->isEnableds.SetToHeat(forRaces, heatLevel);
			this->values    .SetToHeat(forRaces, heatLevel);
		}
	};



	template <>
	struct OptionalPointerPair<std::string>
	{
		Pair<bool> isEnableds{false};

		PointerPair<std::string> values{nullptr};


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->isEnableds.SetToHeat(forRaces, heatLevel);
			this->values    .SetToHeat(forRaces, heatLevel);
		}


		void Log(const std::string_view optionalName) const
		{
			if (this->isEnableds.current)
				Globals::logger.Log<2>(optionalName, this->values.current);
		}
	};



	template <typename T>
	requires Concepts::IsIntervalCompatible<T>
	struct Interval
	{
		Pair<T> minValues;
		Pair<T> maxValues;


		constexpr explicit Interval
		(
			const T         originalMin,
			const T         originalMax,
			const Bounds<T> limits = {}
		) 
			: minValues(originalMin, limits),
			  maxValues(originalMax, limits)
		{
		}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->minValues.SetToHeat(forRaces, heatLevel);
			this->maxValues.SetToHeat(forRaces, heatLevel);
		}


		[[nodiscard]] T GetMinimum() const
		{
			return this->minValues.GetMinimum();
		}


		[[nodiscard]] T GetMaximum() const
		{
			return this->maxValues.GetMaximum();
		}


		[[nodiscard]] T GetRandomValue() const
		{
			return Globals::prng.GenerateNumber<T>(this->minValues.current, this->maxValues.current);
		}


		void Log(const std::string_view intervalName) const
		{
			Globals::logger.Log<2>(intervalName, this->minValues.current, "to", this->maxValues.current);
		}
	};



	template <typename T>
	requires Concepts::IsIntervalCompatible<T>
	struct OptionalInterval
	{
		Pair<bool> isEnableds{false};

		Pair<T> minValues;
		Pair<T> maxValues;


		constexpr explicit OptionalInterval(const Bounds<T> limits = {}) : minValues(T(), limits), maxValues(T(), limits) {}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->isEnableds.SetToHeat(forRaces, heatLevel);
			this->minValues .SetToHeat(forRaces, heatLevel);
			this->maxValues .SetToHeat(forRaces, heatLevel);
		}


		[[nodiscard]] T GetRandomValue() const
		{
			return Globals::prng.GenerateNumber<T>(this->minValues.current, this->maxValues.current);
		}


		void Log(const std::string_view intervalName) const
		{
			if (this->isEnableds.current)
				Globals::logger.Log<2>(intervalName, this->minValues.current, "to", this->maxValues.current);
		}
	};





	// Validation functions -------------------------------------------------------------------------------------------------------------------------

	template <typename Predicate>
	requires std::predicate<Predicate, vault>
	bool ValidateVehicleTypes
	(
		const std::string_view    pairName,
		PointerPair<std::string>& vehiclePair,
		const Predicate&          IsVehicleTypeValid
	) {
		bool allValid = true;

		for (const bool forRaces : {false, true})
		{
			size_t heatLevel = 1;

			for (std::string& vehicle : vehiclePair.GetValues(forRaces))
			{
				const vault type = Globals::GetVaultKey(vehicle.c_str());

				if (not IsVehicleTypeValid(type))
				{
					if constexpr (Globals::loggingEnabled)
					{
						if (allValid)
							Globals::logger.Log<2>(pairName, (forRaces) ? "(race)" : "(roam)");

						Globals::logger.Log<3>(static_cast<int>(heatLevel), vehicle, "->", vehiclePair.current);
					}

					vehicle  = vehiclePair.current;
					allValid = false;
				}

				++heatLevel;
			}
		}

		return allValid;
	}





	// Parsing helpers ------------------------------------------------------------------------------------------------------------------------------

	namespace Details
	{

		template <typename T> struct IsRegular                 : std::false_type {};
		template <typename T> struct IsRegular<Interval   <T>> : std::true_type  {};
		template <typename T> struct IsRegular<PointerPair<T>> : std::true_type  {};
		template <typename T> struct IsRegular<Pair       <T>> : std::true_type  {};

		template <typename ...T>
		concept AreRegular = (IsRegular<T>::value and ...);

		template <typename T> struct IsOptional                         : std::false_type {};
		template <typename T> struct IsOptional<OptionalInterval   <T>> : std::true_type  {};
		template <typename T> struct IsOptional<OptionalPointerPair<T>> : std::true_type  {};
		template <typename T> struct IsOptional<OptionalPair       <T>> : std::true_type  {};

		template <typename ...T>
		concept AreOptional = (IsOptional<T>::value and ...);



		template <typename T>
		[[nodiscard]] auto MakeFormatTuple
		(
			const bool forRaces,
			Pair<T>&   pair
		) {
			// Regular pairs have one array per setting (race / roam), a default value, and limits
			auto defaultValue = (forRaces) ? std::nullopt : std::optional<T>(pair.current);
			return std::tuple(Format<T>(pair.GetValues(forRaces), std::move(defaultValue), pair.limits));
		}


		template <typename T>
		[[nodiscard]] auto MakeFormatTuple
		(
			const bool      forRaces,
			PointerPair<T>& pair
		) {
			// Regular pointer-pairs have no limits
			auto defaultValue = (forRaces) ? std::nullopt : std::optional<T>(pair.current);
			return std::tuple(Format<T>(pair.GetValues(forRaces), std::move(defaultValue)));
		}


		template <typename T>
		[[nodiscard]] auto MakeFormatTuple
		(
			const bool   forRaces,
			Interval<T>& interval
		) {
			// Regular intervals consist of two regular pairs
			return std::tuple_cat
			(
				MakeFormatTuple<T>(forRaces, interval.minValues), 
				MakeFormatTuple<T>(forRaces, interval.maxValues)
			);
		}


		template <typename T>
		[[nodiscard]] auto MakeFormatTuple
		(
			const bool       forRaces,
			OptionalPair<T>& pair
		) {
			// Optional pairs have no default value
			return std::tuple(Format<T>(pair.values.GetValues(forRaces), std::nullopt, pair.values.limits));
		}


		template <typename T>
		[[nodiscard]] auto MakeFormatTuple
		(
			const bool              forRaces,
			OptionalPointerPair<T>& pair
		) {
			// Optional pointer-pairs have no default value and no limits
			return std::tuple(Format<T>(pair.values.GetValues(forRaces), std::nullopt));
		}


		template <typename T>
		[[nodiscard]] auto MakeFormatTuple
		(
			const bool           forRaces,
			OptionalInterval<T>& interval
		) {
			// Optional intervals consist of two pairs without default values
			return std::tuple
			(
				Format<T>(interval.minValues.GetValues(forRaces), std::nullopt, interval.minValues.limits),
				Format<T>(interval.maxValues.GetValues(forRaces), std::nullopt, interval.maxValues.limits)
			);
		}



		template <typename T>
		void CopyRoamToRaceValues(Pair<T>& pair)
		{
			// The race values of regular pairs fall back to their roam values
			pair.race = pair.roam;
		}


		template <typename T>
		void CopyRoamToRaceValues(PointerPair<T>& pair)
		{
			// The race values of regular pointer-pairs fall back to their roam values
			pair.race = pair.roam;
		}


		template <typename T>
		void CopyRoamToRaceValues(Interval<T>& interval)
		{
			// The race values of regular intervals fall back to their roam values
			interval.minValues.race = interval.minValues.roam;
			interval.maxValues.race = interval.maxValues.roam;
		}



		template <class ...HeatParameters>
		requires (AreRegular<HeatParameters...> or AreOptional<HeatParameters...>)
		Values<bool> ParsePartial
		(
			const bool                forRaces,
			const Parser&             parser,
			const std::string_view    section,
			HeatParameters&        ...parameters
		) {
			auto formats = std::tuple_cat(MakeFormatTuple(forRaces, parameters)...);

			return [&]<size_t... formatIDs>(std::index_sequence<formatIDs...>) -> Values<bool>
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
		void OrderIntervals
		(
			Pair<T>&       minValues,
			const Pair<T>& maxValues
		) {
			for (const bool forRaces : {false, true})
			{
				Values<T>&       lowers = minValues.GetValues(forRaces);
				const Values<T>& uppers = maxValues.GetValues(forRaces);

				for (const size_t heatLevelID : heatLevelIDs)
					lowers[heatLevelID] = std::min<T>(lowers[heatLevelID], uppers[heatLevelID]);
			}
		}



		void ValidateIntervals(const auto&) {}


		template <typename T>
		void ValidateIntervals(Interval<T>& interval)
		{
			// Regular intervals must have correctly ordered values
			OrderIntervals<T>(interval.minValues, interval.maxValues);
		}


		template <typename T>
		void ValidateIntervals(OptionalInterval<T>& interval)
		{
			// Optional intervals must have correctly ordered values
			OrderIntervals<T>(interval.minValues, interval.maxValues);
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
		// Parse roam values, using internal default values as fallback
		Details::ParsePartial<HeatParameters...>(/* forRaces = */ false, parser, section, parameters...);

		// Use roam values as initial race values
		(..., Details::CopyRoamToRaceValues(parameters));

		// Parse race values, using copied roam values as fallback
		Details::ParsePartial<HeatParameters...>(/* forRaces = */ true, parser, section, parameters...);

		// Enforce proper value-ordering in intervals
		(..., Details::ValidateIntervals(parameters));
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
			// Parse roam / race values without fallbacks, storing which ones could be parsed successfully
			const auto isEnableds = Details::ParsePartial<HeatParameters...>(forRaces, parser, section, parameters...);

			// Mark successfully parsed values as enabled
			(..., (parameters.isEnableds.GetValues(forRaces) = isEnableds));
		}

		// Enforce proper value-ordering in intervals
		(..., Details::ValidateIntervals(parameters));
	}
}