#pragma once

#include <Windows.h>

#include "Globals.h"
#include "RandomNumbers.h"



namespace PursuitFeatures
{

	// CopLabel enum --------------------------------------------------------------------------------------------------------------------------------

	enum class CopLabel
	{
		CHASER,
		HEAVY,
		LEADER,
		ROADBLOCK,
		HELICOPTER,
		UNKNOWN
	};



	// CopVehicleReaction class ---------------------------------------------------------------------------------------------------------------------

	class CopVehicleReaction
	{
	private:

		inline static bool isHeatLevelKnown = false;



	protected:

		friend void SetToHeat(size_t);
		friend void ResetState();

		static constexpr const float* simulationTime = (float*)0x9885D8;
		inline static                 RandomNumbers::Generator prng;
		
		CopVehicleReaction() = default;



	public:

		virtual ~CopVehicleReaction() = default;

		CopVehicleReaction(const CopVehicleReaction&)            = delete;
		CopVehicleReaction& operator=(const CopVehicleReaction&) = delete;

		virtual void UpdateOnGameplay()       {}
		virtual void UpdateOnHeatChange()     {}
		virtual void UpdateOnceInPursuit()    {}
		virtual void UpdateOncePerHeatLevel() {}


		virtual void ProcessAddition
		(
			const address  copVehicle,
			const hash     copType,
			const CopLabel copLabel
		) = 0;


		virtual void ProcessRemoval
		(
			const address  copVehicle,
			const address  copType,
			const CopLabel copLabel
		) = 0;


		static bool IsHeatLevelKnown() {return isHeatLevelKnown;}
	};





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void SetToHeat(size_t heatLevel) {CopVehicleReaction::isHeatLevelKnown = true;}
	void ResetState()                {CopVehicleReaction::isHeatLevelKnown = false;}
}