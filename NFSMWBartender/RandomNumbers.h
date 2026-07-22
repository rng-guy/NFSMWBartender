#pragma once

#include <array>
#include <limits>
#include <random>
#include <cstdint>
#include <algorithm>
#include <type_traits>



namespace RandomNumbers
{

	// xoshiro256** PRNG (based on code from https://prng.di.unimi.it/) -----------------------------------------------------------------------------

	class Xoshiro256ss
	{
	private:

		uint64_t                seed  = 0x0;
		std::array<uint64_t, 4> state = {};


		[[nodiscard]] static constexpr uint64_t Rotate
		(
			const uint64_t x, 
			const int      k
		)
			noexcept
		{
			return (x << k) | (x >> (64 - k));
		}


		[[nodiscard]] static constexpr uint64_t ApplySplitmix64(uint64_t& state) noexcept
		{
			uint64_t z = (state += 0x9e3779b97f4a7c15);

			z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
			z = (z ^ (z >> 27)) * 0x94d049bb133111eb;

			return z ^ (z >> 31);
		}


	public:

		constexpr void SetSeed(uint64_t seed)
		{
			this->seed = seed;

			for (uint64_t& value : this->state)
				value = this->ApplySplitmix64(seed);
		}


		constexpr explicit Xoshiro256ss(const uint64_t seed)
		{
			this->SetSeed(seed);
		}


		[[nodiscard]] constexpr uint64_t GetSeed() const noexcept
		{
			return this->seed;
		}


		[[nodiscard]] constexpr uint64_t operator()() noexcept
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


		// For STL compatibility
		[[nodiscard]] static constexpr uint64_t min() noexcept
		{
			return std::numeric_limits<uint64_t>::min();
		}


		// For STL compatibility
		[[nodiscard]] static constexpr uint64_t max() noexcept
		{
			return std::numeric_limits<uint64_t>::max();
		}


		// For STL compatibility
		using result_type = uint64_t;
	};





	// Generator wrapper class ----------------------------------------------------------------------------------------------------------------------

	template <class Engine = Xoshiro256ss>
	requires std::uniform_random_bit_generator<Engine>
	class Generator
	{
	private:

		Engine engine;
		

		[[nodiscard]] static uint64_t GenerateSeed()
		{
			std::random_device rng;

			const uint32_t lower = rng();
			const uint32_t upper = rng();

			return (static_cast<uint64_t>(upper) << 32) | lower;
		}


	public:

		Generator() : engine(this->GenerateSeed()) {}

		explicit Generator(const uint64_t seed) : engine(seed) {}


		// Samples from [min, max]
		template <typename T>
		requires std::is_integral_v<T>
		[[nodiscard]] T GenerateNumber
		(
			const T min,
			const T max
		) {
			return std::uniform_int_distribution<T>{min, max}(this->engine);
		}


		template <typename T>
		requires std::is_integral_v<T>
		[[nodiscard]] bool DoPercentTrial(const T chance)
		{
			return (this->GenerateNumber<T>(static_cast<T>(1), static_cast<T>(100)) <= chance);
		}


		// Samples from [min, max)
		template <typename T>
		requires std::is_floating_point_v<T>
		[[nodiscard]] T GenerateNumber
		(
			const T min,
			const T max
		) {
			return std::uniform_real_distribution<T>{min, max}(this->engine);
		}


		template <typename T>
		requires std::is_floating_point_v<T>
		[[nodiscard]] bool DoPercentTrial(const T chance)
		{
			return (this->GenerateNumber<T>(static_cast<T>(0), static_cast<T>(100)) < chance);
		}


		// Samples from [0, size)
		[[nodiscard]] size_t GenerateIndex(const size_t size)
		{
			return this->GenerateNumber<size_t>(0, size - 1);
		}
	};
}