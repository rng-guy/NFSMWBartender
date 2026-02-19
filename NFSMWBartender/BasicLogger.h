#pragma once

#include <array>
#include <format>
#include <string>
#include <fstream>
#include <concepts>
#include <string_view>
#include <type_traits>



namespace BasicLogger
{

	// Logger class ---------------------------------------------------------------------------------------------------------------------------------

	template <size_t numIndents>
	class Logger
	{
	private:

		std::fstream file;

		std::array<std::string, numIndents> indents;


		template <typename T>
		void Print(const T value) 
		{
			this->file << value;
		}


		template <typename T>
		requires std::is_enum_v<T>
		void Print(const T value)
		{
			using base = std::underlying_type_t<T>;
			this->Print<base>(static_cast<base>(value));
		}


		template <typename T>
		requires std::is_pointer_v<T>
		void Print(const T value)
		{
			this->Print<uintptr_t>(reinterpret_cast<uintptr_t>(value));
		}


		template <>
		void Print<const char*>(const char* const value)
		{
			this->file << ((value) ? value : "nullptr");
		}


		template <>
		void Print<uint32_t>(const uint32_t value)
		{
			this->file << std::format("{:08x}", value);
		}


		template <typename T>
		requires std::is_floating_point_v<T>
		void Print(const T value) 
		{
			this->file << std::format("{:.3f}", value);
		}


		template <>
		void Print<bool>(const bool value)
		{
			this->Print<const char*>((value) ? "true" : "false");
		}


		void PrintLine()
		{
			if (this->file.is_open())
				this->file << std::endl; // for debugging
		}


		template <typename T, typename ...Ts>
		void PrintLine
		(
			const T     first,
			const Ts ...rest
		) {
			this->Print<T>(first);

			(..., (this->file << ' ', this->Print<Ts>(rest)));

			this->file << std::endl; // for debugging
		}



	public:

		template <typename ...Ts>
		requires ((sizeof...(Ts) == numIndents) and ... and std::convertible_to<Ts, size_t>)
		explicit Logger(const Ts ...numSpaces) : indents(std::string(numSpaces, ' ')...) {}


		bool Open(const std::string_view path)
		{
			if (this->file.is_open()) return false;

			this->file.open(path, std::ios::app);

			return this->file.is_open();
		}


		bool Close()
		{
			if (not this->file.is_open()) return false;

			this->file.close();

			return (not this->file.is_open());
		}


		template <size_t indentLevel = 0, typename ...Ts>
		void Log(const Ts ...parts)
		{
			if (this->file.is_open())
			{
				if constexpr (indentLevel > 0)
				{
					static_assert(indentLevel - 1 < numIndents, "Invalid indentLevel");
					this->Print((this->indents[indentLevel - 1]).c_str());
				}

				this->PrintLine<Ts...>(parts...);
			}
		}
	};



	// Deduction guide
	template <typename ...Ts>
	Logger(Ts...) -> Logger<sizeof...(Ts)>;
}