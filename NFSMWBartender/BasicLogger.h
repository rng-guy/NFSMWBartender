#pragma once

#include <fstream>
#include <format>
#include <string>



namespace BasicLogger
{

	// Logger class ---------------------------------------------------------------------------------------------------------------------------------

	class Logger
	{
	private:

		std::fstream file;


		template <typename T>
		void Print(const T value) 
		{
			this->file << value;
		}

		
		template<>
		void Print<uint32_t>(const uint32_t value) 
		{
			this->file << std::format("{:08x}", value);
		}


		template<>
		void Print<float>(const float value) 
		{
			this->file << std::format("{:.3f}", value);
		}



	public:

		std::string indent     = "        ";
		std::string longIndent = "              ";


		bool Open(const std::string& fileName)
		{
			if (this->file.is_open()) return false;

			this->file.open(fileName, std::ios::app);

			return (this->file.is_open());
		}


		template <typename ...T>
		void Log(const T ...segments)
		{
			if (not (this->file.is_open())) return;

			constexpr size_t numArgs = sizeof...(T);

			size_t argID = 0;

			(
				[&]
				{
					this->Print<T>(segments);

					if (++argID < numArgs) 
						this->file << ' ';
				}
			(), ...);

			this->file << std::endl;
		}


		template <typename ...T>
		void LogIndent(const T ...segments)
		{
			this->Log<std::string, T...>(this->indent, segments...);
		}


		template <typename ...T>
		void LogLongIndent(const T ...segments)
		{
			this->Log<std::string, T...>(this->longIndent, segments...);
		}
	};
}