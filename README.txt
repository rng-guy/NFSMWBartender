
── ■ │ WHAT IS NFSMW BARTENDER? (v1.13.00) │ ■ ────────────────────────────────────────────────────

You can view this document with web formatting on GitHub: https://github.com/rng-guy/NFSMWBartender

Bartender adds NEW CUSTOMISATION OPTIONS to pursuits. These options come in two feature sets:
 • the "BASIC" FEATURE SET lets you change many otherwise hard-coded values of the game, and
 • the "ADVANCED" FEATURE SET lets you change cop-spawning behaviour and tables without limits.

The sections below ADDRESS THE FOLLOWING QUESTIONS in detail:
 1) • What does the "Basic" feature set do?
 2) • What does the "Advanced" feature set do?
 3) • What mods are (in)compatible with Bartender?
 4) • How do I install Bartender for my game?
 5) • How may I share or bundle Bartender?
 6) • What changed in each version?






── ■ │ 1 - WHAT DOES THE "BASIC" FEATURE SET DO? │ ■ ──────────────────────────────────────────────

The "Basic" feature set LETS YOU CHANGE (per Heat level)
 • how quickly and at what distances from cops the red "BUSTED" bar fills,
 • how quickly the green "EVADE" bar fills once all cops have lost sight of you,
 • at what time interval you gain passive bounty,
 • the maximum combo-bounty multiplier for destroying cops quickly,
 • the internal cooldown for regular roadblock spawns,
 • the internal cooldown for Heavy / LeaderStrategy spawns,
 • what vehicles spawn through HeavyStrategy 3 (the ramming SUVs),
 • what vehicles spawn through HeavyStrategy 4 (the roadblock SUVs), and
 • what vehicles spawn through LeaderStrategy 5 / 7 (Cross and his henchmen).

The "Basic" feature set FIXES FIVE BUGS:
 • helicopters no longer cast static shadows (like cars do) with incorrect placements,
 • Heat levels > 5 are no longer reset back to 5 when you enter free-roam or start an event,
 • Heat levels > 5 are now shown correctly in menus (requires Binary for missing textures),
 • you can no longer get busted due to line-of-sight issues while the "EVADE" bar fills, and
 • regular roadblock and Heavy / LeaderStrategy spawns no longer slow down in longer pursuits.

You can also ASSIGN NEW (BINARY) STRINGS for the game to display when cop vehicles are destroyed,
similar to the "NFSMW Unlimiter" mod by nlgxzef. Bartender's version of this feature is easier to
configure and will never cause "FENG: Default string error" pop-ups, as it checks every string you
provide against the game's (potentially modified) language files whenever you launch it.






── ■ │ 2 - WHAT DOES THE "ADVANCED" FEATURE SET DO? │ ■ ───────────────────────────────────────────

The "Advanced" feature set LETS YOU CHANGE (per Heat level)
 • how many cops can (re)spawn without backup once a wave is exhausted,
 • the global cop-spawn limit for how many cops in total may chase you at any given time,
 • how quickly (if at all) cops flee the pursuit if they don't belong,
 • what vehicles (any amount, with counts and chances) may spawn to chase and search for you,
 • what vehicles (same liberties as above) may spawn in regular roadblocks,
 • what vehicles (ditto) may spawn as pre-generated cops in scripted events,
 • what vehicles (without counts) may spawn as free patrols when there is no active pursuit,
 • what vehicle spawns in place of the regular helicopter, and
 • when exactly (if at all) the helicopter can (de / re)spawn.

The "Advanced" feature set ALSO FIXES the engagement count displayed above the pursuit board:
The count now accurately reflects how many chasing cop spawns remain in the current wave by 
disregarding vehicles that spawn through any Heavy / LeaderStrategy, vehicles that join
the pursuit by detaching themselves from roadblocks, and the helicopter.






── ■ │ 3 - WHAT MODS ARE (IN)COMPATIBLE WITH BARTENDER? │ ■ ───────────────────────────────────────

Bartender should be FULLY COMPATIBLE with all VltEd and Binary mods. Other .asi mods
without pursuit features should also be compatible unless they are listed below.

Some popular .asi mods REQUIRE CONFIGURATION to be compatible with Bartender:
* In "NFSMW ExtraOptions" by ExOptsTeam, disable the "HeatLevelOverride" feature.
* In "NFSMW Unlimiter" by nlgxzef, disable the "EnableCopDestroyedStringHook" feature.
* In "NFSMW LimitAdjuster" by Zolika1351, disable everything under "[Options]".






── ■ │ 4 - HOW DO I INSTALL BARTENDER FOR MY GAME? │ ■ ────────────────────────────────────────────

BEFORE INSTALLING Bartender:
 1) • make sure you have read and understood the section on mod (in)compatibility above,
 2) • make sure your game's "speed.exe" is compatible (i.e. 5.75 MB / 6,029,312 bytes large), and
 3) • install an .asi loader or any mod with one (e.g. the "WideScreenFix" mod by ThirteenAG).

To INSTALL Bartender:
 1) • if it doesn't exist already, create a "scripts" folder in your game's installation folder;
 2) • copy the contents of Bartender's "scripts" folder into your game's "scripts" folder; and
 3) • (optional) in User Mode of Binary 2.8.3 or newer, load and apply "FixMissingTextures.end".

AFTER INSTALLING Bartender: 
 1) • check out its configuration (.ini) files in the `scripts\BartenderSettings` folder, and
 2) • read the "APPENDIX.txt" file should you encounter any issues or want technical details.

To UNINSTALL Bartender, remove its files from your game's "scripts" folder. There is no need to
remove the optional missing textures, as the game will not use them without Bartender.

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






── ■ │ 6 - WHAT CHANGED IN EACH VERSION? │ ■ ──────────────────────────────────────────────────────

v1.00.00: Initial release
      01: Revised "LIMITATIONS" section of README
      02: Revised multiple sections of README
      03: Yet another minor README revision
      04: README? More like "FIXME"
      05: Clarified "Events" spawns in "Cars.ini"
      06: Clarified string assignment in "Labels.ini"
      07: Clarified ignored VltEd parameters when "Advanced" feature set is enabled
      08: Corrected a few typos in README
      09: Clarified cooldowns in "Supports.ini" and helicopter spawns in "Helicopter.ini"
      10: Revised multiple .ini comments and enforced consistency
      11: Further clarified cooldowns in "Supports.ini"
      12: Improved formatting of .ini files and expanded "LIMITATIONS" section of README
      13: Added compatibility note for VltEd and other .asi mods in "LIMITATIONS" section of README
      14: Added compatibility note for Binary mods in "LIMITATIONS" section of README
      15: Added note about README structure
      16: Clarified stability of "NFSMW LimitAdjuster" in "LIMITATIONS" section of README

   01.00: Fixed a bug with vehicle names containing underscores
      01: Removed some superfluous memory patches

   02.00: Improved thread safety of cop-spawn interceptor functions
      01: Rephrased spawning-related entries in "LIMITATIONS" section of README
      02: Removed redundant push / pop instructions

   03.00: Fixed the Heat-level update in free-roam and a rare bug of the game miscounting cops
      01: Rephrased "LIMITATIONS" section of README yet again

   04.00: Added post-race pursuit hook and changed cop count to decrement after full despawns only
      01: Added workaround for vanilla bug of fleeing HeavyStrategy 3 spawns ("ADVANCED" set only)
      02: Improved phrasing and corrected some typos in README

   05.00: Added "count" support to custom "Roadblocks" and "Events" spawn tables
      01: Rephrased README to better reflect recent changes to both feature sets
      02: Restructured "LIMITATIONS" section of README and renamed both "General.ini" files
      03: Restructured and rephrased README some more
      04: Merged "Behaviour.ini" with "Cars.ini" file
      05: Clarified global cop-spawn limit in "LIMITATIONS" section of README and "Cars.ini" file
      06: Added missing number-format comment to "Others.ini" file

   06.00: Fixed incorrect labelling of roadblock vehicles joining pursuits after spike-strip hits
      01: Made terminology in README and .ini files files more consistent, and rephrased some parts

   07.00: Fixed Heavy / LeaderStrategy spawns not fleeing as per their "Duration" VltEd parameters
      01: Improved general performance of cop management functions for the "ADVANCED" set
      02: Clarified some ambiguous phrasing in "LIMITATIONS" section of README
      03: Clarified the different types of patrol spawns in README and "Cars.ini" file
      04: Rephrased parts of the README for what feels like the millionth time

   08.00: Added support for separate free-roam and race pursuit parameters
      01: Improved performance slightly and expanded "LIMITATIONS" section of README

   09.00: Added fixes for Heat levels above 5 resetting and missing menu textures in Career mode
      01: It's a bird! It's a plane! No, it's yet another fresh batch of phrasing corrections!
      02: Added information about enabling / disabling individual bug fixes to README
      03: Clarified logic of spawn-table copying in "LIMITATIONS" section of README
      04: Corrected mislabelled spawn tables in "LIMITATIONS" section of README
      05: Rephrased a couple more parts of the README to remove ambiguity
      06: Overhauled entire README and renamed parameter group in "Labels.ini" file
      07: Improved clarity of README by replacing most references with nouns

   10.00: Assigned correct spawn table to first cop spawn in Career milestone / bounty pursuits
      01: Improved readability of README by overhauling language and phrasing throughout
      02: Expanded all comments and also rearranged busting parameters in configuration files
      03: Further expanded file parsing and usage notes subsections in README
      04: Edited configuration files of "Advanced" feature set to use the game's vanilla values

   11.00: Fixed "Events" spawn tables not applying to scripted patrol spawns in prologue races
      01: Added missing reset hook for "Events" spawn tables between consecutive (prologue) races
      02: Made phrasing of comments in configuration files more internally consistent
      03: Clarified comments about "NFSMW LimitAdjuster" in "Cars.ini" file and README
      04: Improved performance slightly by removing redundant assembly instructions

   12.00: Added fix for vanilla helicopter shadows and further improved performance
      01: Improved performance even more by removing redundant re-hashing
      02: Fixed rare edge case of "Events" spawns temporarily blocking "Chasers" spawns
      03: Another performance improvement by reducing re-hashing even further

   13.00: Improved performance by rewriting and streamlining the internal file-parsing logic