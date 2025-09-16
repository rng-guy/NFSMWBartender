
── ■ │ WHAT IS NFSMW BARTENDER? (v1.17.01) │ ■ ────────────────────────────────────────────────────

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
 • how quickly and at what distances from cops the red "BUSTED" bar fills,
 • how quickly the green "EVADE" bar fills once all cops have lost sight of you,
 • when exactly (if at all) cop vehicles are destroyed if flipped over,
 • when exactly (if at all) racer vehicles are reset if flipped over,
 • when exactly (if at all) interactive music can transition to another track,
 • whether cops in non-player pursuits can request ground supports,
 • the internal cooldown for non-Strategy roadblock requests,
 • the internal cooldown for Strategy requests,
 • when exactly and at what distances (if at all) roadblock cops can join the pursuit,
 • how HeavyStrategy 3 requests interact with non-Strategy roadblock requests,
 • what vehicles spawn through HeavyStrategy 3 (the ramming SUVs),
 • what vehicles spawn through HeavyStrategy 4 (the roadblock SUVs), and
 • what vehicles spawn through LeaderStrategy 5 / 7 (Cross and his henchmen).

The "Basic" feature set ALSO LETS YOU CHANGE (in general)
 • which strings the game shows whenever cop vehicles are destroyed,
 • which radio callsigns and chatter cop vehicles can trigger, and
 • the selection (and order) of interactive music tracks in pursuits.

The "Basic" feature set FIXES SIX BUGS / ISSUES:
 • helicopter mini-map icons are now always visible whenever a helicopter is active,
 • helicopters no longer cast static shadows (like cars do) with incorrect placements,
 • the game is no longer biased in how it chooses to make Strategy requests,
 • Heat levels > 5 are no longer reset back to 5 when you enter free-roam or start an event,
 • Heat levels > 5 are now shown correctly in menus (requires Binary for missing textures), and
 • you can no longer get busted due to line-of-sight issues while the "EVADE" bar fills.






── ■ │ 2 - WHAT DOES THE "ADVANCED" FEATURE SET DO? │ ■ ───────────────────────────────────────────

The "Advanced" feature set LETS YOU CHANGE (per Heat level)
 • how many cops can (re)spawn without backup once a wave of reinforcements is exhausted,
 • the global cop-spawn limit for how many cops in total may chase you at any given time,
 • how quickly (if at all) cops flee a pursuit if they don't belong to the Heat level,
 • what vehicles (any amount, with counts and chances) may spawn to chase and search for you,
 • what vehicles (same liberties as above) may spawn in non-Strategy roadblocks,
 • what vehicles (ditto) may spawn as pre-generated cops in scripted events,
 • what vehicles (without counts) may spawn as free patrols outside pursuits,
 • what vehicle spawns in place of the regular helicopter,
 • when exactly (if at all) the helicopter can (de / re)spawn,
 • how much earlier (if at all) the helicopter rejoins the pursuit when it loses you,
 • the internal cooldown for the helicopter's ramming attempts through HeliStrategy 2,
 • the player-speed threshold for HeavyStrategy 3 spawns to stop their ramming attempts,
 • when exactly (if at all) LeaderStrategy Cross and / or his henchmen become aggressive,
 • when exactly (if at all) the game can request a new LeaderStrategy once Cross is gone, and
 • when exactly (if at all) the game can request a new Strategy while another is still active.

The "Advanced" feature set FIXES THREE BUGS / ISSUES:
 • non-Strategy roadblock and Heavy / LeaderStrategy requests are no longer blocked in races,
 • early Strategy despawns or cancellations no longer stall the game from making new requests, and
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