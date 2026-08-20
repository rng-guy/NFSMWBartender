#pragma once

#include <format>
#include <string>
#include <iterator>
#include <string_view>



namespace FormatBuffer
{
	// Buffer class ---------------------------------------------------------------------------------------------------------------------------------

	class Buffer
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