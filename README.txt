
── ■ │ NFSMW Bartender (v1.5.03) │ ■ ──────────────────────────────────────────────────────────────

This .asi mod adds new customisation options to pursuits. These options come in two sets:
 • the "BASIC" set lets you change many otherwise hard-coded values of the game, and
 • the "ADVANCED" set lets you change cop-spawning behaviour and spawn tables without limits.

For DETAILS on which features and configuration files each set includes, see the sections below.
There are also separate sections for "INSTALLATION" instructions and the "LIMITATIONS" of this mod.






── ■ │ "BASIC" FEATURE SET │ ■ ────────────────────────────────────────────────────────────────────

This feature set LETS YOU CHANGE (per Heat level)
 • at which distance and how quickly you can get busted,
 • how long it takes to lose the cops and enter Cooldown mode,
 • at which time interval you gain passive bounty,
 • the maximum bounty multiplier for destroying cops quickly,
 • the internal cooldown for regular roadblock spawns,
 • the internal cooldown for Heavy and LeaderStrategy spawns,
 • which vehicles spawn through HeavyStrategy 3 (ramming SUVs),
 • which vehicles spawn through HeavyStrategy 4 (roadblock SUVs), and
 • which vehicles spawn through LeaderStrategy 5 / 7 (Cross and his henchmen).

This feature set FIXES TWO BUGS:
 • you can no longer get busted due to line-of-sight issues while the "EVADE" bar fills, and
 • regular roadblock and Heavy / LeaderStrategy spawns no longer slow down in longer pursuits.

You can also ASSIGN NEW (BINARY) STRINGS for the game to display when cop vehicles are destroyed,
similar to the "NFSMW Unlimiter" mod by nlgxzef. Compared to Unlimiter, this mod's version of this
feature is easier to configure, leaner, and even checks strings for correctness on game launch,
ignoring any specified strings that do not actually exist in the game's (modified) binary files.

The CONFIGURATION (.ini) FILES for this set are located in "scripts/BartenderSettings/Basic".
To DISABLE a given feature of this set, delete its .ini section or the entire file.

Before you use this feature set, see the "LIMITATIONS" section further below.






── ■ │ "ADVANCED" FEATURE SET │ ■ ─────────────────────────────────────────────────────────────────

This feature set LETS YOU CHANGE (per Heat level)
 • how many cops can (re)spawn without backup once a wave is exhausted,
 • the global spawn limit for how many cops may chase you at once,
 • how quickly cops leave the pursuit if they don't belong (if at all),
 • which vehicles may spawn to chase you (any amount, with counts and chances),
 • which vehicles may spawn in roadblocks (same as above),
 • which vehicles may spawn in scripted events (ditto),
 • which vehicles may spawn as patrols in free-roam (chances only),
 • which vehicle spawns in place of the regular helicopter, and
 • when exactly the helicopter can (de / re)spawn (if at all).

This feature set ALSO FIXES the displayed engagement count in the centre of the pursuit bar:
its value is now perfectly accurate and reflects how many chasing cops remain in the current
wave. The count ignores vehicles spawned through any Heavy or LeaderStrategy, the helicopter, 
and any vehicles that join the pursuit by detaching themselves from roadblocks.

The CONFIGURATION (.ini) FILES for this set are located in "scripts/BartenderSettings/Advanced".
To DISABLE this feature set, delete the entire "scripts/BartenderSettings/Advanced" folder.

Before you use this feature set, see the "LIMITATIONS" section further below.






── ■ │ INSTALLATION │ ■ ───────────────────────────────────────────────────────────────────────────

BEFORE INSTALLING this mod:
 1) • make sure you have read and understood the "LIMITATIONS" section further below,
 2) • make sure your game's "Speed.exe" is compatible (i.e. 5.75 MB / 6.029.312 bytes large), and
 3) • install an .asi loader or any mod with one (e.g. the "WideScreenFix" mod by ThirteenAG).

To INSTALL this mod, copy the contents of its "scripts" folder into the "scripts" folder located
in your game's installation folder. If your game does not have a "scripts" folder, create one.

AFTER INSTALLING this mod, you can customise its features through its configuration (.ini) files.
You can find these configuration files in the "scripts\BartenderSettings" folder.

To UNINSTALL this mod, remove its files from your game's "scripts" folder.

To UPDATE this mod, uninstall it and repeat the installation process above.
Whenever you do, make sure to replace ALL old configuration files properly!






── ■ │ LIMITATIONS │ ■ ────────────────────────────────────────────────────────────────────────────

The two feature sets of this mod each come with caveats and limitations. To avoid nasty surprises 
and game instability, make sure you understand them all before you use this mod in any capacity.

Both feature sets of this mod should be compatible with all VltEd, Binary, and most .asi mods.
All known and notable exceptions to this are explicitly mentioned in this section.



── ■ "BASIC" FEATURE SET ■ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

COP (BINARY) STRINGS ("BartenderSettings\Basic\Labels.ini"):

 • This feature is completely incompatible with the "EnableCopDestroyedStringHook" feature of the
   "NFSMW Unlimiter" mod by nlgxzef. Either delete this mod's "Labels.ini" configuration file or 
   disable Unlimiter's version of the feature by editing its "NFSMWUnlimiterSettings.ini" file.


GROUND SUPPORTS ("BartenderSettings\Basic\Supports.ini"):

 • All vehicles you specify to replace the HeavyStrategy 3 spawns (the ramming SUVs) should each 
   have a low "MAXIMUM_AI_SPEED" value (the vanilla SUVs use 50) in their "aivehicle" VltEd 
   entries. If they don't, they might cause stability issues by joining the pursuit after their 
   ramming attempt(s), as this effectively makes them circumvent the global cop spawn limit.

 • All vehicles you specify to replace Cross in LeaderStrategy 5 / 7 should each not be used by 
   any other cop(s) elsewhere. If another cop uses the same vehicle as Cross, no LeaderStrategy 
   will be able to spawn as long as that cop is present in the pursuit.



── ■ "ADVANCED" FEATURE SET ■ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

GENERAL:

 • The entire feature set disables itself if ANY Heat level lacks a valid "Chasers" spawn table
   ("BartenderSettings\Advanced\Cars.ini"); you must specify at least one vehicle for each level.

 • If the feature set is enabled, the following "pursuitlevels" VltEd parameters are ignored
   because the feature set fulfils their intended purposes with much greater customisation:
   "cops", "HeliFuelTime", "TimeBetweenHeliActive", and "SearchModeHeliSpawnChance".

   
HELICOPTER (DE)SPAWNING BEHAVIOUR ("BartenderSettings\Advanced\Helicopter.ini"):

 • All vehicles you specify to replace the helicopter must each have the "CHOPPER" class assigned to
   them in their "pvehicle" VltEd entries, either explicitly or implicitly through a parent node.


COP (DE)SPAWNING BEHAVIOUR ("BartenderSettings\Advanced\Behaviour.ini"):

 • Until HeavyStrategy 3 and LeaderStrategy spawns have left the pursuit, they can block new
   "Chasers" from spawning. This happens if these spawns push the total number of active cops in
   the world to (or beyond) the global cop spawn limit, which will then prevent further "Chasers" 
   spawns. This total is calculated across all active pursuits, meaning cops spawned in NPC 
   pursuits can also affect how many "Chasers" may spawn in yours.
   
 • Pushing any global cop spawn limit(s) beyond 8 requires the "NFSMW LimitAdjuster" (LA) mod by 
   Zolika1351 to work properly. Without it, the game will start unloading models and assets because
   its default car loader cannot handle the workload of managing (potentially) dozens of vehicles. 
   To make LA compatible with this mod, open its "NFSMWLimitAdjuster.ini" configuration file and 
   disable ALL features in its "[Options]" section; this will fully unlock the spawn limit without 
   forcing an infinite amount of cops to spawn. Note that LA is not perfectly stable either: It is 
   prone to crashing in the first 30 seconds of the first pursuit in a play session, but will 
   generally stay stable if it does not crash there.


COP SPAWN TABLES ("BartenderSettings\Advanced\Cars.ini"):

 • All vehicles you specify in any of the spawn tables must each have the "CAR" class assigned to 
   them in their "pvehicle" VltEd entries, either explicitly or implicitly through a parent node.

 • Vehicles in "Roadblocks" spawn tables are not equally likely to spawn in every vehicle position 
   of a given roadblock formation. This is because the game processes roadblock vehicles in a fixed, 
   formation-dependent order, making it (e.g.) more likely for vehicles with low "count" and high
   "chance" values to spawn in any position(s) that happen to be processed first. This does not
   apply to vehicles with "count" values of at least 5, as no roadblock contains more than 5 cars.

 • Rarely, cops that are not in "Roadblocks" spawn tables might still show up in roadblocks. This 
   is a vanilla bug; it usually happens when the game attempts to spawn a "Chaser" while it is
   processing a roadblock request, causing it to place the wrong car in the requested roadblock.
   This bug is not restricted to cop spawns: if the stars align, it can also happen with traffic.

 • The "Events" spawn tables do NOT apply to the scripted patrols that spawn in any of the prologue
   D-Day races; those spawns are special and a real hassle to deal with, even among event spawns.

 • The "Events" spawn tables do NOT apply to the very first scripted, pre-generated cop that 
   spawns in a given event; instead, this first cop is always of the type specified in the event's 
   "CopSpawnType" VltEd parameter. This is because the game requests this vehicle before it loads 
   any pursuit or Heat level information, making it impossible for the mod to know which spawn 
   table to use for the very first vehicle. This vehicle, however, is still properly accounted for
   in "count" calculations for any following vehicle spawns.

 • "count" values in "Roadblocks" and "Events" spawn tables are ignored whenever the game requests 
   more vehicles in total than these values would allow: Whenever all "count" values have been 
   exhausted for a given roadblock / event, every vehicle in the relevant table may spawn without 
   restriction until the next roadblock / event begins.

 • Making Heat transitions very fast (< 5 seconds) can cause a mix of cops from more than one
   "Events" spawn table to appear in events that feature scripted, pre-generated cops. This happens
   because, depending on your loading times, the game might update the Heat level as it requests 
   those spawns. To circumvent this issue, set the event's "ForceHeatLevel" VltEd parameter to the 
   Heat level you are aiming for instead.






── ■ │ ACKNOWLEDGEMENT(S) │ ■ ─────────────────────────────────────────────────────────────────────

You are free to bundle this mod and any of its .ini files with your own pursuit mod(s), no credit 
required. If you include the .asi file, however, I ask that you do your users a favour and provide
a link to this mod's GitHub repository in your mod's README file.

This mod would not have seen the light of day without
 • rx, for encouraging me to try .asi modding;
 • nlgxzef, for the Most Wanted debug symbols;
 • DarkByte, for Cheat Engine;
 • GuidedHacking, for their Cheat Engine tutorials;
 • trelbutate, for his "NFSMW Cop Car Healthbars" mod as a resource; and
 • Orsal, Aven, Astra King79, and MORELLO, for testing and providing feedback.






── ■ │ CHANGELOG │ ■ ──────────────────────────────────────────────────────────────────────────────

v1.0.00: Initial release
v1.0.01: Revised "LIMITATIONS" section of README
v1.0.02: Revised multiple sections of README
v1.0.03: Yet another minor README revision
v1.0.04: README? More like "FIXME"
v1.0.05: Clarified "Events" spawns in "Cars.ini"
v1.0.06: Clarified string assignment in "Labels.ini"
v1.0.07: Clarified ignored VltEd parameters when "Advanced" feature set is enabled
v1.0.08: Corrected a few typos in README
v1.0.09: Clarified cooldowns in "Supports.ini" and helicopter spawns in "Helicopter.ini"
v1.0.10: Revised multiple .ini comments and enforced consistency
v1.0.11: Further clarified cooldowns in "Supports.ini"
v1.0.12: Improved formatting of .ini files and expanded "LIMITATIONS" section of README
v1.0.13: Added compatibility note for VltEd and other .asi mods in "LIMITATIONS" section of README
v1.0.14: Added compatibility note for Binary mods in "LIMITATIONS" section of README
v1.0.15: Added note about README structure
v1.0.16: Clarified stability comment about "NFSMW LimitAdjuster" in "LIMITATIONS" section of README

v1.1.00: Fixed a bug with vehicle names containing underscores
v1.1.01: Removed some superfluous memory patches

v1.2.00: Improved thread safety of cop-spawn interceptor functions
v1.2.01: Rephrased spawning-related entries in "LIMITATIONS" section of README
v1.2.02: Removed redundant push / pop instructions

v1.3.00: Fixed the Heat-level update in free-roam and a rare bug of the game miscounting cops
v1.3.01: Rephrased "LIMITATIONS" section of README yet again

v1.4.00: Added post-race pursuit hook and changed cop count to decrement after full despawns only
v1.4.01: Added workaround for vanilla bug of fleeing HeavyStrategy 3 spawns ("ADVANCED" set only)
v1.4.02: Improved phrasing and corrected some typos in README

v1.5.00: Added "count" support to custom "Roadblocks" and "Events" spawn tables
v1.5.01: Rephrased README to better reflect recent changes to both feature sets
v1.5.02: Restructured "LIMITATIONS" section of README and renamed both "General.ini" files
v1.5.03: Restructured "LIMITATIONS" section some more