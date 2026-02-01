#pragma once

#include <array>
#include <tuple>
#include <string>
#include <utility>
#include <optional>
#include <algorithm>

#include "Globals.h"
#include "ConfigParser.h"



namespace HeatParameters
{
	
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Heat parameters
	constexpr size_t maxHeatLevel = 10;
	constexpr float  maxHeat      = static_cast<float>(maxHeatLevel);

	// Configuration files
	const std::string defaultValueHandle = "default";

	const std::string configFormatRoam   = "heat{:02}";
	const std::string configFormatRace   = "race{:02}";
	const std::string configPathMain     = "scripts/BartenderSettings/";
	const std::string configPathBasic    = configPathMain + "Basic/";
	const std::string configPathAdvanced = configPathMain + "Advanced/";





	// Scoped aliases -------------------------------------------------------------------------------------------------------------------------------

	using Parser = ConfigParser::Parser;

	template <typename T>
	using Bounds = ConfigParser::Bounds<T>;

	template <typename T>
	using Values = std::array<T, maxHeatLevel>;

	template <typename T>
	using Format = ConfigParser::Format<T, maxHeatLevel>;





	// Heat-level indices ---------------------------------------------------------------------------------------------------------------------------

	constexpr Values<size_t> GenerateHeatLevels()
	{
		Values<size_t> heatLevels   = {};
		size_t         currentLevel = 1;

		for (size_t& heatLevel : heatLevels)
			heatLevel = currentLevel++;

		return heatLevels;
	}

	constexpr Values<size_t> heatLevels = GenerateHeatLevels();





	// HeatParameter structs ------------------------------------------------------------------------------------------------------------------------

	template <typename T>
	struct BasePair
	{
	protected:

		explicit BasePair() = default;



	public:

		Values<T> roam = {};
		Values<T> race = {};


		Values<T>& GetValues(const bool forRaces)
		{
			return (forRaces) ? this->race : this->roam;
		}


		const Values<T>& GetValues(const bool forRaces) const
		{
			return (forRaces) ? this->race : this->roam;
		}
	};



	template <typename T>
	requires std::is_arithmetic_v<T>
	struct Pair : public BasePair<T>
	{
		T current;

		Bounds<T> limits;


		explicit Pair
		(
			const T          original, 
			const Bounds<T>& limits = {}
		) 
			: current(original), limits(limits)
		{
		}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->current = this->GetValues(forRaces)[heatLevel - 1];
		}


		T GetMinimum() const
		{
			T minimum = this->roam[0];

			for (const bool forRaces : {false, true})
			{
				const Values<T>& values = this->GetValues(forRaces);

				for (const T value : values)
					minimum = std::min<T>(minimum, value);
			}

			return minimum;
		}


		T GetMaximum() const
		{
			T maximum = this->roam[0];

			for (const bool forRaces : {false, true})
			{
				const Values<T>& values = this->GetValues(forRaces);

				for (const T value : values)
					maximum = std::max<T>(maximum, value);
			}

			return maximum;
		}


		void Log(const std::string& pairName) const
		{
			Globals::logger.Log<2>(pairName, this->current);
		}
	};



	template <>
	struct Pair<bool> : public BasePair<bool>
	{
		bool current;


		explicit Pair(const bool original) : current(original) {}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->current = this->GetValues(forRaces)[heatLevel - 1];
		}


		void Log(const std::string& pairName) const
		{
			Globals::logger.Log<2>(pairName, this->current);
		}
	};



	template <typename T>
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


		explicit PointerPair(const char* const original) : current(original) {}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->current = (this->GetValues(forRaces)[heatLevel - 1]).c_str();
		}


		void Log(const std::string& pairName) const
		{
			Globals::logger.Log<2>(pairName, this->current);
		}
	};



	template <typename T>
	requires std::is_arithmetic_v<T>
	struct OptionalPair
	{
		Pair<bool> isEnableds{false};

		Pair<T> values;


		explicit OptionalPair(const Bounds<T>& limits = {}) : values(T(), limits) {}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->isEnableds.SetToHeat(forRaces, heatLevel);
			this->values    .SetToHeat(forRaces, heatLevel);
		}


		void Log(const std::string& optionalName) const
		{
			if (this->isEnableds.current)
				Globals::logger.Log<2>(optionalName, this->values.current);
		}
	};



	template <>
	struct OptionalPair<bool>
	{
		Pair<bool> isEnableds{false};

		Pair<bool> values{false};


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->isEnableds.SetToHeat(forRaces, heatLevel);
			this->values    .SetToHeat(forRaces, heatLevel);
		}


		void Log(const std::string& optionalName) const
		{
			if (this->isEnableds.current)
				Globals::logger.Log<2>(optionalName, this->values.current);
		}
	};



	template <typename T>
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


		void Log(const std::string& optionalName) const
		{
			if (this->isEnableds.current)
				Globals::logger.Log<2>(optionalName, this->values.current);
		}
	};



	template <typename T>
	requires (std::is_arithmetic_v<T> and (not std::is_same_v<T, bool>))
	struct Interval
	{
		Pair<T> minValues;
		Pair<T> maxValues;


		explicit Interval
		(
			const T          originalMin,
			const T          originalMax,
			const Bounds<T>& limits = {}
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


		T GetMinimum() const
		{
			return this->minValues.GetMinimum();
		}


		T GetMaximum() const
		{
			return this->maxValues.GetMaximum();
		}


		T GetRandomValue() const
		{
			return Globals::prng.GenerateNumber<T>(this->minValues.current, this->maxValues.current);
		}


		void Log(const std::string& intervalName) const
		{
			Globals::logger.Log<2>(intervalName, this->minValues.current, "to", this->maxValues.current);
		}
	};



	template <typename T>
	requires (std::is_arithmetic_v<T> and (not std::is_same_v<T, bool>))
	struct OptionalInterval
	{
		Pair<bool> isEnableds{false};

		Pair<T> minValues;
		Pair<T> maxValues;


		explicit OptionalInterval(const Bounds<T>& limits = {}) : minValues(T(), limits), maxValues(T(), limits) {}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->isEnableds.SetToHeat(forRaces, heatLevel);
			this->minValues .SetToHeat(forRaces, heatLevel);
			this->maxValues .SetToHeat(forRaces, heatLevel);
		}


		T GetRandomValue() const
		{
			return Globals::prng.GenerateNumber<T>(this->minValues.current, this->maxValues.current);
		}


		void Log(const std::string& intervalName) const
		{
			if (this->isEnableds.current)
				Globals::logger.Log<2>(intervalName, this->minValues.current, "to", this->maxValues.current);
		}
	};





	// Validation functions -------------------------------------------------------------------------------------------------------------------------

	bool ValidateVehicles
	(
		const std::string&        pairName,
		PointerPair<std::string>& pointerPair,
		const auto&               IsVehicleValid
	) {
		size_t numTotalReplaced = 0;

		for (const bool forRaces : {false, true})
		{
			size_t numReplaced = 0;

			auto& vehicles = pointerPair.GetValues(forRaces);

			for (const size_t heatLevel : heatLevels)
			{
				std::string& vehicle = vehicles[heatLevel - 1];

				if (not IsVehicleValid(Globals::StringToVaultKey(vehicle)))
				{
					// With logging disabled, the compiler optimises "pairName" away
					if constexpr (Globals::loggingEnabled)
					{
						if (numReplaced == 0)
							Globals::logger.Log<2>(pairName, (forRaces) ? "(race)" : "(roam)");

						Globals::logger.Log<3>(static_cast<int>(heatLevel), vehicle, "->", pointerPair.current);
					}

					vehicle = pointerPair.current;

					++numReplaced;
				}
			}

			numTotalReplaced += numReplaced;
		}

		return (numTotalReplaced == 0);
	}





	// Auxiliary parsing structs / functions --------------------------------------------------------------------------------------------------------

	namespace Details
	{

		template <typename T> struct IsRegular                 : std::false_type {};
		template <typename T> struct IsRegular<Interval   <T>> : std::true_type  {};
		template <typename T> struct IsRegular<PointerPair<T>> : std::true_type  {};
		template <typename T> struct IsRegular<Pair       <T>> : std::true_type  {};

		template <typename T> struct IsOptional                         : std::false_type {};
		template <typename T> struct IsOptional<OptionalInterval   <T>> : std::true_type  {};
		template <typename T> struct IsOptional<OptionalPointerPair<T>> : std::true_type  {};
		template <typename T> struct IsOptional<OptionalPair       <T>> : std::true_type  {};



		template <typename T>
		auto MakeFormatTuple
		(
			const bool forRaces,
			Pair<T>&   pair
		) {
			auto defaultValue = (forRaces) ? std::nullopt : std::optional<T>(pair.current);

			return std::tuple(Format<T>(pair.GetValues(forRaces), std::move(defaultValue), pair.limits));
		}


		template <>
		auto MakeFormatTuple<bool>
		(
			const bool  forRaces,
			Pair<bool>& pair
		) {
			auto defaultValue = (forRaces) ? std::nullopt : std::optional<bool>(pair.current);

			return std::tuple(Format<bool>(pair.GetValues(forRaces), std::move(defaultValue)));
		}


		template <typename T>
		auto MakeFormatTuple
		(
			const bool      forRaces,
			PointerPair<T>& pair
		) {
			auto defaultValue = (forRaces) ? std::nullopt : std::optional<T>(pair.current);

			return std::tuple(Format<T>(pair.GetValues(forRaces), std::move(defaultValue)));
		}


		template <typename T>
		auto MakeFormatTuple
		(
			const bool   forRaces,
			Interval<T>& interval
		) {
			return std::tuple_cat
			(
				MakeFormatTuple<T>(forRaces, interval.minValues), 
				MakeFormatTuple<T>(forRaces, interval.maxValues)
			);
		}


		template <typename T>
		auto MakeFormatTuple
		(
			const bool       forRaces,
			OptionalPair<T>& pair
		) {
			return std::tuple(Format<T>(pair.values.GetValues(forRaces), std::nullopt, pair.values.limits));
		}


		template <>
		auto MakeFormatTuple<bool>
		(
			const bool          forRaces,
			OptionalPair<bool>& pair
		) {
			return std::tuple(Format<bool>(pair.values.GetValues(forRaces), std::nullopt));
		}


		template <typename T>
		auto MakeFormatTuple
		(
			const bool              forRaces,
			OptionalPointerPair<T>& pair
		) {
			return std::tuple(Format<T>(pair.values.GetValues(forRaces), std::nullopt));
		}


		template <typename T>
		auto MakeFormatTuple
		(
			const bool           forRaces,
			OptionalInterval<T>& interval
		) {
			return std::tuple
			(
				Format<T>(interval.minValues.GetValues(forRaces), std::nullopt, interval.minValues.limits),
				Format<T>(interval.maxValues.GetValues(forRaces), std::nullopt, interval.maxValues.limits)
			);
		}



		template <typename T>
		void CopyRoamToRaceValues(T& pair)
		{
			pair.race = pair.roam;
		}


		template <typename T>
		void CopyRoamToRaceValues(Interval<T>& interval)
		{
			interval.minValues.race = interval.minValues.roam;
			interval.maxValues.race = interval.maxValues.roam;
		}



		template <typename ...T>
		Values<bool> ParsePartial
		(
			const bool            forRaces,
			Parser&               parser,
			const std::string&    section,
			T&                 ...parameters
		) {
			auto formats = std::tuple_cat(MakeFormatTuple(forRaces, parameters)...);

			return [&]<size_t... formatIDs>(std::index_sequence<formatIDs...>) -> Values<bool>
			{
				return parser.ParseFormat<maxHeatLevel>
				(
					section,
					(forRaces) ? configFormatRace : configFormatRoam,
					std::move(std::get<formatIDs>(formats))...
				);
			}
			(std::make_index_sequence<std::tuple_size_v<decltype(formats)>>{});
		}



		template <typename T>
		void CheckIntervals
		(
			Pair<T>&       minValues,
			const Pair<T>& maxValues
		) {
			for (const bool forRaces : {false, true})
			{
				Values<T>&       lowers = minValues.GetValues(forRaces);
				const Values<T>& uppers = maxValues.GetValues(forRaces);

				for (const size_t heatLevel : heatLevels)
					lowers[heatLevel - 1] = std::min<T>(lowers[heatLevel - 1], uppers[heatLevel - 1]);
			}
		}



		template <typename T>
		void FinaliseIntervals(const T& pair) {}


		template <typename T>
		void FinaliseIntervals(Interval<T>& interval)
		{
			CheckIntervals<T>(interval.minValues, interval.maxValues);
		}


		template <typename T>
		void FinaliseIntervals(OptionalInterval<T>& interval)
		{
			CheckIntervals<T>(interval.minValues, interval.maxValues);
		}
	}





	// Generic parsing function ---------------------------------------------------------------------------------------------------------------------

	template <typename ...T>
	requires (Details::IsRegular<T>::value and ...)
	void Parse
	(
		Parser&               parser,
		const std::string&    section,
		T&                 ...parameters
	) {
		for (const bool forRaces : {false, true})
		{
			if (forRaces)
				(..., Details::CopyRoamToRaceValues(parameters));

			Details::ParsePartial<T...>(forRaces, parser, section, parameters...);
		}
			
		(..., Details::FinaliseIntervals(parameters));
	}



	template <typename ...T>
	requires (Details::IsOptional<T>::value and ...)
	void Parse
	(
		Parser&               parser,
		const std::string&    section,
		T&                 ...parameters
	) {
		for (const bool forRaces : {false, true})
		{
			const auto isEnableds = Details::ParsePartial<T...>(forRaces, parser, section, parameters...);

			(..., (parameters.isEnableds.GetValues(forRaces) = isEnableds));
		}

		(..., Details::FinaliseIntervals(parameters));
	}
}