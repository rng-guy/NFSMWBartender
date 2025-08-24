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



	// PursuitReaction class ------------------------------------------------------------------------------------------------------------------------

	class PursuitReaction
	{
	private:

		inline static bool isHeatLevelKnown = false;



	protected:

		static constexpr const float* simulationTime = (float*)0x9885D8;

		const address pursuit;

		friend void SetToHeat(const bool, const size_t);
		friend void ResetState();
		
		explicit PursuitReaction(const address pursuit) : pursuit(pursuit) {};



	public:

		virtual ~PursuitReaction() = default;


		virtual void UpdateOnGameplay()       {}
		virtual void UpdateOnHeatChange()     {}
		virtual void UpdateOnceInPursuit()    {}
		virtual void UpdateOncePerHeatLevel() {}


		virtual void ProcessAddition
		(
			const address  copVehicle,
			const CopLabel copLabel
		) {};


		virtual void ProcessRemoval
		(
			const address  copVehicle,
			const CopLabel copLabel
		) {};


		static bool IsHeatLevelKnown() {return isHeatLevelKnown;}
	};





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		PursuitReaction::isHeatLevelKnown = true;
	}



	void ResetState() 
	{
		PursuitReaction::isHeatLevelKnown = false;
	}
}