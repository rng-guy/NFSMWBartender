#pragma once

#include <array>
#include <string>
#include <utility>
#include <cstdarg>
#include <ostream>
#include <fstream>
#include <iterator>
#include <concepts>
#include <filesystem>
#include <string_view>
#include <type_traits>

#include "FormatBuffer.hpp"



namespace BasicLogger
{
	// Format wrappers ------------------------------------------------------------------------------------------------------------------------------

	template<typename T>
	requires std::integral<T>
	struct BinFormat
	{
	// Members

		T value;
	};


	template<typename T>
	requires std::integral<T>
	struct DecFormat
	{
	// Members

		T value;
	};


	template<typename T>
	requires std::integral<T>
	struct HexFormat
	{
	// Members

		T value;
	};





	// LogLiteral class -----------------------------------------------------------------------------------------------------------------------------

	template <bool isEnabled>
	class LogLiteral {};


	template <>
	class LogLiteral</* isEnabled = */ true>
	{
	private: // members

		std::string_view string;


	public: // methods

		consteval LogLiteral() noexcept = default;

		template <size_t size>
		consteval LogLiteral(const char (&string)[size]) noexcept : string(string, size - 1) {}


		[[nodiscard]] constexpr std::string_view GetView() const noexcept
		{
			return this->string;
		}


		[[nodiscard]] constexpr operator std::string_view() const noexcept
		{
			return this->GetView();
		}
	};


	template <>
	class LogLiteral</* isEnabled = */ false>
	{
	public: // methods

		consteval LogLiteral() noexcept = default;

		template <size_t size>
		consteval LogLiteral(const char (&)[size]) noexcept {}


		[[nodiscard]] constexpr std::string_view GetView() const noexcept
		{
			return {};
		}


		[[nodiscard]] constexpr operator std::string_view() const noexcept
		{
			return this->GetView();
		}
	};





	// LogString class ------------------------------------------------------------------------------------------------------------------------------

	template <bool isEnabled>
	class LogString {};


	template <>
	class LogString</* isEnabled = */ true>
	{
	private: // members

		std::string string;


	public: // methods

		constexpr LogString() noexcept = default;


		LogString(const char* const      string) : string(string) {}
		LogString(const std::string_view string) : string(string) {}
		LogString(const std::string&     string) : string(string) {}

		LogString(std::string&& string) noexcept : string(std::move(string)) {}


		[[nodiscard]] std::string_view GetView() const noexcept
		{
			return this->string;
		}


		[[nodiscard]] operator std::string_view() const noexcept
		{
			return this->GetView();
		}
	};


	template <>
	class LogString</* isEnabled = */ false>
	{
	public: // methods

		constexpr LogString() noexcept = default;

		LogString(const char* const)      {}
		LogString(const std::string_view) {}
		LogString(const std::string&)     {}

		LogString(std::string&&) noexcept {}


		[[nodiscard]] std::string_view GetView() const noexcept
		{
			return {};
		}


		[[nodiscard]] operator std::string_view() const noexcept
		{
			return this->GetView();
		}
	};





	// Logger class ---------------------------------------------------------------------------------------------------------------------------------

	template <size_t ...indents>
	class Logger
	{
	private: // members

		std::fstream file;

		mutable FormatBuffer::Buffer buffer;

		static constexpr std::array<size_t, 1 + sizeof...(indents)> indentWidths = {0, indents...};


	private: // methods

		template <typename T>
		void Print(const BinFormat<T> wrapper)
		{
			this->file << this->buffer.Format("{:#0{}b}", wrapper.value, 8 * sizeof(T));
		}


		template <typename T>
		void Print(const DecFormat<T> wrapper)
		{
			this->file << this->buffer.Format("{:d}", wrapper.value);
		}


		template <typename T>
		void Print(const HexFormat<T> wrapper)
		{
			this->file << this->buffer.Format("{:0{}x}", wrapper.value, 2 * sizeof(T));
		}


		template <bool isEnabled>
		void Print(const LogLiteral<isEnabled>& string)
		{
			this->Print<std::string_view>(string);
		}


		template <bool isEnabled>
		void Print(const LogString<isEnabled>& string)
		{
			this->Print<std::string_view>(string);
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

			else if constexpr (std::unsigned_integral<T>)
				this->Print(HexFormat(value));

			else if constexpr (std::floating_point<T>)
				this->file << this->buffer.Format("{:.3f}", value);

			else this->file << value;
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


	public: // methods

		Logger() = default;


		[[nodiscard]] bool IsOpen() const noexcept
		{
			return this->file.is_open();
		}


		bool Open(const std::filesystem::path& path) noexcept
		{
			if (not this->IsOpen())
				this->file.open(path, std::ios::app);

			return this->IsOpen();
		}


		explicit Logger(const std::filesystem::path& path)
		{
			this->Open(path);
		}


		bool Close() noexcept
		{
			if (this->IsOpen())
				this->file.close();

			return (not this->IsOpen());
		}


		template <size_t indentLevel = 0, typename ...Ts>
		void Log(Ts&& ...segments)
		{
			static_assert(indentLevel < this->indentWidths.size(), "Invalid indentLevel");

			if (not this->IsOpen()) return;

			constexpr size_t numWhitespaces = this->indentWidths[indentLevel];

			if constexpr (numWhitespaces > 0)
				std::fill_n(std::ostreambuf_iterator<char>(this->file), numWhitespaces, ' ');

			this->PrintLine(std::forward<Ts>(segments)...);
		}
	};
}