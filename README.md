
![POV: Cross busts your sorry ass at Rockport's hottest bar.](Thumbnail.jpg "Graphic design is my passion.")

This .asi mod adds new customisation options to pursuits in *Need for Speed: Most Wanted* (2005). These new options come in **two feature sets**:
* the **"Basic"** set allows you to change otherwise hard-coded values of the game, and
* the **"Advanced"** set allows you to specify custom spawn tables for cops without limits.

&nbsp;

&nbsp;

&nbsp;



# "Basic" feature set

The **.ini files** for this feature set are located in `scripts/BartenderSettings/Basic`.

To **disable** a feature of this set, delete its .ini section (or the respective .ini file).

See the **Limitations** section further below before using this feature set.

&nbsp;

This feature set **allows you to change** (per Heat level)
* at which distance and how quickly you can get busted,
* how long it takes to lose the cops and enter Cooldown mode,
* the maximum bounty multiplier for destroying cops quickly,
* the internal cooldown for regular roadblock spawns,
* the internal cooldown for ground support requests,
* which vehicles spawn in place of ramming SUVs,
* which vehicles spawn in place of roadblock SUVs, and
* which vehicles spawn in place of Cross and his henchmen.

&nbsp;

This feature set **fixes two bugs**:
* you can no longer get BUSTED due to line-of-sight issues while the EVADE bar fills, and
* ground supports (Cross, Rhinos etc.) will no longer stop spawning in longer pursuits.

&nbsp;

You can also **assign new (Binary) strings** for the game to display when you destroy a given car, like the [NFSMW Unlimiter](https://github.com/nlgxzef/NFSMWUnlimiter/releases) mod by nlgxzef does. Unlike Unlimiter, this mod's implementation of this feature is easier to configure, leaner, and even checks strings for correctness on game launch: It ignores any that do not actually exist in the game's (modified) binary files.

&nbsp;

&nbsp;

&nbsp;



# "Advanced" feature set

The **.ini files** for this feature set are located in `scripts/BartenderSettings/Advanced`.

To **disable** this feature set, delete all .ini files in that folder (or the folder itself).

See the **Limitations** section further below before using this feature set.

&nbsp;

This feature set **allows you to change** (per Heat level)
* how many cops can spawn without backup once a wave is exhausted,
* the global spawn limit for how many cops may chase you at once,
* how quickly cops leave the pursuit if they don't belong (if at all),
* which vehicles may spawn to chase you (with counts, chances, and no type limit),
* which vehicles may spawn in roadblocks (with chances and no type limit),
* which vehicles may spawn in scripted events (as above),
* which vehicles may spawn as patrols in free-roam (ditto),
* which vehicle spawns in place of the regular helicopter, and
* when exactly the helicopter can (de- / re-)spawn (if it at all).

&nbsp;

This feature set **also fixes** the displayed engagement count in the centre of the pursuit bar: its value is now perfectly accurate and reflects how many "Chaser" cops remain in the current wave. The count ignores support vehicles (e.g. Cross), the helicopter, and any vehicles that join the pursuit by detaching themselves from roadblocks.

&nbsp;

&nbsp;

&nbsp;



# Installation

**Before installing** this mod:
1. make sure your game's "Speed.exe" is compatible (i.e. 5.75 MB / 6.029.312 bytes large), and
2. install an .asi loader or any mod with one (e.g. the [WideScreenFix](https://github.com/ThirteenAG/WidescreenFixesPack/releases/tag/nfsmw) mod by ThirteenAG).

&nbsp;

To **install** this mod, copy its `BartenderSettings` folder and compiled .asi file into the `scripts` folder located in your game's installation folder. If your game does not have a `scripts` folder, create one.

**After installing** this mod, you can configure its features by editing its configuration files. You can find them in the `scripts\BartenderSettings` folder.

To **uninstall** this mod, remove its files from your game's `scripts` folder.

To **reinstall** this mod, uninstall it and repeat the installation process above.

&nbsp;

&nbsp;

&nbsp;



# Limitations

The two feature sets of this mod each come with caveats and limitations. To avoid nasty surprises and instability, make sure you understand them all before you use this mod in any capacity.

&nbsp;

"**Basic**" feature set:
* Any vehicles you specify to replace the ramming SUVs (`BartenderSettings\Basic\Supports.ini`) should each have a low `MAXIMUM_AI_SPEED` value (the vanilla SUVs use 50) in their VltEd `aivehicle` entries. If they don't, they might cause stability issues by joining the pursuit after their ramming attempt(s); this effectively makes them circumvent the global spawn limit, and the (vanilla) game really does not like managing more than 8 active cops for very long.
* The string assignment for cops (`BartenderSettings\Basic\Labels.ini`) is incompatible with the `EnableCopDestroyedStringHook` feature of the [NFSMW Unlimiter](https://github.com/nlgxzef/NFSMWUnlimiter/releases) mod by nlgxzef. Either delete this mod's `Labels.ini` file or disable Unlimiter's version of the feature by editing its `NFSMWUnlimiterSettings.ini` configuration file (should be in its `[Main]` section).

&nbsp;

"**Advanced**" feature set:
* The feature set disables itself if *any* Heat level lacks a valid spawn table for "Chasers" (`BartenderSettings\Advanced\Cars.ini`); you must specify at least one car for each level.
* If the feature set is enabled, the `cops` VltEd entries of all Heat levels are ignored. Apart from those and `SearchModeHeliSpawnChance`, all other VltEd parameters stay functional. (With this feature set, the helicopter will always spawn when its (re-)spawn timer expires.)
* All helicopter vehicles you specify (`BartenderSettings\Advanced\Helicopter.ini`) must each have the `CHOPPER` class assigned to them in their `pvehicle` VltEd entries.
* Rarely, cops that are not in "Roadblock" spawn tables (`BartenderSettings\Advanced\Cars.ini`) might still show up in roadblocks. This is a vanilla bug; it usually happens when the game attempts to spawn a "Chaser" while it is processing a roadblock request, causing it to place the wrong car in the requested roadblock. This bug isn't restricted to cop spawns: if the stars align, it can also happen with traffic cars or even the helicopter.
* The first scripted cop to spawn in a given event is always of the type specified in the event's VltEd entry instead of the "Event" spawn tables (`BartenderSettings\Advanced\Cars.ini`). This is because the game actually requests the first cop before it loads any pursuit or Heat level information, which makes it impossible for the mod to know which table to use for this one car.
* Setting a global cop spawn limit (`BartenderSettings\Advanced\General.ini`) above 8 requires the [NFSMW LimitAdjuster](https://zolika1351.pages.dev/mods/nfsmwlimitadjuster) (LA) mod by Zolika1351 to work properly. Without it, the game will randomly start unloading models and assets because its default car loader cannot handle the workload of managing (potentially) dozens of extra vehicles. To make LA compatible with this mod, open its `LimitAdjuster.ini` file and disable *all* features in its `[Options]` section; this will unlock the spawn limit without forcing an infinite amount of cops to spawn. Note that LA is not perfectly stable either: It is prone to crashing some ~10 seconds into the first pursuit of a given play session, but will generally be stable if it does not.

&nbsp;

&nbsp;

&nbsp;



# Acknowledgement(s)

You are free to bundle this mod and any of its .ini files with your own pursuit mod(s), no **credit required**. If you include the .asi file, however, I ask that you do your users a favour and provide a link to this mod's github repository in your mod's README file.

&nbsp;

This mod would not have seen the light of day without
* **[rx](https://github.com/rxyyy)**, for encouraging me to try .asi modding;
* **[nlgxzef](https://github.com/nlgxzef)**, for the Most Wanted debug symbols;
* **DarkByte**, for [Cheat Engine](https://www.cheatengine.org/);
* **GuidedHacking**, for their [Cheat Engine tutorials](https://www.youtube.com/playlist?list=PLt9cUwGw6CYFSoQHsf9b12kHWLdgYRhmQ);
* **trelbutate**, for his [NFSMW Cop Car Healthbars](https://github.com/trelbutate/MWHealthbars/) mod as a resource; and
* **Orsal**, **Aven**, **Astra King79**, and **MORELLO**, for testing and providing feedback.
