
── ■ │ NFSMW Bartender (v1.4.00) │ ■ ──────────────────────────────────────────────────────────────

This .asi mod adds new customisation options to pursuits. These options come in two sets:
 • the "BASIC" set lets you change many otherwise hard-coded values of the game, and
 • the "ADVANCED" set lets you change cop-spawning behaviour and spawn tables without limits.

For more details on which features (and .ini files) each set includes, see the following sections.
There are also separate sections for "INSTALLATION" instructions and the "LIMITATIONS" of this mod.






── ■ │ "BASIC" FEATURE SET │ ■ ────────────────────────────────────────────────────────────────────

The .ini files for this feature set are located in "scripts/BartenderSettings/Basic".
To DISABLE a given feature of this set, delete its .ini section (or the respective .ini file).

See the "LIMITATIONS" section further below before using this feature set.

This feature set lets you change (per Heat level)
 • at which distance and how quickly you can get busted,
 • how long it takes to lose the cops and enter Cooldown mode,
 • at which time interval you gain passive bounty,
 • the maximum bounty multiplier for destroying cops quickly,
 • the internal cooldown for regular roadblock spawns,
 • the internal cooldown for Heavy and LeaderStrategy spawns,
 • which vehicles spawn through HeavyStrategy 3 (ramming SUVs),
 • which vehicles spawn through HeavyStrategy 4 (roadblock SUVs), and
 • which vehicles spawn through LeaderStrategy 5 / 7 (Cross and his henchmen).

This feature set fixes two bugs:
 • you can no longer get BUSTED due to line-of-sight issues while the EVADE bar fills, and
 • regular roadblock and Heavy / LeaderStrategy spawns no longer slow down in longer pursuits.

You can also assign new (Binary) strings for the game to display when cop vehicles are destroyed,
similar to the "NFSMW Unlimiter" mod by nlgxzef. Compared to Unlimiter, this mod's version of this
feature is easier to configure, leaner, and even checks strings for correctness on game launch,
ignoring any specified strings that do not actually exist in the game's (modified) binary files.






── ■ │ "ADVANCED" FEATURE SET │ ■ ─────────────────────────────────────────────────────────────────

The .ini files for this feature set are located in "scripts/BartenderSettings/Advanced".
To DISABLE this feature set, delete all .ini files in that folder (or the folder itself).

See the "LIMITATIONS" section further below before using this feature set.

This feature set lets you change (per Heat level)
 • how many cops can (re)spawn without backup once a wave is exhausted,
 • the global spawn limit for how many cops may chase you at once,
 • how quickly cops leave the pursuit if they don't belong (if at all),
 • which vehicles may spawn to chase you (any amount, with counts and chances),
 • which vehicles may spawn in roadblocks (any amount, with chances),
 • which vehicles may spawn in scripted events (same as above),
 • which vehicles may spawn as patrols in free-roam (ditto),
 • which vehicle spawns in place of the regular helicopter, and
 • when exactly the helicopter can (de / re)spawn (if at all).

This feature set also fixes the displayed engagement count in the centre of the pursuit bar:
its value is now perfectly accurate and reflects how many chasing cops remain in the current
wave. The count ignores vehicles spawned through any Heavy or LeaderStrategy, the helicopter, 
and any vehicles that join the pursuit by detaching themselves from roadblocks.






── ■ │ INSTALLATION │ ■ ───────────────────────────────────────────────────────────────────────────

BEFORE INSTALLING this mod:
 1) • make sure you have read and understood the "LIMITATIONS" section of this README below,
 2) • make sure your game's "Speed.exe" is compatible (i.e. 5.75 MB / 6.029.312 bytes large), and
 3) • install an .asi loader or any mod with one (e.g. the "WideScreenFix" mod by ThirteenAG).

To INSTALL this mod, copy the contents of its "scripts" folder into the "scripts" folder located
in your game's installation folder. If your game does not have a "scripts" folder, create one.

AFTER INSTALLING this mod, you can customise its features by editing its configuration files.
You can find these configuration (.ini) files in the "scripts/BartenderSettings" folder.

To UNINSTALL this mod, remove its files from your game's "scripts" folder.

To REINSTALL this mod, uninstall it and repeat the installation process above.






── ■ │ LIMITATIONS │ ■ ────────────────────────────────────────────────────────────────────────────

The two feature sets of this mod each come with caveats and limitations. To avoid nasty surprises 
and game instability, make sure you understand them all before you use this mod in any capacity.

Both feature sets of this mod should be compatible with all VltEd, Binary, and most .asi mods.
All known and notable exceptions to this are explicitly mentioned in this section.


"BASIC" feature set:

 • All vehicles you specify to replace the ramming SUVs ("BartenderSettings\Basic\Supports.ini") 
   should each have a low "MAXIMUM_AI_SPEED" value (the vanilla SUVs use 50) in their VltEd 
   "aivehicle" entries. If they don't, they might cause stability issues by joining the pursuit 
   after their ramming attempt(s); this effectively makes them circumvent the global spawn limit,
   and the (vanilla) game really does not like managing more than 8 active cops for very long.

 • All vehicles you specify to replace Cross ("BartenderSettings\Basic\Supports.ini") should each
   not be used by any other cop(s) elsewhere. If another cop uses the same vehicle as Cross, no
   LeaderStrategy will be able to spawn as long as that cop is present in the pursuit.

 • The string assignment for cops ("BartenderSettings\Basic\Labels.ini") is incompatible with
   the "EnableCopDestroyedStringHook" feature of the "NFSMW Unlimiter" mod by nlgxzef. Either
   delete this mod's "Labels.ini" configuration file or disable Unlimiter's version of the 
   feature by editing its "NFSMWUnlimiterSettings.ini" configuration file.


"ADVANCED" feature set:

 • The feature set disables itself if ANY Heat level lacks a valid "Chasers" spawn table
   ("BartenderSettings\Advanced\Cars.ini"); you must specify at least one car for each level.

 • If the feature set is enabled, the following "pursuitlevels" VltEd parameters are ignored
   because the feature set fulfils their intended purposes with much greater customisation:
   "cops", "HeliFuelTime", "TimeBetweenHeliActive", and "SearchModeHeliSpawnChance".

 • All vehicles you specify in any of the spawn tables ("BartenderSettings\Advanced\Cars.ini")
   must each have the "CAR" class assigned to them in their "pvehicle" VltEd entries.

 • All vehicles you specify to replace the helicopter ("BartenderSettings\Advanced\Helicopter.ini")
   must each have the "CHOPPER" class assigned to them in their "pvehicle" VltEd entries.

 • Rarely, cops that are not in "Roadblock" spawn tables ("BartenderSettings\Advanced\Cars.ini")
   might still show up in roadblocks. This is a vanilla bug; it usually happens when the game 
   attempts to spawn a "Chaser" while it is processing a roadblock request, causing it to place 
   the wrong car in the requested roadblock. This bug isn't restricted to cop spawns: if the stars
   align, it can also happen with traffic cars or even the helicopter.

 • The "Event" spawn tables ("BartenderSettings\Advanced\Cars.ini") apply only after the first
   scripted, pre-generated cop has already spawned in a given event; instead, this first cop is
   always of the type specified in the event's "CopSpawnType" VltEd parameter. This is because 
   the game requests this cop before it loads any pursuit or Heat level information, making it 
   impossible for the mod to know which spawn table to use for the very first vehicle.

 • Making Heat transitions very fast (< 5 seconds) can cause a mix of cops from more than one
   "Event" spawn table ("BartenderSettings\Advanced\Cars.ini") to appear in events that feature
   scripted, pre-generated cops. This happens because, depending on your loading times, the game
   might update the Heat level as it requests those cops. To circumvent this issue, set the
   event's "ForceHeatLevel" VltEd parameter to the Heat level you are aiming for instead.

 • Until the HeavyStrategy 3 and LeaderStrategy spawns have left the pursuit, they can block new
   "Chasers" from spawning. This happens if these spawns pushed the total number of active cops in
   the world to (or beyond) the global cop spawn limit ("BartenderSettings\Advanced\General.ini"), 
   which will then prevent further "Chasers" spawns. This total is calculated across all pursuits,
   meaning cops spawned in NPC pursuits can also affect how many "Chasers" can spawn in yours.
   
 • Setting any global cop spawn limit ("BartenderSettings\Advanced\General.ini") above 8 requires
   the "NFSMW LimitAdjuster" (LA) mod by Zolika1351 to work properly. Without it, the game will
   randomly start unloading models and assets because its default car loader cannot handle the
   workload of managing (potentially) dozens of extra vehicles. To make LA compatible with this
   mod, open its "LimitAdjuster.ini" configuration file and disable ALL features in its "[Options]"
   section; this will unlock the spawn limit without forcing an infinite amount of cops to spawn. 
   Note that LA is not perfectly stable either: It is prone to crashing in the first 30 seconds of
   the first pursuit in a play session, but will generally stay stable if it does not crash there.






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
v1.0.05: Clarified "Event" spawns in "Cars.ini"
v1.0.06: Clarified string assignment in "Labels.ini"
v1.0.07: Clarified ignored VltEd parameters when "Advanced" feature set is enabled
v1.0.08: Corrected a few typos in README
v1.0.09: Clarified cooldowns in Supports.ini and helicopter spawns in "Helicopter.ini"
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