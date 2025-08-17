#pragma once

#include <unordered_set>
#include <unordered_map>

#include "BasicLogger.h"
#include "RandomNumbers.h"

#undef min
#undef max



// Unscoped aliases
using byte    = unsigned char;
using address = uint32_t;
using binary  = uint32_t;
using vault   = uint32_t;



namespace Globals
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Pseudorandom number generator
	RandomNumbers::Generator prng;

	// Common function pointers
	vault (__cdecl*    const GetVaultKey)   (const char*) = (vault  (__cdecl*)   (const char*))0x5CC240;
	vault (__thiscall* const GetVehicleType)(address)     = (vault  (__thiscall*)(address))    0x6880A0;

	// Logging
	constexpr bool loggingEnabled = false;

	BasicLogger::Logger logger;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	address GetFromVault
	(
		const vault rootKey,
		const vault nodeKey,
		const vault attributeKey = 0x0
	) {
		static address (__cdecl*    const GetVaultNode)     (vault, vault)        = (address (__cdecl*)   (vault, vault))       0x455FD0;
		static address (__thiscall* const GetVaultAttribute)(address, vault, int) = (address (__thiscall*)(address, vault, int))0x454190;

		const address node = GetVaultNode(rootKey, nodeKey);
		return (node and attributeKey) ? GetVaultAttribute(node, attributeKey, 0) : node;
	}





	// Custom hash function and (scoped) aliases ----------------------------------------------------------------------------------------------------

	template <typename T>
	struct IdentityHash 
	{
		size_t operator()(const T value) const
		{
			return value;
		}
	};

	

	using AddressSet = std::unordered_set<address, IdentityHash<address>>;
	using VaultSet   = std::unordered_set<vault,   IdentityHash<vault>>;

	template <typename T>
	using AddressMap = std::unordered_map<address, T, IdentityHash<address>>;

	template <typename T>
	using VaultMap = std::unordered_map<vault, T, IdentityHash<vault>>;
}