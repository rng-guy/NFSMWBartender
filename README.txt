
── ■ │ WHAT IS NFSMW BARTENDER? (v1.10.01) │ ■ ────────────────────────────────────────────────────

You can view this document with web formatting on GitHub: https://github.com/rng-guy/NFSMWBartender

Bartender adds NEW CUSTOMISATION OPTIONS to pursuits. These options come in two feature sets:
 • the "BASIC" FEATURE SET lets you change many otherwise hard-coded values of the game, and
 • the "ADVANCED" FEATURE SET lets you change cop-spawning behaviour and tables without limits.

The sections below ADDRESS THE FOLLOWING QUESTIONS in detail:
 1) • What does the "Basic" feature set do?
 2) • What does the "Advanced" feature set do?
 3) • What should I know before I use Bartender?
 4) • How do I install Bartender for my game?
 5) • How may I share or bundle Bartender?
 6) • What changed in each version?






── ■ │ 1 - WHAT DOES THE "BASIC" FEATURE SET DO? │ ■ ──────────────────────────────────────────────

The "Basic" feature set LETS YOU CHANGE (per Heat level)
 • at which distance and how quickly you can get busted,
 • how long it takes to fill the "EVADE" bar and enter "COOLDOWN" mode,
 • at which time interval you gain passive bounty,
 • the maximum combo-bounty multiplier for destroying cops quickly,
 • the internal cooldown for regular roadblock spawns,
 • the internal cooldown for Heavy and LeaderStrategy spawns,
 • which vehicles spawn through HeavyStrategy 3 (the ramming SUVs),
 • which vehicles spawn through HeavyStrategy 4 (the roadblock SUVs), and
 • which vehicles spawn through LeaderStrategy 5 / 7 (Cross and his henchmen).

The "Basic" feature set FIXES FOUR BUGS:
 • Heat levels > 5 are no longer reset back to 5 when you enter free-roam or start an event,
 • Heat levels > 5 are now shown correctly in menus (requires Binary for missing textures),
 • you can no longer get busted due to line-of-sight issues while the "EVADE" bar fills, and
 • regular roadblock and Heavy / LeaderStrategy spawns no longer slow down in longer pursuits.

You can also ASSIGN NEW (BINARY) STRINGS for the game to display when cop vehicles are destroyed,
similar to the "NFSMW Unlimiter" mod by nlgxzef. Bartender's version of this feature is easier to
configure and will never cause "FENG: Default string error" popups, as it checks every string you
specify against the game's (potentially modified) resource files whenever you launch it.






── ■ │ 2 - WHAT DOES THE "ADVANCED" FEATURE SET DO? │ ■ ───────────────────────────────────────────

The "Advanced" feature set LETS YOU CHANGE (per Heat level)
 • how many cops can (re)spawn without backup once a wave is exhausted,
 • the global cop-spawn limit for how many cops in total may chase you at any given time,
 • how quickly (if at all) cops flee the pursuit if they don't belong,
 • which vehicles (any amount, with counts and chances) may spawn to chase and search for you,
 • which vehicles (same liberties as above) may spawn in regular roadblocks,
 • which vehicles (ditto) may spawn as pre-generated cops in scripted events,
 • which vehicles (without counts) may spawn as free patrols when there is no active pursuit,
 • which vehicle spawns in place of the regular helicopter, and
 • when exactly (if at all) the helicopter can (de / re)spawn.

The "Advanced" feature set ALSO FIXES the displayed engagement count in the centre of the pursuit
bar at the bottom of the pursuit HUD: The count now accurately reflects how many chasing cop spawns
remain in the current wave by ignoring vehicles that spawn through any Heavy / LeaderStrategy, 
vehicles that join the pursuit from roadblocks, and the helicopter.






── ■ │ 3 - WHAT SHOULD I KNOW BEFORE I USE BARTENDER? │ ■ ─────────────────────────────────────────

If you configure Bartender improperly, it might cause STABILITY ISSUES in your game due to how much
control it gives you over the game's cop-spawning logic. Also, there are a few quirks to the way 
Bartender parses its configuration files that you should be aware of before you edit any of them.

To help you AVOID GAME INSTABILITY, the subsections below contain
 • everything you need to make informed edits to Bartender's configuration files, and
 • detailed compatibility notes for mods known to conflict with any Bartender features.

Barring any exceptions mentioned in the subsections below, Bartender should be FULLY COMPATIBLE
with all VltEd and Binary mods. Any .asi mods without pursuit features should also be compatible.



── ■ 3.1 - WHAT SHOULD I KNOW ABOUT CONFIGURATION PARSING? ■ - - - - - - - - - - - - - - - - - - - 

Some PARAMETER GROUPS (indicated by "[...]") in Bartender's configuration files allow you to
specify a "default" value (see their comments). Bartender parses these groups in three steps:
 1) • Bartender sets this "default" to the game's vanilla value(s) if you omit it,
 2) • Bartender sets all free-roam Heat levels (format: "heatXY") you omit to this "default", and
 3) • Bartender sets all race Heat levels (format: "raceXY") you omit to their free-roam values.

Bartender treats all INVALID VALUES you specify in its configuration files as omitted instead.
A value is invalid if, for example, you specify a decimal where Bartender expects a whole number.
Negative numbers are never invalid: Bartender sets them to 0 instead when it parses a file.



── ■ 3.2 - WHAT SHOULD I KNOW ABOUT THE "BASIC" FEATURE SET? ■ - - - - - - - - - - - - - - - - - - 

Regarding the "BASIC" feature set IN GENERAL:

 • The configuration (.ini) files for this feature set are located in "BartenderSettings/Basic".

 • You can disable any feature of this set by deleting the file containing its parameters.
   Bug fixes, however, don't have parameters and are tied to specific files implicitly instead.

 • To disable the two Heat-level fixes, delete all configuration files of this feature set.

 • The Heat-level reset fix is incompatible with the "HeatLevelOverride" feature of the 
   "NFSMW ExtraOptions" mod by ExOptsTeam. To disable this ExtraOptions feature, edit its
   "NFSMWExtraOptionsSettings.ini" configuration file. If you do this, you can still change the
   maximum available Heat level with VltEd: The "0xe8c24416" parameter of a given "race_bin_XY"
   VltEd entry determines the maximum Heat level (1-10) while you are at Blacklist rival #XY.

 • If you don't install the optional missing textures ("FixMissingTextures.end"), then the game
   will not display a number next to Heat gauges in menus for cars with Heat levels above 5.
   Whether you install these textures does not affect the Heat-level reset fix in any way.


Regarding COP (BINARY) STRINGS ("BartenderSettings\Basic\Labels.ini"):

 • This feature is incompatible with the "EnableCopDestroyedStringHook" feature of the 
   "NFSMW Unlimiter" mod by nlgxzef. Either delete Bartender's "Labels.ini" configuration file
   or disable Unlimiter's version of the feature by editing its "NFSMWUnlimiterSettings.ini" file.


Regarding GROUND SUPPORTS ("BartenderSettings\Basic\Supports.ini"):

 • Deleting this file also disables the fix for slower roadblock and Heavy / LeaderStrategy spawns.

 • You should assign low "MAXIMUM_AI_SPEED" values (~50 should be fine) to the "aivehicle" VltEd
   entries of every vehicle you specify to replace the HeavyStrategy 3 spawns (the ramming SUVs).
   If you don't limit their speeds, they might cause stability issues by joining the pursuit
   as regular cops after their ramming attempt(s) regardless of the global cop-spawn limit.

 • Don't use the vehicles you specify to replace Cross in LeaderStrategy 5 / 7 spawns anywhere
   else in the game. If you reuse them, no LeaderStrategy will spawn as long as they are present.


Regarding UNCATEGORISED FEATURES ("BartenderSettings\Basic\Others.ini"):

 • Deleting this file also disables the fix for getting busted while the "EVADE" bar fills.



── ■ 3.3 - WHAT SHOULD I KNOW ABOUT THE "ADVANCED" FEATURE SET? ■ - - - - - - - - - - - - - - - - -

Regarding the "Advanced" feature set IN GENERAL:

 • The configuration (.ini) files for this feature set are located in "BartenderSettings/Advanced".

 • You can disable any feature of this set by deleting the file containing its parameters. This
   does not apply to the engagement-count fix, which is tied to this entire feature set instead.

 • You must specify at least one vehicle in each free-roam "Chasers" spawn table
   ("BartenderSettings\Advanced\Cars.ini"), else Bartender will disable this entire feature set.

 • You should ensure that every HeavyStrategy you include in a given Heat level's "pursuitsupport"
   VltEd entry is only listed there once (e.g. there is not a second HeavyStrategy 3), and that
   there is also no more than one LeaderStrategy listed there. If you have duplicates there,
   they can cause the game (and Bartender) to misread their "Duration" VltEd parameters.

 • If this feature set is enabled, the following "pursuitlevels" VltEd parameters are ignored
   because this feature set fulfils their intended purposes with extended customisation:
   "cops", "HeliFuelTime", "TimeBetweenHeliActive", and "SearchModeHeliSpawnChance".

   
Regarding HELICOPTER (DE / RE)SPAWNING ("BartenderSettings\Advanced\Helicopter.ini"):

 • You must assign the "CHOPPER" class to the "pvehicle" VltEd entries of all vehicles you specify
   to replace the regular helicopter, either directly or through their parent VltEd entries.


Regarding COP (DE / RE)SPAWNING ("BartenderSettings\Advanced\Cars.ini"):

 • Until HeavyStrategy 3 and LeaderStrategy spawns have left the pursuit, they can block new
   "Chasers" from spawning (but not the other way around). This is vanilla behaviour: These
   spawns also count toward the total number of cops loaded by the game, which the game compares
   against the global cop-spawn limit to make spawn decisions for "Chasers". Cops spawned in NPC
   pursuits can also affect how many "Chasers" the game may spawn in yours, as the total number
   of cops loaded by the game includes all non-roadblock cars of every active pursuit at once.
   
 • You must install the "NFSMW LimitAdjuster" mod (LA) by Zolika1351 for game stability if you set
   any global cop-spawn limit above 8. Without LA, the game will start unloading models and assets
   because its default cop loader simply cannot handle managing more than 8 vehicles for very long. 
   To fully unlock the global cop-spawn limit without taking spawning control away from Bartender,
   open LA's "NFSMWLimitAdjuster.ini" configuration file and disable EVERYTHING in its "[Options]"
   parameter group. Even with these changes, LA itself might still crash sometimes.

 • You must assign the "CAR" class to the "pvehicle" VltEd entries of all vehicles you specify
   in any of the spawn tables, either directly or through their parent VltEd entries.

 • All free-roam "Roadblocks", "Events", and "Patrols" spawn tables that you leave empty become
   copies of their respective "Chasers" tables (which must each contain at least one vehicle).

 • All race spawn tables that you leave empty become copies of their free-roam counterparts.

 • Vehicles in "Roadblocks" spawn tables are not equally likely to spawn in every vehicle position 
   of a given roadblock formation. This is because the game processes roadblock vehicles in a fixed, 
   formation-dependent order, making it (e.g.) more likely for vehicles with low "count" and high
   "chance" values to spawn in any position that happens to be processed first. This does not apply
   to vehicles with "count" values of at least 5, as no roadblock contains more than 5 cars.

 • Rarely, cops that are not in "Roadblocks" spawn tables might still show up in roadblocks.
   This is a vanilla bug: it usually happens when the game attempts to spawn a "Chaser" while it
   is processing a roadblock request, causing it to place the wrong car in the requested roadblock.
   This bug is not restricted to cop spawns: if the stars align, it can even happen with traffic.

 • The "Events" spawn tables don't apply to the scripted patrols that spawn in any of the prologue
   D-Day events; those spawns are special and a real hassle to deal with, even among event spawns.

 • The "Events" spawn tables don't apply to the very first scripted, pre-generated cop that 
   spawns in a given event. Instead, this first cop is always of the type listed in the event's 
   "CopSpawnType" VltEd parameter. This is because the game requests this vehicle before it loads
   any pursuit or Heat-level information, making it impossible for Bartender to know which spawn
   table to use for this single vehicle.

 • Bartender temporarily ignores the "count" values in "Roadblocks" and "Events" spawn tables 
   whenever the game requests more roadblock / event vehicles in total than they would allow.

 • You should not use fast Heat transitions ("0x80deb840" VltEd parameter(s) set to < 5 seconds),
   else you might see a mix of cops from more than one "Events" spawn table appear in events with
   scripted, pre-generated cops. This happens because, depending on your loading times, the game
   might update the Heat level as it requests those spawns. You can avoid this issue by setting
   the event's "ForceHeatLevel" VltEd parameter to the target Heat level instead.

 • Bartender uses different spawn tables for each of the two patrol-spawn types in the game:
   "Patrols" tables replace the free patrols that spawn when there is no active pursuit,
   and "Chasers" tables replace the searching patrols that spawn in pursuits when you are
   in "COOLDOWN" mode. You can control the number of patrol spawns through the "NumPatrolCars"
   VltEd parameter, but there are two important quirks: Free patrol spawns ignore the global
   cop-spawn limit, while searching patrol spawns ignore the remaining engagement count.






── ■ │ 4 - HOW DO I INSTALL BARTENDER FOR MY GAME? │ ■ ────────────────────────────────────────────

BEFORE INSTALLING Bartender:
 1) • make sure you have read and understood the "WHAT SHOULD I KNOW...?" section above,
 2) • make sure your game's "speed.exe" is compatible (i.e. 5.75 MB / 6,029,312 bytes large), and
 3) • install an .asi loader or any mod with one (e.g. the "WideScreenFix" mod by ThirteenAG).

To INSTALL Bartender:
 1) • if it doesn't exist already, create a "scripts" folder in your game's installation folder;
 2) • copy the contents of Bartender's "scripts" folder into your game's "scripts" folder; and
 3) • (optional) in User Mode of Binary 2.8.3 or newer, load and apply "FixMissingTextures.end".

AFTER INSTALLING Bartender, you can customise its features through its configuration (.ini) files.
You can find these configuration files in the game's new "scripts\BartenderSettings" folder.

To UNINSTALL Bartender, remove its files from your game's "scripts" folder. There is no need to
remove the optional missing textures, as the game doesn't ever use them without Bartender.

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
      01: Improved readability of README by simplifying language and phrasing throughout