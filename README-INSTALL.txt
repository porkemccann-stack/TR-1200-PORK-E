TR-1200 - SP-1200 Layout + TR-808 Sounds + Extra Long 808 Boom
For MPC X with latest update (MPC OS 3.x) - VST2

WHAT'S IN THIS ZIP:
  VST2 Plugins/TR-1200/
    plugin.json - name, MIDI map, params
    plugin.cpp  - full synth code, 12-bit crush, long boom
    skin.json   - SP-1200 layout, TR-808 colors (red/orange/yellow/white)
    TR-1200-panel.png - panel art
    CMakeLists.txt
  UI/TR-1200-panel.png - same panel for reference
  GITHUB-BUILD-WORKFLOW.yml - one-click ARM build (rename to .github/workflows/build.yml on GitHub)

HOW TO GET THE .so FOR MPC X (2 MINUTES - SAME AS 808 LONG BOOM):
  MPC X needs an ARM build, not a Windows .dll. This zip uses GitHub Actions to build it.
  1. On your phone or computer, make a new private repo on GitHub, upload this whole TR-1200 folder
  2. Create file .github/workflows/build.yml and paste in GITHUB-BUILD-WORKFLOW.yml contents
  3. Tap Actions tab -> Build TR-1200 for MPC X -> Run workflow
  4. Wait for green check, tap Artifacts -> TR-1200-MPC-X-so -> Download
  5. Unzip that artifact - you get TR-1200.so
  6. Put TR-1200.so inside VST2 Plugins/TR-1200/ next to plugin.json / skin.json

INSTALL ON MPC X - LATEST UPDATE:
  1. Put MPC X in Controller Mode OR plug USB drive / use USB cable to computer
  2. Copy the WHOLE folder "VST2 Plugins" to the ROOT of MPC X Internal Drive
     - If you already have a VST2 Plugins folder, merge - copy TR-1200 folder inside it
     - Path should be: Internal / VST2 Plugins / TR-1200 / TR-1200.so + plugin.json + skin.json + panel
  3. Eject safely, restart MPC X in Standalone
  4. MENU > Preferences > Plugins > Rescan / Install > Find TR-1200 > APPLY
  5. Restart MPC X again when it asks
  6. New Track > Plugin > TR-1200
  7. Set Track Type = Plugin, Monitor = Auto, Master/Level NOT at 000 (000 = mute)
  8. Play Pad 1 = BD LONG

MIDI / PAD MAP:
  Pad 1 / Note 36 - BD LONG - Extra Long Boom - up to 4.0 sec
  Pad 2 / 37 - BD Classic 808
  Pad 3 / 38 - Snare
  Pad 4 / 39 - Clap
  Pad 5 / 40 - Rimshot
  Pad 6 / 41 - Cowbell
  Pad 7 / 42 - Clave
  Pad 8 / 43 - Tom Low
  Pad 9 / 47 - Closed Hat
  Pad 10 / 49 - Crash Cymbal
  Pad 11 / 44 - Tom Mid
  Pad 12 / 45 - Tom High
  Pad 13 / 46 - Conga
  Pad 14 / 48 - Open Hat
  Pad 15 / 50 - Ride
  Pad 16 / 39 - Clap 2

QUICK SETTINGS THAT WON'T CLIP:
  LONG BOOM Decay: 2.8s default - try 2.2s for trap, 3.5s for long boom
  BOOM Level: 0.85
  Master: 0.70-0.75 - DO NOT leave at 1.0, that's what clipped 808 Long Boom
  12-Bit Crush: 0.65 - SP-1200 grit
  Filter: 8000 Hz
  Poly: ON (default) so long tail rings

IF NO SOUND:
  1. Check Master meter moves - if it moves but no sound, check Monitor IN/Auto and headphone routing
  2. Check Track Type = Plugin (not Drum / Audio)
  3. Check Master/Level not 000
  4. Check TR-1200.so is actually in the folder - plugin.json alone won't make sound
  5. Restart MPC X after APPLY

TO COPY TO MPC LIVE:
  Copy the same whole VST2 Plugins/TR-1200 folder to MPC Live internal or external HD, then REGISTER in Preferences > Plugins.

Built for Pork-E / Face Off Muzik - TR-1200 v1.0.0
