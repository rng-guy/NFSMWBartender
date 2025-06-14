#pragma once

#include <random>
#include <array>



namespace RandomNumbers
{

	// Generator class ------------------------------------------------------------------------------------------------------------------------------

	class Generator
	{
	private:

		std::array<uint64_t, 4> state;


		static uint64_t Join
		(
			uint32_t upper,
			uint32_t lower
		) {
			return ((uint64_t)upper << 32) bitor lower;
		}


		static uint64_t Rotate
		(
			const uint64_t x,
			const int      k
		) {
			return (x << k) bitor (x >> (64 - k));
		}


		uint64_t Generate()
		{
			const uint64_t result = this->Rotate(this->state[0] + this->state[3], 23) + this->state[0];
			const uint64_t      t = this->state[1] << 17;

			this->state[2] ^= this->state[0];
			this->state[3] ^= this->state[1];
			this->state[1] ^= this->state[2];
			this->state[0] ^= this->state[3];
			this->state[2] ^= t;

			this->state[3] = this->Rotate(this->state[3], 45);

			return result;
		}



	public:

		Generator()
		{
			std::random_device rng;

			this->state =
			{
				this->Join(rng(), rng()),
				this->Join(rng(), rng()),
				this->Join(rng(), rng()),
				this->Join(rng(), rng())
			};
		}


		double GetDouble
		(
			const double max,
			const double min = 0.
		) {
			return min + (this->Generate() >> 11) * 0x1.0p-53 * (max - min);
		}


		float GetFloat
		(
			const float max,
			const float min = 0.f
		) {
			return min + (this->Generate() >> 40) * 0x1.0p-24f * (max - min);
		}


		int GetInt
		(
			const int max,
			const int min = 0
		) {
			return min + (int)(this->GetDouble(max - min));
		}
	};
}