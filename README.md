Open Syobon Action
==================
###### Original game developer: Chiku (ちく)
###### Open-source port developer: Mathew Velasquez
###### Developer/maintainer of this fork: Karen／明美 (@angelXwind)
###### Ported by: PortBREW

Open Syobon Action (オープンしょぼんのアクション) is an open-source cross-platform port of the unforgiving Japanese platformer using SDL, ported to the Wii U with widescreen and an English translation.

It is also called "Shobon no Action," "Neko Mario," "Cat Mario," or "Cat MeOUCHio."

Notes
-----
* Rather than just playing this game by yourself, it is much more enjoyable to let someone who's never tried it before try playing the game.
* Those who are under 12 years old are not allowed to play.
* Please be aware that by playing this game, there is a possibility that you may develop trust issues towards other humans.
* Please be aware that there is a possibility of getting stressed out while playing this game.
* If any problems are caused by this game, please understand that the developers cannot take any responsibility.
* If you wish to distribute this game to a large amount of people, please give credit to the original developer, Chiku (ちく).
* The game's text was entirely in Japanese (translated to English in this fork), but you do not need to understand the Japanese language in order to play.

Controls
-------------------
| Wii U control        | Game action                        |
|----------------------|------------------------------------|
| D-pad Left / Right   | Move left / right                  |
| Left analogue stick  | Move left / right (analogue)       |
| **A**                | Jump                               |
| **B**                | Jump (alternative)                 |
| **START (+)**        | Return to the title screen (menu)  |
| GamePad touch screen | Confirm / start (acts like Z)      |
| **HOME**             | Exit to the Wii U Menu             |

All four Wii U controller types are supported through the devkitPro SDL2 joystick driver:
* Wii U GamePad
* Wii U Pro Controller
* Classic Controller (attached to a Wii Remote)
* Wii Remote (+ Nunchuk)
Keyboard controls from the original (Z, Up, Enter, 1–9, 0, Space, Esc, F1) still work when a USB keyboard is connected, including the original `;` jump alias.

`HOME` is handled by the SDL2 Wii U driver's ProcUI integration: the app is suspended while the Wii U Menu is open and resumes where it left off, and choosing *Close* from the HOME menu exits cleanly.

Bugs
----
Please report any bugs at the GitHub issue tracker: https://github.com/PortBREW/Syobon-Action-U/issues

Building
-----------------

Everything builds inside the devkitPro Docker image; nothing is installed on the host.

```sh
./build.sh
```

This produces:

| File                  | Purpose                                            |
|-----------------------|----------------------------------------------------|
| `SyobonActionU.rpx`   | Raw Wii U executable                               |
| `SyobonActionU.wuhb`  | Wii U Homebrew Bundle (executable + assets + meta) |

`build.sh` is a thin wrapper around:

```sh
docker run --rm -v "$PWD":/src -w /src devkitpro/devkitppc:latest make
```

Packaging for the SD card
-----------------

```sh
./package.sh
```

creates `dist/wiiu/apps/SyobonActionU/` containing the `.wuhb`, the `.rpx`, and the `BGM/`, `SE/`, `res/` resources (for the loose-`.rpx` case). Copy the `wiiu` folder to the root of your SD card.

Installing and running
-----------------

Requires a Wii U running **Aroma** (or Tiramisu (untested)) with the Homebrew Launcher.

1. Copy `wiiu/apps/SyobonActionU/` to `sd:/wiiu/apps/SyobonActionU/`.
2. Launch **Syobon Action U** from the Homebrew Launcher (`.rpx`) or
   straight from the Wii U Menu (`.wuhb`).

Release Changelog
-----------------
**Syobon Action U**
* Initial release

**Open Syobon Action RC 3**
* Fixed audio on Windows 7 and above

**Open Syobon Action RC 2**
* First Windows release!
* Dvorak keyboard support
* Error messages more detailed and directed to stdout
* Compiled code optimized for speed
* Better initialization and de-initialization code, so you can be confident
 that all resources are freed.
* Optional sound: The game will no longer crash if there is no audio device
 available, and all sound can be disabled with the -nosound parameter
* Minor bugfixes here and there

**Open Syobon Action RC 1**
* Fixed incorrect "box" rendering (used to draw green pipes)
* Fixed random crashing at certain parts (thanks to mekuso for the bug report)
* Volumes now appropriately altered
* Gamepad support added
* Now uses original indexed PNGs, with original filenames (capital PNG)
* Minor tweaks and optimizations

**Open Syobon Action v0.9:**
* Fixed incorrect character rendering, uses font very similar to the original game
 (sazanami-gothic.ttf rather than msgothic.ttf)
* Changed encoding of the original readme and source code to UTF-8, so they can
 be viewed correctly with most software
* Changed DOS newlines to Unix newlines in source and readme
* Source code has been cleaned up with "indent", removed a few obsolete comments

**Open Syobon Action v0.8:**
* First release
