#pragma once

#include "Globals.h"



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

		static constexpr const float* simulationTime = (float*)0x9885D8;

		friend void SetToHeat(const bool, const size_t);
		friend void ResetState();
		
		CopVehicleReaction() = default;



	public:

		virtual ~CopVehicleReaction() = default;


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

	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		CopVehicleReaction::isHeatLevelKnown = true;
	}



	void ResetState() 
	{
		CopVehicleReaction::isHeatLevelKnown = false;
	}
}