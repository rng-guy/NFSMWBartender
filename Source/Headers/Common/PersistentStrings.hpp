#pragma once

#include <string>
#include <string_view>

#include "Globals.hpp"
#include "ModContainers.hpp"



namespace PersistentStrings
{
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	RELEASE_CONSTINIT ModContainers::StableVaultMap<const std::string> vaultHashToString;





	// Management functions -------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] const std::string* Get(const vault hash)
	{
		const auto foundHash = vaultHashToString.find(hash);
		if (foundHash == vaultHashToString.end()) return nullptr;

		return foundHash->second.get();
	}



	[[nodiscard]] const std::string& Create
	(
		const vault            hash,
		const std::string_view string
	) {
		const auto [pairIt, _] = vaultHashToString.try_emplace(hash, string);
		return *(pairIt->second); // guaranteed to stay valid for the session
	}


	[[nodiscard]] const std::string& Create(const std::string_view string)
	{
		return Create(Globals::GetVaultHash(string), string);
	}



	void Make
	(
		const vault       hash,
		std::string_view& string
	) {
		string = Create(hash, string);
	}


	void Make(std::string_view& string)
	{
		Make(Globals::GetVaultHash(string), string);
	}



	void Make
	(
		const vault  hash,
		const char*& string
	) {
		if (not string) return; // UB with std::string_view constructor

		string = Create(hash, string).c_str();
	}


	void Make(const char*& string)
	{
		if (not string) return; // UB with std::string_view constructor

		Make(Globals::GetVaultHash(string), string);
	}
}