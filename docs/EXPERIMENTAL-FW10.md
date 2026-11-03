# Experimental PS5 10.xx capture bring-up

This branch adds a **video-first, unverified PS5 10.xx path** to PlayBridge's native ELF. It is intended for initial testing on firmware **10.20**. The existing, console-tested **13.60** code path and its fixed addresses are retained.

## Evidence and scope

The user-supplied `PBS-Payload-0.3.0-beta.3-ps5.elf` was inspected statically:

- `pbs_ps5_find_offsets` at ELF RVA `0xdaa0` selects a 10.01-family profile for 10.20 (firmware comparison masks the low build bits).
- `pbs_ps5_offsets_1001` at `0xd3130` and `pbs_ps5_offsets_1060` at `0xd3178` both encode a `0x2b8` AVCap2 request and the same parameter field positions. The 13.60 table (`0xd3448`) instead has a `0x2e8` request.
- PBS references `sceResourceArbitratorSetCallback`, `sceResourceArbitratorClearCallback`, `sceResourceArbitratorChangeState`, and `sceAvcap2GetNonVclNalUnitStream`. The 10.xx path resolves these by name, rather than reusing PlayBridge's 13.60 internal RVAs.
- PBS performs capture calls through a remote-process path. **A matching request layout does not prove the same calls will work from PlayBridge's own payload process.** This is a testable hypothesis, not verified 10.xx compatibility.

The 10.xx branch currently supports 1080p video, confirmed by a 10.20 console test. AAC audio is now **experimental**: it attempts the existing AVCap2 audio path, but preserves video streaming if audio initialization fails. Higher requested resolutions fall back to 1080p. The original 13.60 path retains its previous audio, resolutions, and transport protocol. Firmware outside 10.01–10.60 and 13.60 is rejected.

## Build and test on 10.20

You need the existing jailbreak/homebrew runtime with an ELF loader; this code does **not** exploit or jailbreak firmware.

1. Check out `feature/experimental-fw10xx-capture`.
2. Build with the instructions in [BUILDING.md](BUILDING.md), preferably using a distinct output file:

   ```powershell
   python tools/build_payload.py --sdk C:\SDK\ps5-payload-sdk --llvm "C:\Program Files\LLVM\bin" --output build\PlayBridge-fw10.elf
   ```

3. Launch that compiled ELF manually with your normal Payload Manager/ELF loader.
4. In PlayBridge on Windows, enable **ELF already loaded in Payload Manager** and use the **Balanced** 1080p profile. Connect to port 9076 within 60 seconds.
5. If video fails, retrieve `/data/ps5screen-av3.txt` from the PS5 and share lines from `fw=` through `OpenVideo`, `GetHeader`, and `Start` (or the first error). These stages identify which API/ABI assumption failed.

Do not upload the supplied PBS binary as part of this repository. It was used as a reference only.

## Optional 10.20 native GameLiveStreaming resource probe

The decrypted `NPXS40142` GameLiveStreamingDaemon uses ResourceArbitrator resource ID **200** for `RUNNING`, `STOP`, and `RUNNING_AUDIO_ONLY` transitions. PlayBridge's working 10.20 AVCap2 request uses **12**. **ID 200 is not established as a game-only video source:** it might be only the native broadcast service's own lifecycle resource. Trying it could fail or interfere with active broadcast.

To test this hypothesis without changing the known-working build, the GitHub Actions workflow produces **two separate** ELFs:

1. **`PlayBridge-fw10-experimental.elf`**: unchanged, working 10.20 capture (resource 12). Recommended for normal streaming.
2. **`PlayBridge-fw1020-gls-resource-probe.elf`**: *opt-in*, unverified 10.20-only experiment (resource 200 in both the AVCap2 request and ResourceArbitrator state change).

To build the probe yourself:

```powershell
python tools/build_payload.py --gls-resource-probe --sdk C:\\SDK\\ps5-payload-sdk --llvm "C:\\Program Files\\LLVM\\bin" --output build\\PlayBridge-fw1020-gls-resource-probe.elf
```

Test with the regular PlayBridge Windows viewer. Do **not** run a native Twitch broadcast concurrently. With a game running, trigger an ordinary notification and compare the local TV against the PlayBridge image. Record whether `Resource register`, `Resource request`, `OpenVideo`, and `Start` succeed in `/data/ps5screen-av3.txt`. If the probe fails to open video, switch back to the default artifact. Do not interpret a successful `OpenVideo` as proof that notifications are filtered; the visual A/B test is essential.

This experiment does not suppress PS5 notifications locally or implement privacy filtering. The 13.60 code path is untouched.

## Native game-only source research (10.20)

Static tracing of the decrypted `NPXS40100` AvCaptureManager reveals three distinct video-device implementations:

- `SceAvCaptureDeviceVideoScanIn` (name-return routine at ELF file offset `0x19950`)
- `SceAvCaptureDeviceVideoNormal` (routine at `0x1a9a0`)
- `SceAvCaptureDeviceVideoSecond` (routine at `0x1b270`)

AvCaptureManager also invokes `sceVencSetNormalPrivacyProtectionConfig` in multiple device paths (for example around file offsets `0x18dc8`, `0x1ac58`, and `0x1b1fb`), passing a 24-byte block derived from the active device state. **These findings do not establish which source or privacy flag hides notifications.** The tested GLS resource ID 200 produced normal video *with* notifications, so resource 200 alone is ruled out as a filter.

The external [Glint AVCap2 capture notes](https://github.com/MiladRTX/Glint/blob/main/docs/CAPTURE-INTERNALS.md) document another relevant operation, `sceAvcap2EnableScanInEncode()`, before `sceAvcap2OpenVideo()`. A third **10.20-only, opt-in** test build now calls that AVCap2 export after the resource request and immediately before OpenVideo. It retains **resource 12** and the otherwise working request unchanged:

- `PlayBridge-fw1020-scanin-source-probe.elf` — compiles with `--scanin-source-probe`.

```powershell
python tools/build_payload.py --scanin-source-probe --sdk C:\\SDK\\ps5-payload-sdk --llvm "C:\\Program Files\\LLVM\\bin" --output build\\PlayBridge-fw1020-scanin-source-probe.elf
```

**Hardware test:** Run a game with the regular PlayBridge Windows viewer, without a native broadcast. Check the `ScanIn source probe: EnableScanInEncode` return code, then trigger a PS5 notification and compare the TV with the stream. An absent export or non-zero return means the experiment may not have activated. If the console or capture behaves unexpectedly, stop the payload and return to the default ELF; a console restart may be necessary if a system-wide ScanIn state persists. This is **not** a verified game-only method. Never combine it with the separate GLS resource 200 probe.

## Expected limitations

- A 10.20 console test confirmed video capture works with the revised request. AAC audio has not yet been verified on 10.20.
- PBS's remote-process capture path differs from PlayBridge's direct path. If `Initialize`, resource acquisition, or `OpenVideo` fails, porting only offsets will not be sufficient.
- 10.xx AAC audio is experimental and may be unavailable; no 1440p or 4K support is claimed.
- Do not package or distribute a Windows release as supporting 10.xx until console validation and an ELF build have both passed.

## Return codes

- 9: firmware is outside the selected families.
- 10: missing `sceAvcap2GetVideoOutMode` export.
- 11: `ResourceArbitrator` module base unavailable.
- 12–13: resource registration/channel setup failed.
- 16: required ResourceArbitrator exports were not resolved.

The original log messages give more detail for all other failures.
