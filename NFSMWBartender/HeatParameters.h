#pragma once

#include <array>
#include <tuple>
#include <string>
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
	const char* const configDefaultKey  = "default"; // for game compatibility
	constexpr size_t  configFormatStart = 1;

	constexpr std::string_view configFormatRoam = "heat{:02}";
	constexpr std::string_view configFormatRace = "race{:02}";

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

	consteval Values<size_t> GenerateHeatLevels()
	{
		Values<size_t> heatLevels = {};

		for (size_t heatID = 0; heatID < maxHeatLevel; ++heatID)
			heatLevels[heatID] = heatID + 1;

		return heatLevels;
	}

	constexpr Values<size_t> heatLevels = GenerateHeatLevels();





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


		constexpr Values<T>& GetValues(const bool forRaces) noexcept
		{
			return (forRaces) ? this->race : this->roam;
		}


		constexpr const Values<T>& GetValues(const bool forRaces) const noexcept
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


		explicit constexpr Pair
		(
			const T         original, 
			const Bounds<T> limits = {}
		) 
		requires (Concepts::IsBoundsCompatible<T>) : current(original), limits(limits) {}


		explicit constexpr Pair(const T original) 
		requires (not Concepts::IsBoundsCompatible<T>) : current(original), limits({}) {}


		constexpr void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) 
			noexcept
		{
			this->current = this->GetValues(forRaces)[heatLevel - 1];
		}


		constexpr T GetMinimum() const noexcept
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


		constexpr T GetMaximum() const noexcept
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


		constexpr void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) 
			noexcept
		{
			this->current = &(this->GetValues(forRaces)[heatLevel - 1]);
		}
	};



	template <>
	struct PointerPair<std::string> : public BasePair<std::string>
	{
		const char* current;


		explicit constexpr PointerPair(const char* const original) : current(original) {}


		constexpr void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) 
			noexcept
		{
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


		explicit constexpr OptionalPair(const Bounds<T> limits = {})
		requires (Concepts::IsBoundsCompatible<T>) : values(T(), limits) {}


		constexpr OptionalPair() 
		requires (not Concepts::IsBoundsCompatible<T>) : values(T()) {}


		constexpr void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) 
			noexcept
		{
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


		constexpr void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) 
			noexcept
		{
			this->isEnableds.SetToHeat(forRaces, heatLevel);
			this->values    .SetToHeat(forRaces, heatLevel);
		}
	};



	template <>
	struct OptionalPointerPair<std::string>
	{
		Pair<bool> isEnableds{false};

		PointerPair<std::string> values{nullptr};


		constexpr void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) 
			noexcept
		{
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


		explicit constexpr Interval
		(
			const T         originalMin,
			const T         originalMax,
			const Bounds<T> limits = {}
		) 
			: minValues(originalMin, limits),
			  maxValues(originalMax, limits)
		{
		}


		constexpr void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) 
			noexcept
		{
			this->minValues.SetToHeat(forRaces, heatLevel);
			this->maxValues.SetToHeat(forRaces, heatLevel);
		}


		constexpr T GetMinimum() const noexcept
		{
			return this->minValues.GetMinimum();
		}


		constexpr T GetMaximum() const noexcept
		{
			return this->maxValues.GetMaximum();
		}


		T GetRandomValue() const noexcept
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


		explicit constexpr OptionalInterval(const Bounds<T> limits = {}) : minValues(T(), limits), maxValues(T(), limits) {}


		constexpr void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) 
			noexcept
		{
			this->isEnableds.SetToHeat(forRaces, heatLevel);
			this->minValues .SetToHeat(forRaces, heatLevel);
			this->maxValues .SetToHeat(forRaces, heatLevel);
		}


		T GetRandomValue() const noexcept
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

	bool ValidateVehicleTypes
	(
		const std::string_view    pairName,
		PointerPair<std::string>& pointerPair,
		const auto&               IsVehicleTypeValid
	) {
		size_t numTotalReplaced = 0;

		for (const bool forRaces : {false, true})
		{
			size_t numReplaced = 0;

			auto& vehicles = pointerPair.GetValues(forRaces);

			for (const size_t heatLevel : heatLevels)
			{
				std::string& vehicle = vehicles[heatLevel - 1];

				if (not IsVehicleTypeValid(Globals::GetVaultKey(vehicle.c_str())))
				{
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

		template <typename ...T>
		concept AreRegular = (IsRegular<T>::value and ...);

		template <typename T> struct IsOptional                         : std::false_type {};
		template <typename T> struct IsOptional<OptionalInterval   <T>> : std::true_type  {};
		template <typename T> struct IsOptional<OptionalPointerPair<T>> : std::true_type  {};
		template <typename T> struct IsOptional<OptionalPair       <T>> : std::true_type  {};

		template <typename ...T>
		concept AreOptional = (IsOptional<T>::value and ...);



		template <typename T>
		constexpr auto MakeFormatTuple
		(
			const bool forRaces,
			Pair<T>&   pair
		) 
			noexcept
		{
			auto defaultValue = (forRaces) ? std::nullopt : std::optional<T>(pair.current);

			return std::tuple(Format<T>(pair.GetValues(forRaces), std::move(defaultValue), pair.limits));
		}


		template <typename T>
		constexpr auto MakeFormatTuple
		(
			const bool      forRaces,
			PointerPair<T>& pair
		) {
			auto defaultValue = (forRaces) ? std::nullopt : std::optional<T>(pair.current);

			return std::tuple(Format<T>(pair.GetValues(forRaces), std::move(defaultValue)));
		}


		template <typename T>
		constexpr auto MakeFormatTuple
		(
			const bool   forRaces,
			Interval<T>& interval
		) 
			noexcept
		{
			return std::tuple_cat
			(
				MakeFormatTuple<T>(forRaces, interval.minValues), 
				MakeFormatTuple<T>(forRaces, interval.maxValues)
			);
		}


		template <typename T>
		constexpr auto MakeFormatTuple
		(
			const bool       forRaces,
			OptionalPair<T>& pair
		) 
			noexcept
		{
			return std::tuple(Format<T>(pair.values.GetValues(forRaces), std::nullopt, pair.values.limits));
		}


		template <typename T>
		constexpr auto MakeFormatTuple
		(
			const bool              forRaces,
			OptionalPointerPair<T>& pair
		) {
			return std::tuple(Format<T>(pair.values.GetValues(forRaces), std::nullopt));
		}


		template <typename T>
		constexpr auto MakeFormatTuple
		(
			const bool           forRaces,
			OptionalInterval<T>& interval
		) 
			noexcept
		{
			return std::tuple
			(
				Format<T>(interval.minValues.GetValues(forRaces), std::nullopt, interval.minValues.limits),
				Format<T>(interval.maxValues.GetValues(forRaces), std::nullopt, interval.maxValues.limits)
			);
		}



		template <typename T>
		constexpr void CopyRoamToRaceValues(Pair<T>& pair) noexcept
		{
			pair.race = pair.roam;
		}


		template <typename T>
		constexpr void CopyRoamToRaceValues(PointerPair<T>& pair) noexcept
		{
			pair.race = pair.roam;
		}


		template <typename T>
		constexpr void CopyRoamToRaceValues(Interval<T>& interval) noexcept
		{
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
		constexpr void CheckIntervals
		(
			Pair<T>&       minValues,
			const Pair<T>& maxValues
		) 
			noexcept
		{
			for (const bool forRaces : {false, true})
			{
				Values<T>&       lowers = minValues.GetValues(forRaces);
				const Values<T>& uppers = maxValues.GetValues(forRaces);

				for (const size_t heatLevel : heatLevels)
					lowers[heatLevel - 1] = std::min<T>(lowers[heatLevel - 1], uppers[heatLevel - 1]);
			}
		}



		constexpr void FinaliseParameters(const auto&) noexcept {}


		template <typename T>
		constexpr void FinaliseParameters(Interval<T>& interval) noexcept
		{
			CheckIntervals<T>(interval.minValues, interval.maxValues);
		}


		template <typename T>
		constexpr void FinaliseParameters(OptionalInterval<T>& interval) noexcept
		{
			CheckIntervals<T>(interval.minValues, interval.maxValues);
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
		for (const bool forRaces : {false, true})
		{
			if (forRaces)
				(..., Details::CopyRoamToRaceValues(parameters));

			Details::ParsePartial<HeatParameters...>(forRaces, parser, section, parameters...);
		}
			
		(..., Details::FinaliseParameters(parameters));
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
			const auto isEnableds = Details::ParsePartial<HeatParameters...>(forRaces, parser, section, parameters...);

			(..., (parameters.isEnableds.GetValues(forRaces) = isEnableds));
		}

		(..., Details::FinaliseParameters(parameters));
	}
}