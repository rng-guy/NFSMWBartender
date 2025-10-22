#pragma once

#include <array>
#include <random>
#include <concepts>

#undef min
#undef max



namespace RandomNumbers
{

	// xoshiro256** PRNG (based on code from https://prng.di.unimi.it/) -----------------------------------------------------------------------------

	class Xoshiro256ss
	{
	private:

		std::array<uint64_t, 4> state = {};


		static constexpr uint64_t Join
		(
			const uint32_t upper,
			const uint32_t lower
		) {
			return ((uint64_t)upper << 32) bitor lower;
		}


		static constexpr uint64_t Rotate
		(
			const uint64_t x, 
			const int      k
		) {
			return (x << k) bitor (x >> (64 - k));
		}


		static constexpr uint64_t ApplySplitmix64(uint64_t& state)
		{
			uint64_t z = (state += 0x9e3779b97f4a7c15);

			z = (z xor (z >> 30)) * 0xbf58476d1ce4e5b9;
			z = (z xor (z >> 27)) * 0x94d049bb133111eb;

			return z xor (z >> 31);
		}



	public:

		explicit constexpr Xoshiro256ss(uint64_t seed = 0x0)
		{
			if (not seed)
			{
				std::random_device rng;
				seed = this->Join(rng(), rng());
			}

			for (uint64_t& value : this->state)
				value = this->ApplySplitmix64(seed);
		}


		constexpr uint64_t operator()()
		{
			const uint64_t result = this->Rotate(this->state[1] * 5, 7) * 9;
			const uint64_t t      = this->state[1] << 17;

			this->state[2] ^= this->state[0];
			this->state[3] ^= this->state[1];
			this->state[1] ^= this->state[2];
			this->state[0] ^= this->state[3];
			this->state[2] ^= t;

			this->state[3] = this->Rotate(this->state[3], 45);

			return result;
		}


		static constexpr uint64_t min()
		{
			return 0;
		}


		static constexpr uint64_t max()
		{
			return uint64_t(-1);
		}
	};





	// Generator wrapper class ----------------------------------------------------------------------------------------------------------------------

	class Generator
	{
	private:

		Xoshiro256ss engine;
		


	public:

		template <std::integral T>
		T GenerateNumber
		(
			const T min,
			const T max
		) {
			std::uniform_int_distribution<T> distribution(min, max);

			return distribution(this->engine);
		}


		template <std::floating_point T>
		T GenerateNumber
		(
			const T min,
			const T max
		) {
			std::uniform_real_distribution<T> distribution(min, max);

			return distribution(this->engine);
		}


		size_t GenerateIndex(const size_t size)
		{
			return (size > 1) ? this->GenerateNumber<size_t>(1, size) - 1 : 0;
		}
	};
}