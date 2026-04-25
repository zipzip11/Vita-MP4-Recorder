# Vita MP4 Recorder
Vita MP4 Recorder is a plugin that allows to record MP4 video clips during your play sessions.<br>

## Features set
- Allows to record clips of unlimited duration (given enough free storage is available).
- Records clips in MP4 containers with H.264 video codec and AVC audio codec.
- Performs hw encoding in H.264 thanks to sceMp4Rec module.

## How to install
- Put `VitaMP4Recorder.suprx` in your `tai` folder.
- Add the plugin under a section for the game you want to use it for (eg `*GTAVCECTY`) in your `config.txt` file. (Alternatively you can place it under `*ALL` in a section where `main` is disabled (check down for an example) but some apps may crash with this due to the resources requirements).
- If you want to use the plugin with homebrews, put `VitaMP4Recorder.skprx` in your `tai` folder.
- Add the plugin under `KERNEL` section in your `config.txt` file.

Here's an example of a config.txt with the plugin installed in `*ALL` section:
```
*KERNEL
ux0:tai/PSVshell.skprx
ux0:tai/AnalogsEnhancer.skprx
ux0:tai/fd_fix.skprx
ux0:tai/kubridge.skprx
ux0:tai/VitaMP4Recorder.skprx
#ux0:tai/gxmdbg.skprx
ux0:tai/ioplus.skprx
*ALL
ux0:tai/WDNR.suprx
*main
ux0:tai/pngshot.suprx
ur0:tai/henkaku.suprx
*!main
*ALL
ux0:tai/VitaMP4Recorder.suprx
*NPXS10015
ur0:tai/henkaku.suprx
*NPXS10016
ur0:tai/henkaku.suprx
```

## Controls
- L + Select = Open the Config Menu
- L + Start = Start/Stop Recording (Shortcut)
- Triangle = Close Config Menu (when in Config Menu)

## Output Videos
The output videos can be found in `ux0:video` and are automatically imported in the official Video app.<br>

## Known Issues
- Audio recording is not always available and it tends to desync compared to video.
- Applications running at higher than 30 FPS will slowdown to 30 FPS and the final video will result slowed down.
- Applications running this plugin will take slightly longer to boot.

## Credits
- CatoTheYounger and hatoving for testing the plugin.
