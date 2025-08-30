#pragma once

#include <random>
#include <limits>
#include <array>

#undef min
#undef max



namespace RandomNumbers
{

	// xoshiro256++ PRNG (based on code from https://prng.di.unimi.it/) -----------------------------------------------------------------------------

	class Generator
	{
	private:

		std::array<uint64_t, 4> state = {};


		static uint64_t Join
		(
			const uint32_t upper,
			const uint32_t lower
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


		uint64_t Advance()
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

		explicit Generator()
		{
			std::random_device rng;

			for (size_t i = 0; i < this->state.size(); i++)
				this->state[i] = this->Join(rng(), rng());
		}


		static constexpr uint64_t min()
		{
			return std::numeric_limits<uint64_t>::min();
		}


		static constexpr uint64_t max()
		{
			return std::numeric_limits<uint64_t>::max();
		}


		uint64_t operator()()
		{
			return this->Advance();
		}


		template <typename T>
		T Generate
		(
			const T min,
			const T max  // exclusive
		){
			return min + (T)(this->Generate<double>(0., (double)(max - min)));
		}


		template <>
		double Generate<double>
		(
			const double min,
			const double max  // exclusive
		) {
			return min + (this->Advance() >> 11) * 0x1.0p-53 * (max - min);
		}


		template <>
		float Generate<float>
		(
			const float min,
			const float max  // exclusive
		) {
			return min + (this->Advance() >> 40) * 0x1.0p-24f * (max - min);
		}
	};
}