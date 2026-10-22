#pragma once

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"
#include "../../Common/HeatParameters.hpp"

#include "CopNotifications.hpp"
#include "RadioSpeech.hpp"
#include "CopDetection.hpp"
#include "HelicopterVision.hpp"
#include "InteractiveMusic.hpp"
#include "StatusSettings.hpp"
#include "GroundSupport.hpp"
#include "NitrousCharge.hpp"
#include "GameBreaker.hpp"



namespace BasicFeatures
{
	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[BSC]";
	constexpr Globals::LogLiteral logName = "BasicFeatures";





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	bool Initialise(ConfigParser::Parser& parser)
	{
		parser.ClearAllFiles();

		anyFeatureEnabled |= CopNotifications::Initialise(parser);
		anyFeatureEnabled |= RadioSpeech     ::Initialise(parser);
		anyFeatureEnabled |= CopDetection    ::Initialise(parser);
		anyFeatureEnabled |= HelicopterVision::Initialise(parser);
		anyFeatureEnabled |= InteractiveMusic::Initialise(parser);
		anyFeatureEnabled |= StatusSettings  ::Initialise(parser);
		anyFeatureEnabled |= GroundSupport   ::Initialise(parser);
		anyFeatureEnabled |= NitrousCharge   ::Initialise(parser);
		anyFeatureEnabled |= GameBreaker     ::Initialise(parser);

		if (anyFeatureEnabled)
		{
			RadioSpeech     ::ApplyFixes();
			CopDetection    ::ApplyFixes();
			HelicopterVision::ApplyFixes();
			StatusSettings  ::ApplyFixes();
			GroundSupport   ::ApplyFixes();
		}

		return anyFeatureEnabled;
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		// Sentinel not needed as features have own checks

		RadioSpeech   ::SetToHeatState(state);
		StatusSettings::SetToHeatState(state);
		GroundSupport ::SetToHeatState(state);
		NitrousCharge ::SetToHeatState(state);
		GameBreaker   ::SetToHeatState(state);
	}



	void NotifyOfTaggedCop
	(
		const address copVehicle, 
		const address perpVehicle
	) {
		// Sentinel not needed as features have own checks

		NitrousCharge::NotifyOfTaggedCop(copVehicle, perpVehicle);
		GameBreaker  ::NotifyOfTaggedCop(copVehicle, perpVehicle);
	}



	void NotifyOfAssaultedCop
	(
		const address copVehicle,
		const address perpVehicle,
		const byte    numCopAssaulted
	) {
		// Sentinel not needed as features have own checks

		NitrousCharge::NotifyOfAssaultedCop(copVehicle, perpVehicle, numCopAssaulted);
		GameBreaker  ::NotifyOfAssaultedCop(copVehicle, perpVehicle, numCopAssaulted);
	}



	void NotifyOfFinishedCollision(const address perpVehicle)
	{
		// Sentinel not needed as features have own checks

		NitrousCharge::NotifyOfFinishedCollision(perpVehicle);
		GameBreaker  ::NotifyOfFinishedCollision(perpVehicle);
	}



	void NotifyOfDestroyedCop
	(
		const address pursuit, 
		const address copVehicle
	) {
		// Sentinel not needed as features have own checks

		NitrousCharge::NotifyOfDestroyedCop(pursuit, copVehicle);
		GameBreaker  ::NotifyOfDestroyedCop(pursuit, copVehicle);
	}
}