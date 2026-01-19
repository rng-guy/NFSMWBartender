#pragma once

#include <array>
#include <vector>
#include <string>
#include <format>

#include "Globals.h"
#include "HeatParameters.h"
#include "ConfigParser.h"



namespace RoadblockOverrides
{

    // Parameters -----------------------------------------------------------------------------------------------------------------------------------

    bool featureEnabled = false;

    constexpr size_t maxNumParts = 6;

    // Data structures
    enum RBPartType // C-style for compatibility
    {
        NONE     = 0,
        CAR      = 1,
        BLOCKADE = 2,
        SPIKES   = 3
    };

    struct RBPart
    {
        RBPartType partType = RBPartType::NONE;

        float offsetX = 0.f; // metres
        float offsetY = 0.f; // metres
        float angle   = 0.f; // revolutions
    };

    struct RBTable
    {
        float  minRoadWidth    = 0.f; // metres
        size_t numCarsRequired = 0;

        RBPart parts[maxNumParts] = {}; // C-style for compatibility
    };

    struct RBSetup
    {
        std::string name;

        bool    hasSpikes = false;
        RBTable table     = RBTable();

        HeatParameters::Pair<int> chances = HeatParameters::Pair<int>(100);
    };

    // Heat levels
    HeatParameters::Pair<float> widthTolerances(0.f);

    // Code caves
    std::vector<RBSetup> roadblockSetups;





    // Replacement functions ------------------------------------------------------------------------------------------------------------------------

    const RBTable* __cdecl SelectRoadblockTable
    (
        const float  roadWidth, 
        const size_t maxNumCars, 
        const bool   needsSpikes
    ) {
        static std::vector<const RBSetup*> candidateSetups;

        if constexpr (Globals::loggingEnabled)
        {
            Globals::logger.LogIndent("[RBL] Roadblock request", (needsSpikes) ? "(spikes)" : "(regular)");

            Globals::logger.LogLongIndent("Max. number of cars:", static_cast<int>(maxNumCars));
            Globals::logger.LogLongIndent("Road width:", roadWidth);
        }

        float bestWidth   = 0.f;
        int   totalChance = 0;

        for (const bool bestWidthKnown : {false, true})
        {
            if constexpr (Globals::loggingEnabled)
            {
                if (bestWidthKnown)
                    Globals::logger.LogLongIndent("Best width:", bestWidth);
            }

            for (const RBSetup& setup : roadblockSetups)
            {
                if (setup.chances.current < 1)                continue;
                if (setup.hasSpikes != needsSpikes)           continue;
                if (setup.table.minRoadWidth > roadWidth)     continue;
                if (setup.table.numCarsRequired > maxNumCars) continue;

                if (bestWidthKnown)
                {
                    if (bestWidth <= setup.table.minRoadWidth + widthTolerances.current)
                    {
                        totalChance += setup.chances.current;
                        candidateSetups.push_back(&setup);
                    }
                }
                else if (setup.table.minRoadWidth > bestWidth)
                    bestWidth = setup.table.minRoadWidth;
            }
        }

        if (not candidateSetups.empty())
        {
            int       cumulativeChance = 0;
            const int chanceThreshold  = Globals::prng.GenerateNumber<int>(1, totalChance);

            if constexpr (Globals::loggingEnabled)
                Globals::logger.LogLongIndent(static_cast<int>(candidateSetups.size()), "viable candidate(s)");

            for (const RBSetup* setup : candidateSetups)
            {
                cumulativeChance += setup->chances.current;

                if (cumulativeChance >= chanceThreshold)
                {
                    candidateSetups.clear();

                    if constexpr (Globals::loggingEnabled)
                        Globals::logger.LogLongIndent("Setup:", setup->name);

                    return &(setup->table);
                }
            }

            if constexpr (Globals::loggingEnabled)
                Globals::logger.Log("WARNING: [RBL] Failed to select roadblock setup");

            candidateSetups.clear();
        }
        else if constexpr (Globals::loggingEnabled)
            Globals::logger.LogLongIndent("No viable candidates");
   
        return nullptr;
    }





    // Code caves -----------------------------------------------------------------------------------------------------------------------------------

    constexpr address spawnFailureEntrance = 0x43E1DA;
    constexpr address spawnFailureExit     = 0x43E1E0;

    // Prevents failed roadblock requests from stalling cop spawns
    __declspec(naked) void SpawnFailure()
    {
        __asm
        {
            // Execute original code first
            test ecx, ecx
            mov dword ptr [esp + 0x18], ecx
            jne conclusion // found suitable setup

            mov edx, dword ptr [esp + 0x54] // AICopManager
            mov dword ptr [edx + 0xBC], ecx // roadblock pursuit
            mov dword ptr [edx + 0xB8], ecx // max. car count
           
            mov edx, dword ptr [esp + 0x4C4] // pursuit
            mov byte ptr [edx + 0x190], cl   // request pending
            
            conclusion:
            jmp dword ptr spawnFailureExit
        }
    }





    // State management -----------------------------------------------------------------------------------------------------------------------------

    void ApplyFixes()
    {
        // Fixes cop-spawn stalling due to failed roadblock spawn attempts
        MemoryTools::MakeRangeJMP(SpawnFailure, spawnFailureEntrance, spawnFailureExit);
    }



    bool Initialise(HeatParameters::Parser& parser)
    {
        if (not parser.LoadFile(HeatParameters::configPathAdvanced + "Roadblocks.ini")) return false;

        HeatParameters::Parse<float>(parser, "Roadblocks:Tolerance", {widthTolerances, 0.f});

        std::array<int,   maxNumParts> partTypes    = {};
        std::array<float, maxNumParts> partOffsetsX = {};
        std::array<float, maxNumParts> partOffsetsY = {};
        std::array<float, maxNumParts> partAngles   = {};

        const auto&       sections = parser.GetSections();
        const std::string baseName = "Setups:";

        roadblockSetups.reserve(50);

        for (const auto& [section, contents] : sections)
        {
            if (section.find(baseName) > 0) continue;

            float minRoadWidth = 0.f;

            if (not parser.ParseParameter<float>(section, "minRoadWidth", minRoadWidth, 0.f)) continue;

            RBSetup& setup = roadblockSetups.emplace_back();

            setup.table.minRoadWidth = minRoadWidth;

            const auto isValids = parser.ParseParameterTable<int, float, float, float>
            (
                section,
                "part{:02}",
                ConfigParser::FormatParameter<int,   maxNumParts>(partTypes),
                ConfigParser::FormatParameter<float, maxNumParts>(partOffsetsX),
                ConfigParser::FormatParameter<float, maxNumParts>(partOffsetsY),
                ConfigParser::FormatParameter<float, maxNumParts>(partAngles)
            );

            size_t numValidParts = 0;

            for (size_t partID = 0; partID < maxNumParts; ++partID)
            {
                if (not isValids[partID]) continue;

                switch (partTypes[partID])
                {
                case RBPartType::CAR:
                    ++setup.table.numCarsRequired;
                    break;

                case RBPartType::BLOCKADE:
                    break;

                case RBPartType::SPIKES:
                    setup.hasSpikes = true;
                    break;

                default:
                    continue;
                }

                setup.table.parts[numValidParts++] =
                {
                    static_cast<RBPartType>(partTypes[partID]),
                    partOffsetsX[partID],
                    partOffsetsY[partID],
                    partAngles  [partID]
                };
            }
            
            if (setup.table.numCarsRequired > 0)
            {
                setup.name = section.substr(baseName.length());
                HeatParameters::Parse<int>(parser, section, {setup.chances, 0});
            }
            else roadblockSetups.pop_back();
        }

        if (roadblockSetups.empty()) return false;

        // Code Changes
        MemoryTools::MakeRangeJMP(SelectRoadblockTable, 0x4063D0, 0x40644A);

        ApplyFixes();

        // Status flag
        featureEnabled = true;

        return true;
    }



    void SetToHeat
    (
        const bool   isRacing,
        const size_t heatLevel
    ) {
        if (not featureEnabled) return;

        widthTolerances.SetToHeat(isRacing, heatLevel);

        size_t numRegularRoadblocks = 0;
        size_t numSpikeRoadblocks   = 0;

        for (RBSetup& setup : roadblockSetups)
        {
            setup.chances.SetToHeat(isRacing, heatLevel);

            if (setup.chances.current > 0)
            {
                if (setup.hasSpikes)
                    ++numSpikeRoadblocks;

                else 
                    ++numRegularRoadblocks;
            }
        }

        // With logging disabled, the compiler optimises the unsigned integers away
        if constexpr (Globals::loggingEnabled)
        {
            Globals::logger.Log("    HEAT [RBL] RoadblockOverrides");

            widthTolerances.Log("widthTolerance          ");

            Globals::logger.LogLongIndent("numRegularRoadblocks:   ", static_cast<int>(numRegularRoadblocks));
            Globals::logger.LogLongIndent("numSpikeRoadblocks:     ", static_cast<int>(numSpikeRoadblocks));
        }
    }



    void LogSetupReport()
    {
        if (not featureEnabled) return;

        Globals::logger.Log("  CONFIG [RBL] RoadblockOverrides");

        size_t numRegularRoadblocks = 0;
        size_t numSpikeRoadblocks   = 0;

        for (const RBSetup& setup : roadblockSetups)
        {
            if (setup.hasSpikes)
                ++numSpikeRoadblocks;

            else
                ++numRegularRoadblocks;
        }

        Globals::logger.LogLongIndent("Regular roadblocks:", static_cast<int>(numRegularRoadblocks));
        Globals::logger.LogLongIndent("Spike   roadblocks:", static_cast<int>(numSpikeRoadblocks));
    }
}