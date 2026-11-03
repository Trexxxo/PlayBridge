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

**Factory trace:** At ELF file offset `0x17ab0`, the manager switches on the 32-bit request field at `+0x10` and chooses one of three device constructors. At `0x17b22` (value 0) it constructs the ScanIn device (`0x18920`, vtable `0x29378`); at `0x17b5e` (value 1) it constructs VideoNormal (`0x1a5e0`, vtable `0x294e0`); at `0x17b9a` (value 2) it constructs VideoSecond (`0x1a9b0`, vtable `0x29628`). These vtable-to-name mappings were checked against ELF RELA entries for the name-return methods above. The `0x2b8` request constructor at `0x17cd0` and IPC caller around `0x4c83`/`0x4d9a` connect the factory to AVCap2 request handling. PlayBridge's 10.20 request leaves `+0x10` zero, so **VideoScanIn is the default candidate**.

**New opt-in hypothesis:** A build using `--video-normal-probe` sets only the 32-bit `+0x10` request field to `1`, preserving resource 12 and every other field. This should exercise VideoNormal rather than VideoScanIn if the request object maps directly to the factory input as traced. Native PS5 saved clips were observed to exclude notifications while they remain visible on TV; however, that fact does **not** yet prove native recording uses VideoNormal. This is a test, not a guaranteed fix. Check `VideoNormal probe`, `OpenVideo`, `Start`, and the actual stream image. If it fails, return to the baseline ELF. The `4` field remains a profile/FPS selector and is not changed.

## Privacy encoder configuration

The manager has a diagnostic string `sceVencSetNormalPrivacyProtectionConfig` at file offset `0x291bd`. Associated call sequences appear around `0x18dc8`, `0x1ac58`, and `0x1b1fb` and construct a 24-byte configuration from the active device state. Internal routines near `0x19fe0` and `0x19ff0` derive a memory pointer and detect nonzero mask data.

This confirms a native video privacy configuration path is present, **not what it protects**. It could handle protected-content frames rather than PS5 notification overlays. No request flag or call sequence has yet been verified as safe and effective for notification removal.

## Next differentiating test

**Observed hardware result (user, twice):** A notification visible on the TV did **not** appear in a clip saved using PS5 Create > Start New Recording. This confirms a game-only/native recording behavior worth matching. Next test the isolated VideoNormal selector probe; no other firmware, mode, or ResourceArbitrator field is changed.

- **Absent from saved clip**: the shared AvCaptureManager game-recording pipeline likely excludes at least this overlay; tracing its device selection becomes higher priority.
- **Present in saved clip but absent from native Twitch stream**: filtering may be specific to the GameLiveStreaming daemon or broadcast setup.

This test does not require modifying the payload. If further inspection is required, compare the actual native OpenVideo request using authorized userland debugging before changing any opaque request bytes. Keep firmware 13.60 and the working 10.20 capture path untouched.
