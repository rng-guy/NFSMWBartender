#pragma once

#include <vector>
#include <memory>
#include <format>
#include <string>
#include <iterator>
#include <string_view>



namespace StringTools
{
	// Pool class -----------------------------------------------------------------------------------------------------------------------------------

	class Pool
	{
	private:

		std::vector<std::unique_ptr<const std::string>> pool;


	public:

		[[nodiscard]] const std::string& Intern(const std::string_view string)
		{
			for (const auto& interned : this->pool)
			{
				if (*interned == string) return *interned;
			}

			this->pool.push_back(std::make_unique<const std::string>(string));

			return *(this->pool.back());
		}


		void MakeIntern(std::string_view& string)
		{
			string = this->Intern(string);
		}


		void MakeIntern(const char*& string)
		{
			if (not string) return; // UB with std::string_view constructor

			string = this->Intern(string).c_str();
		}


		void Reserve(const size_t capacity)
		{
			this->pool.reserve(capacity);
		}


		[[nodiscard]] size_t Size() const noexcept
		{
			return this->pool.size();
		}
	};





	// FormatBuffer class ---------------------------------------------------------------------------------------------------------------------------

	class FormatBuffer
	{
	private: // members

		std::string buffer;


	public: // methods

		[[nodiscard]] const std::string& GetString() const noexcept
		{
			return this->buffer;
		}


		// Invalidates retrieved string_view
		template <typename ...Ts>
		[[nodiscard]] std::string_view Format
		(
			const std::format_string<Ts...>    format,
			Ts&&                            ...formatArgs
		) {
			this->buffer.clear();

			const auto backIt = std::back_inserter(this->buffer);
			std::format_to(backIt, format, std::forward<Ts>(formatArgs)...);

			return this->buffer;
		}


		// May invalidate retrieved string_view
		void Reserve(const size_t capacity)
		{
			this->buffer.reserve(capacity);
		}


		// Invalidates retrieved string_view
		void Clear() noexcept
		{
			this->buffer.clear();
		}
	};
}