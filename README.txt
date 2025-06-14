
 ── ■ │ NFSMW Bartender (v1.0.0) │ ■ ───────────────────────────────────────────────────────────────

This .asi mod adds new customisation options to pursuits. These options come in two sets:
 • the "Basic" set allows you to change otherwise hard-coded values of the game, and
 • the "Advanced" set allows you to specify custom spawn tables for cops without limits.

For more details on which features (and .ini files) each set includes, see the following sections.






 ── ■ │ "BASIC" FEATURE SET │ ■ ────────────────────────────────────────────────────────────────────

The .ini files for this feature set are located in "scripts/BartenderSettings/Basic".
To DISABLE a given feature of this set, delete its .ini section (or the respective .ini file).

See the "LIMITATIONS" section further below before using this feature set.

This feature set allows you to change (per Heat level)
 • at which distance and how quickly you can get busted,
 • how long it takes to lose the cops and enter Cooldown mode,
 • the maximum bounty multiplier for destroying cops quickly,
 • the internal cooldown for regular roadblock spawns,
 • the internal cooldown for ground support requests,
 • which vehicles spawn in place of ramming SUVs,
 • which vehicles spawn in place of roadblock SUVs, and
 • which vehicles spawn in place of Cross and his henchmen.

This feature set fixes two bugs:
 • you can no longer get BUSTED due to line-of-sight issues while the EVADE bar fills, and
 • ground supports (Cross, Rhinos etc.) will no longer stop spawning in longer pursuits.

You can also assign new (Binary) strings for the game to display when you destroy a given car,
like the "NFSMW Unlimiter" mod by nlgxzef does. Unlike Unlimiter, this mod's implementation of
this feature is much to configure, leaner, and even checks strings for correctness on game
launch: It ignores any that do not actually exist in the game's (modified) binary files.






 ── ■ │ "ADVANCED" FEATURE SET │ ■ ─────────────────────────────────────────────────────────────────

The .ini files for this feature set are located in "scripts/BartenderSettings/Advanced".
To DISABLE this feature set, delete all .ini files in that folder (or the folder itself).

See the "LIMITATIONS" section further below before using this feature set.

This feature set allows you to change (per Heat level)
 • how many cops can spawn without backup once a wave is exhausted,
 • the global spawn limit for how many cops may chase you at once,
 • which cops may spawn to chase you (with counts, chances, and no type limit),
 • which cops may spawn in roadblocks (with chances and no type limit),
 • which cops may spawn in scripted events (as above),
 • which cops may spawn as patrols in free-roam (ditto),
 • which vehicles spawn in place of the regular helicopter,
 • how quickly cops leave the pursuit if they don't belong (if at all), and
 • when exactly the helicopter can (de- / re-)spawn (if it at all).

This feature set also fixes the displayed engagement count in the middle of the pursuit bar:
Its value is now perfectly accurate and reflects how many "chaser" type cops remain in the
current wave. The count ignores support vehicles (e.g. Cross), the helicopter, and any
vehicles that join the pursuit from roadblocks.






 ── ■ │ INSTALLATION │ ■ ───────────────────────────────────────────────────────────────────────────

BEFORE INSTALLING this mod:
 1) • make sure your game's "Speed.exe" is compatible (i.e. 5.75 MB / 6.029.312 bytes large), and
 2) • install an .asi loader or any mod with one (e.g. the "WideScreenFix" mod by ThirteenAG).

To INSTALL this mod, copy the contents of its "scripts" folder into the "scripts" folder located
in your game's installation folder. If your game does not have one, just create it instead.

AFTER INSTALLING this mod, you can configure its features by editing its configuration files.
You can find these configuration (.ini) files in "scripts\BartenderSettings" folder.

To UNINSTALL this mod, remove its files from your game's "scripts" folder.

To REINSTALL this mod, uninstall it and repeat the installation process above.






 ── ■ │ LIMITATIONS │ ■ ────────────────────────────────────────────────────────────────────────────

"Basic" feature set:

 • The string assignment for cops ("BartenderSettings\Basic\Labels.ini") is incompatible with
   the "EnableCopDestroyedStringHook" feature of the "NFSMW Unlimiter" mod by nlgxzef. Either
   delete this mod's "Labels.ini" file or disable Unlimiter's version of the feature by editing
   its "NFSMWUnlimiterSettings.ini" configuration file (should be in its "[Main]" section).


"Advanced" feature set:

 • The entire feature set disables itself if ANY Heat level lacks a valid spawn table for
   "Chasers" ("BartenderSettings\Advanced\Cars.ini"). You must specify at least one car for each.

 • If the feature set is enabled, the "cops" VltEd entries of all Heat levels are ignored.
   Apart from those and "SearchModeHeliSpawnChance", all other VltEd parameters stay functional.

 • Rarely, cops might spawn in roadblocks that are not in the current "Roadblock" spawn table
   ("BartenderSettings\Advanced\Cars.ini"). This is a vanilla bug; it usually happens when the
   game attempts to spawn a "Chaser" while it is processing a roadblock request. This can then
   cause it to place the wrong car in the requested roadblock. This bug isn't restricted to cop
   spawns: It can happen with traffic cars and even the helicopter if the stars align.

 • The first scripted cop to spawn in a given event is always of the type specified in the event's
   VltEd entry, and not taken from the "Event" spawn table ("BartenderSettings\Advanced\Cars.ini").
   This is because the game actually requests the first cop before it loads any pursuit or Heat
   level information, which makes it impossible for the mod to know which spawn table to use.
   
 • Setting a global cop spawn limit ("BartenderSettings\Advanced\General.ini") above 8 requires
   the "NFSMW LimitAdjuster" (LA) mod by Zolika1351 to work properly. Without it, the game will
   randomly start unloading models and assets because its default car loader cannot handle the
   workload of managing (potentially) dozens of extra vehicles. To make LA compatible with this
   mod, open its "LimitAdjuster.ini" file and disable ALL features in its "[Options]" section;
   this will unlock the spawn limit without forcing an infinite amount of cops to spawn. Note
   that LA is not perfectly stable either: It is prone to crashing some ~10 seconds into the
   first pursuit of a given play session, but will generally be stable if it does not.






 ── ■ │ ACKNOWLEDGEMENT(S) │ ■ ─────────────────────────────────────────────────────────────────────

You are free to bundle this mod and any of its .ini files with your own pursuit mod(s),
no credit required. If you include the .asi file, however, I ask that you do your users
a favour and provide a link to this mod's github respository in your mod's README file.

This mod would not have seen the light of day without
 • rx, for encouraging me to try .asi modding;
 • nlgxzef, for the Most Wanted debug symbols;
 • DarkByte, for Cheat Engine;
 • GuidedHacking, for their Cheat Engine tutorials;
 • trelbutate, for his "NFSMW Cop Car Healthbars" mod as a resource; and
 • Orsal, Aven, Astra King79, and MORELLO, for testing and providing feedback.






 ── ■ │ CHANGELOG │ ■ ──────────────────────────────────────────────────────────────────────────────

v1.0.0: Initial release
