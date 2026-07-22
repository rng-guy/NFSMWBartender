#pragma once

#include <array>
#include <cstdarg>
#include <ostream>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <type_traits>



namespace BasicLogger
{

	// Format wrappers ------------------------------------------------------------------------------------------------------------------------------

	template<typename T>
	requires std::is_integral_v<T>
	struct BinFormat
	{
		T value;

		constexpr explicit BinFormat(const T value) noexcept : value(value) {}
	};


	template<typename T>
	requires std::is_integral_v<T>
	struct DecFormat
	{
		T value;

		constexpr explicit DecFormat(const T value) noexcept : value(value) {}
	};


	template<typename T>
	requires std::is_integral_v<T>
	struct HexFormat
	{
		T value;

		constexpr explicit HexFormat(const T value) noexcept : value(value) {}
	};





	// Logger class ---------------------------------------------------------------------------------------------------------------------------------

	template <size_t ...indents>
	class Logger
	{
	private:

		std::fstream file;

		static constexpr std::array<size_t, 1 + sizeof...(indents)> indentWidths = {0, indents...};


		template <typename T>
		void Print(const BinFormat<T> wrapper)
		{
			this->file << std::format("{:#0{}b}", wrapper.value, 8 * sizeof(T));
		}


		template <typename T>
		void Print(const DecFormat<T> wrapper) noexcept
		{
			this->file << wrapper.value;
		}


		template <typename T>
		void Print(const HexFormat<T> wrapper)
		{
			this->file << std::format("{:0{}x}", wrapper.value, 2 * sizeof(T));
		}


		template <typename T>
		requires (not std::is_trivially_copyable_v<T>)
		void Print(const T& value)
		{
			this->file << value;
		}


		template <typename T>
		requires std::is_trivially_copyable_v<T>
		void Print(const T value)
		{
			if constexpr (std::is_enum_v<T>)
				this->Print(static_cast<std::underlying_type_t<T>>(value));

			else if constexpr (std::is_pointer_v<T>)
				this->Print(reinterpret_cast<uintptr_t>(value));

			else if constexpr (std::is_unsigned_v<T>)
				this->Print(HexFormat(value));

			else if constexpr (std::is_floating_point_v<T>)
				this->file << std::format("{:.3f}", value);

			else
				this->file << value;
		}


		void Print(const char* const value) noexcept
		{
			this->file << ((value) ? value : "nullptr");
		}


		void Print(const bool value) noexcept
		{
			this->file << ((value) ? "true" : "false");
		}


		void PrintLine() noexcept
		{
			this->file << std::endl; // to flush
		}


		template <typename T, typename ...Ts>
		void PrintLine
		(
			T&&     first,
			Ts&& ...rest
		) {
			this->Print(std::forward<T>(first));
			(..., (this->file << ' ', this->Print(std::forward<Ts>(rest))));

			this->PrintLine();
		}


	public:

		bool Open(const std::filesystem::path& path) noexcept
		{
			if (not this->file.is_open())
				this->file.open(path, std::ios::app);

			return this->file.is_open();
		}


		bool Close() noexcept
		{
			if (this->file.is_open())
				this->file.close();

			return (not this->file.is_open());
		}


		template <size_t indentLevel = 0, typename ...Ts>
		void Log(Ts&& ...segments)
		{
			static_assert(indentLevel < this->indentWidths.size(), "Invalid indentLevel");

			if (this->file.is_open())
			{
				constexpr size_t numWhitespaces = this->indentWidths[indentLevel];

				if constexpr (numWhitespaces > 0)
					std::fill_n(std::ostreambuf_iterator<char>(this->file), numWhitespaces, ' ');

				this->PrintLine(std::forward<Ts>(segments)...);
			}
		}
	};
}