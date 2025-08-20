
── ■ │ WHAT IS NFSMW BARTENDER? (v1.14.02) │ ■ ────────────────────────────────────────────────────

You can view this document with web formatting on GitHub: https://github.com/rng-guy/NFSMWBartender

Bartender adds NEW CUSTOMISATION OPTIONS to pursuits. These new options come in two feature sets:
 • the "BASIC" FEATURE SET lets you change many otherwise hard-coded values of the game, and
 • the "ADVANCED" FEATURE SET lets you change cop-spawning behaviour and tables without limits.

The sections below ADDRESS THESE QUESTIONS in detail:
 1) • What does the "Basic" feature set do?
 2) • What does the "Advanced" feature set do?
 3) • What mods are (in)compatible with Bartender?
 4) • How do I install Bartender for my game?
 5) • How may I share or bundle Bartender?






── ■ │ 1 - WHAT DOES THE "BASIC" FEATURE SET DO? │ ■ ──────────────────────────────────────────────

The "Basic" feature set LETS YOU CHANGE (per Heat level)
 • how quickly and at what distances from cops the red "BUSTED" bar fills,
 • how quickly the green "EVADE" bar fills once all cops have lost sight of you,
 • at what time interval you gain passive bounty,
 • the maximum combo-bounty multiplier for destroying cops quickly,
 • the internal cooldown for non-Strategy roadblock requests,
 • the internal cooldown for Heavy / LeaderStrategy requests,
 • what vehicles spawn through HeavyStrategy 3 (the ramming SUVs),
 • what vehicles spawn through HeavyStrategy 4 (the roadblock SUVs), and
 • what vehicles spawn through LeaderStrategy 5 / 7 (Cross and his henchmen).

The "Basic" feature set FIXES FIVE BUGS:
 • helicopters no longer cast static shadows (like cars do) with incorrect placements,
 • Heat levels > 5 are no longer reset back to 5 when you enter free-roam or start an event,
 • Heat levels > 5 are now shown correctly in menus (requires Binary for missing textures),
 • you can no longer get busted due to line-of-sight issues while the "EVADE" bar fills, and
 • non-Strategy roadblock and Heavy / LeaderStrategy requests no longer slow down over time.

You can also ASSIGN NEW (BINARY) STRINGS for the game to display when cop vehicles are destroyed,
similar to the "NFSMW Unlimiter" mod by nlgxzef. Bartender's version of this feature is easier
to configure and will never cause "FENG: Default string error" pop-ups, as it checks every string
label you provide against the game's (potentially modified) language files whenever you launch it.






── ■ │ 2 - WHAT DOES THE "ADVANCED" FEATURE SET DO? │ ■ ───────────────────────────────────────────

The "Advanced" feature set LETS YOU CHANGE (per Heat level)
 • how many cops can (re)spawn without backup once a wave is exhausted,
 • the global cop-spawn limit for how many cops in total may chase you at any given time,
 • how quickly (if at all) cops flee a pursuit if they don't belong to the Heat level,
 • what vehicles (any amount, with counts and chances) may spawn to chase and search for you,
 • what vehicles (same liberties as above) may spawn in non-Strategy roadblocks,
 • what vehicles (ditto) may spawn as pre-generated cops in scripted events,
 • what vehicles (without counts) may spawn as free patrols when there is no active pursuit,
 • what vehicle spawns in place of the regular helicopter, and
 • when exactly (if at all) the helicopter can (de / re)spawn.

The "Advanced" feature set FIXES TWO BUGS:
 • HeavyStrategy 3 spawns no longer end their ramming attempts prematurely to flee instead, and
 • the engagement count shown above the pursuit board now (accurately) counts chasing cops only.






── ■ │ 3 - WHAT MODS ARE (IN)COMPATIBLE WITH BARTENDER? │ ■ ───────────────────────────────────────

Bartender should be FULLY COMPATIBLE with all VltEd and Binary mods. Other .asi mods
without pursuit features should also be compatible unless they are listed below.

Some popular .asi mods REQUIRE (RE)CONFIGURATION to be compatible with Bartender:
 • In "NFSMW ExtraOptions" by ExOptsTeam, disable the "HeatLevelOverride" feature.
 • In "NFSMW Unlimiter" by nlgxzef, disable the "EnableCopDestroyedStringHook" feature.
 • In "NFSMW LimitAdjuster" by Zolika1351, disable every cop-related feature under "[Options]".






── ■ │ 4 - HOW DO I INSTALL BARTENDER FOR MY GAME? │ ■ ────────────────────────────────────────────

BEFORE INSTALLING Bartender:
 1) • make sure you have read and understood the section about mod (in)compatibility above,
 2) • make sure your game's "speed.exe" is compatible (i.e. 5.75 MB / 6,029,312 bytes large), and
 3) • install an .asi loader or any mod with one (e.g. the "WideScreenFix" mod by ThirteenAG).

To INSTALL Bartender:
 1) • if it doesn't exist already, create a "scripts" folder in your game's installation folder;
 2) • copy the contents of Bartender's "scripts" folder into your game's "scripts" folder;
 3) • if Bartender's .asi file gets flagged by your antivirus software, whitelist the file; and
 4) • (optional) in User Mode of Binary 2.8.3 or newer, load and apply "FixMissingTextures.end".

AFTER INSTALLING Bartender:
 1) • check out its configuration (.ini) files in the "scripts\BartenderSettings" folder, and
 2) • see the "APPENDIX.txt" file should you encounter any issues or want more feature details.

To UNINSTALL Bartender, remove its files from your game's "scripts" folder. There is no need
to remove the optional missing textures, as the game won't ever use them without Bartender.

To UPDATE Bartender, uninstall it and repeat the installation process above.
Whenever you update, make sure to replace ALL old configuration files!






── ■ │ 5 - HOW MAY I SHARE OR BUNDLE BARTENDER? │ ■ ───────────────────────────────────────────────

You are free to bundle Bartender and its files with your own pursuit mod, NO CREDIT REQUIRED.
In the interest of code transparency, however, consider linking to Bartender's GitHub repository 
(https://github.com/rng-guy/NFSMWBartender) somewhere in your mod's documentation (e.g. README).

Finally, Bartender would not have seen the light of day without
 • DarkByte, for Cheat Engine;
 • rx, for encouraging me to try .asi modding;
 • nlgxzef, for the Most Wanted debug symbols;
 • GuidedHacking, for their Cheat Engine tutorials;
 • Sebastiano Vigna, for his pseudorandom number generators;
 • Matthias C. M. Troffaes, for his configuration-file parser;
 • ExOptsTeam, for permitting me to use their Heat-level fixes;
 • trelbutate, for his "NFSMW Cop Car Healthbars" mod as a resource; and
 • Orsal, Aven, Astra King79, and MORELLO, for testing and providing feedback.