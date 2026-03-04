#pragma once

#include <array>
#include <random>
#include <cstdint>
#include <type_traits>

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
		) 
			noexcept
		{
			return (static_cast<uint64_t>(upper) << 32) bitor lower;
		}


		static constexpr uint64_t Rotate
		(
			const uint64_t x, 
			const int      k
		)
			noexcept
		{
			return (x << k) bitor (x >> (64 - k));
		}


		static constexpr uint64_t ApplySplitmix64(uint64_t& state) noexcept
		{
			uint64_t z = (state += 0x9e3779b97f4a7c15);

			z = (z xor (z >> 30)) * 0xbf58476d1ce4e5b9;
			z = (z xor (z >> 27)) * 0x94d049bb133111eb;

			return z xor (z >> 31);
		}



	public:

		explicit Xoshiro256ss(uint64_t seed = 0x0)
		{
			if (not seed)
			{
				std::random_device rng;

				seed = this->Join(rng(), rng());
			}

			for (uint64_t& value : this->state)
				value = this->ApplySplitmix64(seed);
		}


		uint64_t operator()() noexcept
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


		static constexpr uint64_t min() noexcept
		{
			return 0;
		}


		static constexpr uint64_t max() noexcept
		{
			return uint64_t(-1);
		}
	};





	// Generator wrapper class ----------------------------------------------------------------------------------------------------------------------

	template <class Engine = Xoshiro256ss>
	class Generator
	{
	private:

		Engine engine;
		


	public:

		template <typename T>
		requires std::is_integral_v<T>
		T GenerateNumber
		(
			const T min,
			const T max
		) 
			noexcept
		{
			// ...can generate values from [min, max]
			return std::uniform_int_distribution<T>{min, max}(this->engine);
		}


		template <typename T>
		requires std::is_integral_v<T>
		bool DoPercentTrial(const T chance) noexcept
		{
			return (this->GenerateNumber<T>(static_cast<T>(1), static_cast<T>(100)) <= chance);
		}


		template <typename T>
		requires std::is_floating_point_v<T>
		T GenerateNumber
		(
			const T min,
			const T max
		) 
			noexcept
		{
			// ...can generate values from [min, max)
			return std::uniform_real_distribution<T>{min, max}(this->engine);
		}


		template <typename T>
		requires std::is_floating_point_v<T>
		bool DoPercentTrial(const T chance) noexcept
		{
			return (this->GenerateNumber<T>(static_cast<T>(0), static_cast<T>(100)) < chance);
		}


		size_t GenerateIndex(const size_t size) noexcept
		{
			return (size > 1) ? this->GenerateNumber<size_t>(0, size - 1) : 0;
		}
	};
}