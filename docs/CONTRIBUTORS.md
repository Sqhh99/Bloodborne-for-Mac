# Contributors to this port

Thank you to everyone who sent pull requests:

- **[Noel Miller](https://github.com/noelmiller)**
  - macOS 27 support for the GPU process (#7).
  - A survivable GPU setup warning (#10).
  - Touchpad gestures (#8).
  - Buffer arena sizing for Macs with less memory (#13).
  - The HUD item icon above 1080p (#17).
  - Online play (#9, in progress).
  - Survivable co-op renderer stops (#11).
- **[Mojahed Abushawish](https://github.com/MojahedAbushawish)**
  - Lossless texture compression (#33).
  - The upscaler haze fix (#48).
  - Invariant vertex positions, copies held to the end of a render pass, and the loading-screen fix
    (from #20).
  - Mouse look (#20).
  - Guest memory mapped in runs for 16-18 GB Macs (#85, #86).
  - Benchmarks on an M3 Pro.
- **[DiAvisoo](https://github.com/DiAvisoo)**
  - Native ultrawide (#30).
  - Duplicate-launch and game-folder checks (#28).
  - The disappearing-geometry finding (#36).
  - Audio dropout diagnostics (#32).
  - The launcher's Stop button (#31).
- **[satperm](https://github.com/satperm)**
  - The About panel and update check (#23).
  - Dated session logs and crash reports (#40).
- **[soulripper13](https://github.com/soulripper13)**: the game's icon in the Dock (#66).
- **[evill33t](https://github.com/evill33t)**: PS4 runtime safety fixes (#80, #81, #82), the
  long-session crash guards (#100, #114), stale descriptor hardening (#98), the GPU watchdog
  fix (#107), and MetalFX frame
  generation (#64, in progress).
- **[byebyevlad](https://github.com/byebyevlad)**: builds on macOS 15 with Xcode 16 (#141).

Thanks also to everyone who reports issues and tests builds on their Macs. Reports, logs and
measurements that shaped recent releases came from @noelmiller, @MojahedAbushawish, @DiAvisoo,
@satperm, @IlyaDylko (keyboard sprint #61, long-session FPS #55), @wgorecodes (stuck game #71,
preferred display #60), @Frosty2804 (16:10, #75), @lifeos-creator (M3 Pro, #53/#69, tested 0.5.9; the updater, #96), @Sah-D (#47),
@ulhaas, @Ohaq2108 and @rozenstein94 (the opening-movie freeze, #46), @khhan0703 (the opening-movie crash, #84, tested 0.5.9), @Didjus (#18),
@PensionPlanning (#19), @muharremsedona6034gm-hub (#22), @Walltor (#25), @AmberUpraity,
@VanDung-dev, @douglasrlee, @jefer9011 and @mariuszparowa666 (GPU stalls, fixed in 0.5.4.2),
@zhouhaoyaomei-ship-it, @SimonKabb and @juliotwo (the memmove crash, fixed in 0.5.5), @ELLDev
and @GChaucer (the GPU crash in fights, #74), @brewboi (5120×2160 and the cursor, #93), @danielsirbubusiness (#83, tested 0.5.9), @g3rro333333333, @rwv7p56n9s-cmyk, @LICHTYYYVVV and @vtsilva77.
