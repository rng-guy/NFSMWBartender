
── ■ │ WHAT IS NFSMW BARTENDER? (v2.01.00) │ ■ ────────────────────────────────────────────────────

View this document with better formatting on GitHub: https://github.com/rng-guy/NFSMWBartender

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
 • at what time interval you gain passive bounty,
 • the maximum combo-bounty multiplier for destroying cops quickly,
 • how quickly and below what distance from cops the red "BUSTED" bar fills,
 • how quickly the green "EVADE" bar fills once all cops have lost sight of you,
 • whether player-damaged cop vehicles are destroyed instantly if flipped over,
 • when exactly (if at all) cop vehicles are destroyed regardless of damage if flipped over,
 • when exactly (if at all) racer vehicles are reset if flipped over,
 • which ground supports the cops can request in non-player pursuits,
 • the internal cooldown between non-Strategy roadblock requests,
 • the internal cooldown between Heavy / LeaderStrategy requests,
 • when exactly (if at all) and below what distance roadblock vehicles can join pursuits,
 • whether roadblock vehicles react to racers entering "COOLDOWN" mode,
 • whether roadblock vehicles react to racers hitting their spike strips,
 • how HeavyStrategy 3 requests interact with non-Strategy roadblock requests,
 • which vehicles spawn in place of the ramming SUVs through HeavyStrategy 3,
 • which vehicles spawn in place of the roadblock SUVs through HeavyStrategy 4, and
 • which vehicles spawn in place of Cross and his henchmen through LeaderStrategy 5 / 7.

The "Basic" feature set ALSO LETS YOU CHANGE (in general)
 • which strings the game shows whenever cop vehicles are destroyed;
 • which radio callsigns and chatter cop vehicles can trigger; and
 • the selection, order, and length of interactive themes that play in pursuits.

The "Basic" feature set FIXES SIX BUGS / ISSUES:
 • the helicopter mini-map icon is now always visible whenever a helicopter is active,
 • helicopters no longer cast static shadows (like cars do) with incorrect placements,
 • the game is no longer incorrectly biased in how it chooses to make Strategy requests,
 • Heat levels > 5 are no longer reset back to 5 when you enter free-roam or start an event,
 • Heat levels > 5 are now shown correctly in menus (requires Binary for missing textures), and
 • you can no longer get busted due to line-of-sight issues while the green "EVADE" bar fills.






── ■ │ 2 - WHAT DOES THE "ADVANCED" FEATURE SET DO? │ ■ ───────────────────────────────────────────

The "Advanced" feature set LETS YOU CHANGE (per Heat level)
 • how many chasing cops can (re)spawn without backup once a wave of reinforcements is exhausted,
 • the global cop-spawn limit for how many chasing cops in total may be active at any given time,
 • whether spawning decisions for chasing cops are independent of all other pursuit vehicles,
 • when exactly (if at all) and how many chasing cops from other Heat levels can flee the pursuit,
 • which vehicles (any amount, with counts and chances) may spawn to chase and search for racers,
 • which vehicles (same liberties as above) may spawn in non-Strategy roadblocks,
 • which vehicles (ditto) may spawn as pre-generated cops in scripted events,
 • which vehicles (without counts) may spawn as free patrols outside pursuits,
 • which vehicle spawns in place of the regular helicopter,
 • when exactly (if at all) the helicopter can first spawn in each player pursuit,
 • when exactly (if at all) the helicopter can respawn if it runs out of fuel,
 • when exactly (if at all) the helicopter can respawn if it gets wrecked,
 • when exactly (if at all) the helicopter can rejoin the pursuit early if it loses you,
 • when exactly (if at all) the helicopter can run out of fuel after each (re)spawn,
 • the internal cooldown between the helicopter's ramming attempts through HeliStrategy 2,
 • below what racer speed HeavyStrategy 3 spawns cancel their ramming attempts early,
 • when exactly (if at all) LeaderStrategy Cross and / or his henchmen become aggressive,
 • when exactly (if at all) the game can request a new LeaderStrategy once Cross is gone, and
 • when exactly (if at all) the game can request a new Strategy while another is still active.

The "Advanced" feature set FIXES THREE BUGS / ISSUES:
 • non-Strategy roadblock and Heavy / LeaderStrategy requests are no longer blocked in races,
 • early Strategy despawns or cancellations no longer stall the game from making new requests, and
 • the engagement count shown above the pursuit board now (accurately) counts chasing cops only.






── ■ │ 3 - WHAT MODS ARE (IN)COMPATIBLE WITH BARTENDER? │ ■ ───────────────────────────────────────

Almost all VLTED AND BINARY MODS should be fully compatible with all Bartender configurations.
However, Bartender's "Advanced" feature set replaces some "pursuitlevels" VltEd parameters:
 • the "cops" array,
 • "HeliFuelTime",
 • "TimeBetweenHeliActive", and
 • "SearchModeHeliSpawnChance".

Most OTHER .ASI MODS should be fully compatible with all Bartender configurations.
However, some pursuit-related .asi mods require manual (re)configuration for compatibility:
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
 1) • check out the configuration (.ini) files in the new "scripts\BartenderSettings" folder; and
 2) • if you encounter any issues or want more feature details, see the "APPENDIX.txt" file.

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