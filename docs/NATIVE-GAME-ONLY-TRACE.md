# Native game-only source trace (PS5 firmware 10.20)

Static-analysis notebook for making Sony notifications remain on the local TV while excluding them from the PlayBridge video stream. **No notification-free solution has been verified.** This document makes no changes to capture behavior.

## Tested hardware outcomes

| Experimental capture path | AVCap2 result | Notifications in PlayBridge |
| --- | --- | --- |
| ResourceArbitrator ID 12 (baseline) | Open/Start work | Visible |
| ResourceArbitrator ID 200 (native GLS hypothesis) | Open/Start work | Visible |
| Resource ID 12 + `sceAvcap2EnableScanInEncode()` | Enable/Open/Start return zero | Visible |

Neither resource 200 nor enabling ScanIn encode is sufficient. Retain the tested resource-12 build as the default.

## Artifacts

- `NPXS40142.decrypted.elf`: native GameLiveStreamingDaemon.
- `NPXS40100.decrypted.elf`: native AvCaptureManager.
- Both are x86-64 Sony OS-specific ELF type `0xfe10` with program headers but no section table.
- Offsets below are **file offsets in the decrypted files**, not virtual addresses or firmware-independent locations. Sony binary files are not committed.

## Daemon to AVCap2

- `NPXS40142` imports `sceAvcap2OpenVideo` by NID `4jwUgirLFrA`, dynamic symbol index 202, PLT relocation index 183, GOT virtual address `0x169fa8`.
- Code at ELF file offset `0x9b130` takes the address of this relocation slot for a callable wrapper. Looking only for a direct `call OpenVideo@plt` misses this wrapper path.
- The native daemon also requests ResourceArbitrator feature `200` with RUNNING/STOP/RUNNING_AUDIO_ONLY states; its use is not evidence of game-only source selection. The console A/B test refutes 200 as a standalone notification filter.

## AvCaptureManager video devices

| File offset | Class name returned |
| --- | --- |
| `0x19950` | `SceAvCaptureDeviceVideoScanIn` |
| `0x1a9a0` | `SceAvCaptureDeviceVideoNormal` |
| `0x1b270` | `SceAvCaptureDeviceVideoSecond` |

These are separate internal device implementations, not proven user-accessible mode choices. Code around `0x17ce9` handles a 0x2b8-byte request object; validation near `0x17be0` and dispatch near `0x17d80` are the next reverse-engineering targets. The `4` in the PlayBridge client request is identified as a profile/FPS selector by the available AVCap2 client layout, not an established source selector.

## Privacy encoder configuration

The manager has a diagnostic string `sceVencSetNormalPrivacyProtectionConfig` at file offset `0x291bd`. Associated call sequences appear around `0x18dc8`, `0x1ac58`, and `0x1b1fb` and construct a 24-byte configuration from the active device state. Internal routines near `0x19fe0` and `0x19ff0` derive a memory pointer and detect nonzero mask data.

This confirms a native video privacy configuration path is present, **not what it protects**. It could handle protected-content frames rather than PS5 notification overlays. No request flag or call sequence has yet been verified as safe and effective for notification removal.

## Next differentiating test

Record a short video with the PS5 built-in Create > Start New Recording feature while a notification is visibly shown on the TV. Check whether that notification appears in the **saved native clip**:

- **Absent from saved clip**: the shared AvCaptureManager game-recording pipeline likely excludes at least this overlay; tracing its device selection becomes higher priority.
- **Present in saved clip but absent from native Twitch stream**: filtering may be specific to the GameLiveStreaming daemon or broadcast setup.

This test does not require modifying the payload. If further inspection is required, compare the actual native OpenVideo request using authorized userland debugging before changing any opaque request bytes. Keep firmware 13.60 and the working 10.20 capture path untouched.
