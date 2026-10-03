#pragma once

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"
#include "../../Common/HeatParameters.hpp"

#include "../../Utilities/MemoryTools.hpp"

#include "GameBreaker.hpp"
#include "NitrousCharge.hpp"
#include "RadioSpeech.hpp"
#include "CopDetection.hpp"
#include "GroundSupport.hpp"
#include "GeneralSettings.hpp"
#include "HelicopterVision.hpp"
#include "InteractiveMusic.hpp"
#include "CopNotifications.hpp"



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
		anyFeatureEnabled |= GeneralSettings ::Initialise(parser);
		anyFeatureEnabled |= GroundSupport   ::Initialise(parser);
		anyFeatureEnabled |= NitrousCharge   ::Initialise(parser);
		anyFeatureEnabled |= GameBreaker     ::Initialise(parser);

		if (anyFeatureEnabled)
		{
			// Feature-specific fixes
			RadioSpeech     ::ApplyFixes();
			CopDetection    ::ApplyFixes();
			HelicopterVision::ApplyFixes();
			GeneralSettings ::ApplyFixes();
			GroundSupport   ::ApplyFixes();

			// Incorrect helicopter blob-shadow
			MemoryTools::Write<float>(0.f, {0x903660});

			// Hard-coded Heat-level resets (credit: ExOptsTeam)
			MemoryTools::Write<float>       (HeatParameters::maxHeat,    {0x7BB502, 0x7B1387, 0x7B0C89, 0x7B4D7C, 0x435088});
			MemoryTools::Write<const float*>(&(HeatParameters::maxHeat), {0x435079, 0x7A5B03, 0x7A5B12});
		}

		return anyFeatureEnabled;
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		// Sentinel not needed as each feature has own check

		RadioSpeech    ::SetToHeatState(state);
		GeneralSettings::SetToHeatState(state);
		GroundSupport  ::SetToHeatState(state);
		NitrousCharge  ::SetToHeatState(state);
		GameBreaker    ::SetToHeatState(state);
	}



	void NotifyOfTaggedCop
	(
		const address copVehicle, 
		const address perpVehicle
	) {
		// Sentinel not needed as each feature has own check

		NitrousCharge::NotifyOfTaggedCop(copVehicle, perpVehicle);
		GameBreaker  ::NotifyOfTaggedCop(copVehicle, perpVehicle);
	}



	void NotifyOfAssaultedCop
	(
		const address copVehicle,
		const address perpVehicle,
		const byte    numCopAssaulted
	) {
		// Sentinel not needed as each feature has own check

		NitrousCharge::NotifyOfAssaultedCop(copVehicle, perpVehicle, numCopAssaulted);
		GameBreaker  ::NotifyOfAssaultedCop(copVehicle, perpVehicle, numCopAssaulted);
	}



	void NotifyOfFinishedCollision(const address perpVehicle)
	{
		// Sentinel not needed as each feature has own check

		NitrousCharge::NotifyOfFinishedCollision(perpVehicle);
		GameBreaker  ::NotifyOfFinishedCollision(perpVehicle);
	}



	void NotifyOfDestroyedCop
	(
		const address pursuit, 
		const address copVehicle
	) {
		// Sentinel not needed as each feature has own check

		NitrousCharge::NotifyOfDestroyedCop(pursuit, copVehicle);
		GameBreaker  ::NotifyOfDestroyedCop(pursuit, copVehicle);
	}
}