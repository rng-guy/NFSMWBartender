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

		// Invalidates retrieved string_view
		void Clear() noexcept
		{
			this->buffer.clear();
		}


		// May invalidate retrieved string_view
		void Reserve(const size_t size)
		{
			this->buffer.reserve(size);
		}


		// Invalidates retrieved string_view
		template <typename ...T>
		std::string_view Format
		(
			const std::format_string<T...>    format,
			T&&                            ...formatArgs
		) {
			this->buffer.clear();

			std::format_to(std::back_inserter(this->buffer), format, std::forward<T>(formatArgs)...);

			return this->buffer;
		}
	};
}