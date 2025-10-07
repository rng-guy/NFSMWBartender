#pragma once

#include "Globals.h"
#include "HeatParameters.h"



namespace PursuitFeatures
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool heatLevelKnown = false;





	// Cache class ----------------------------------------------------------------------------------------------------------------------------------

	class Cache
	{
	private:

		inline static constexpr address storageOffset = 0xD0;

		address& storage;



	public:

		address pursuitObserver = 0x0;
		address chasersManager  = 0x0;
		address strategyManager = 0x0;


		explicit Cache(const address pursuit) : storage(*((address*)(pursuit + this->storageOffset)))
		{
			this->storage = (address)this;
		}


		~Cache()
		{
			this->storage = 0x0;
		}


		static Cache* GetPointer(const address pursuit)
		{
			return (Cache*)(*((address*)(pursuit + Cache::storageOffset)));
		}
	};





	// PursuitReaction class ------------------------------------------------------------------------------------------------------------------------

	class PursuitReaction
	{
	protected:

		const address pursuit;

		inline static const auto IsVehicleDestroyed = (bool (__thiscall*)(address))0x688170;


		static bool MakeVehicleBail(const address copVehicle)
		{
			static const auto StartFlee    = (void(__thiscall*)(address))0x423370;
			const address     copAIVehicle = *((address*)(copVehicle + 0x54));

			if (copAIVehicle)
				StartFlee(copAIVehicle + 0x70C);

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [PFT] Invalid AIVehicle pointer for", copVehicle);

			return copAIVehicle;
		}


		explicit PursuitReaction(const address pursuit) : pursuit(pursuit) {};



	public:

		virtual ~PursuitReaction() = default;

		virtual void UpdateOnGameplay()       {}
		virtual void UpdateOnHeatChange()     {}
		virtual void UpdateOncePerPursuit()   {}
		virtual void UpdateOncePerHeatLevel() {}


		enum class CopLabel
		{
			UNKNOWN,
			CHASER,
			HEAVY,
			LEADER,
			ROADBLOCK,
			HELICOPTER
		};

		virtual void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) = 0;

		virtual void ReactToRemovedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) = 0;
	};





	// IntervalTimer class --------------------------------------------------------------------------------------------------------------------------

	class IntervalTimer
	{
	private:

		float length         = 0.f;
		float startTimestamp = 0.f;
		float endTimestamp   = 0.f;

		const HeatParameters::OptionalInterval& interval;


		void UpdateStart()
		{
			this->startTimestamp = Globals::simulationTime;
		}


		void UpdateLength()
		{
			if (this->IsEnabled())
				this->length = this->interval.GetRandomValue();
		}


		void UpdateEnd()
		{
			this->endTimestamp = this->startTimestamp + this->length;
		}



	public:

		explicit IntervalTimer (const HeatParameters::OptionalInterval& interval) : interval(interval) {}


		bool IsEnabled() const
		{
			return (heatLevelKnown and this->interval.isEnableds.current);
		}


		enum class Setting
		{
			ALL,
			START,
			LENGTH
		};

		void Update(const Setting value)
		{
			switch (value)
			{
			case Setting::START:
				this->UpdateStart();
				break;

			case Setting::ALL:
				this->UpdateStart();
				[[fallthrough]];

			case Setting::LENGTH:
				this->UpdateLength();
			}

			this->UpdateEnd();
		}


		float GetLength() const
		{
			return this->length;
		}


		float TimeLeft() const
		{
			return (this->endTimestamp - Globals::simulationTime);
		}


		bool HasExpired() const
		{
			return (this->IsEnabled() and (this->TimeLeft() <= 0.f));
		}
	};
}