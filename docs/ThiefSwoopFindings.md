# Thief swoop: recovered behavior

Static investigation, 2026-10-17. This records observed data and a bounded reconstruction; it is not a Thief playtest or a claim of exact feel.

The installed donor is `S:\Spiele\ThiefvWwV2\Binaries\Win64\Shipping-ThiefGame.exe`, image base `0x140000000`, SHA-256 `ee707ecd140dc88317b631de4eb0a6173754bdde8b5a15590d4c06e0471edbf9`. Its `ThiefGame.u` package has UE3 version 721/licensee 148 and SHA-256 `c061289fdc634484badc780379992b6daa1827998b114d4b861338367d87fa35`. No original was modified.

## Authored settings and binding

The bounded package reader resolves 29,278 names, 885 imports and 41,037 exports, checks table/property boundaries, and decompresses only selected LZO chunks 0, 1, 36 and 37. Its safe LZO decoder was compiled locally from inspected UEViewer source. Proprietary tables and extracted objects remain private.

`Default__ThiefHeroSettings.mDashSettings` references `Prog_GP_Dash.Settings_Dash`. The authored hero settings object has no additional Dash override in the inspected property stream. The authored Dash object overrides selected class defaults:

| Property | Class default | Authored override |
|---|---:|---:|
| Maximum speed | 600 | inherited |
| Maximum acceleration | 1000 | inherited |
| Stick threshold | 0.25 | inherited |
| Distance | 415 | **525** |
| Exhausted distance | 300 | inherited |
| End distance | 120 | inherited |
| Cooldown seconds | 0.5 | **0** |
| Duration seconds | 0.8 | **1.15** |
| Minimum collision angle | 30 | **45** |
| Minimum distance from collision | 150 | **100** |

The authored object also references `PROG_Cam_PostProcess.PostprocessAll_Swoop`. Distances and speeds above are in original game units, **not established meters**. A serialized cooldown of zero does not prove unrestricted retriggering: state, input and stamina can impose other restrictions. These values and their binding are static shipped data, not measurements of a running swoop.

Reproduce the selected data extraction with the locally built decoder:

```powershell
python tools/thief_inspect.py 'S:\Spiele\ThiefvWwV2\ThiefGame\CookedPCNG\ThiefGame.u' dash-settings --lzo-dll .local/native/thief/lzo/lzo_decoder.dll
```

## State and trajectory trace

All addresses below are RVAs for the pinned donor. Names are analytical roles unless explicitly serialized names.

| RVA | Evidence/role |
|---|---|
| `0x889540` | Dash settings class registration/getter, anchored by its UTF-16 class name |
| `0x1954150` | Active Dash state vtable, referenced by constructor `0x894400` |
| `0x89dc70` | Enter: select normal/exhausted distance, copy speed/acceleration settings, choose target, initialize trajectory |
| `0x88d8f0` | Per-frame active tick; float delta arrives in XMM1 |
| `0x887390` | Horizontal, view/yaw-relative direction normalization; retain previous direction for approximately zero input |
| `0x88d680` | Exit/cleanup, including swooping flag clearing |
| `0x8992a0` | Event handler; **not** the frame tick |
| `0x88c370` | State transition selection, including finished/stop flag |
| `0xac64e0` / `0xac5230` | Trajectory setup wrapper/manager-backed setup |
| `0xac54d0` | Accumulate elapsed time, evaluate position and velocity, release completed interpolation |
| `0xab97e0` | Retarget while preserving current evaluated position and velocity |
| `0xab66d0` / `0xab67a0` | Position/velocity evaluation wrappers |
| `0x972c0` / `0x24dc20` | Cubic Hermite position and its derivative callbacks |

The tick recomputes the remaining target direction, rebuilds/retargets the trajectory, advances it by simulation delta, clamps the requested velocity, and writes to the donor movement object. It does not simply translate the pawn a fixed amount each frame. It checks trajectory completion/direction and other state flags. Stamina consumption is linked to movement speed times delta, divided by the start-to-target distance, through `0xa17780`. Full cancellation, collision-angle policy, camera response and stamina semantics still need further tracing and gameplay measurements.

The Hermite basis is supported by both instruction flow and direct constant reads from the original PE: coefficients `2, -2, 3, 1` in position, and `6, 3, 2, 4, -6` in the derivative. The position wrapper scales endpoint velocities by duration; the derivative wrapper divides the result by duration. This establishes a cubic Hermite primitive, rather than guessing an easing curve from footage.

Our independent implementation in `native/SwoopCore/Trajectory.h` uses caller-supplied units and endpoint conditions. It tests endpoint position/velocity, analytic derivative, continuous steering splice, 30/60/120 Hz integrated deltas, and invalid/time-gap refusal. It is **not connected to KCD2**, does not implement all donor constraints, and does not establish the donor's actual travel or unit conversion. Host tuning and endpoint velocities remain explicit integration decisions.

## Evidence and unresolved work

Private original-PE disassembly: `.local/native/thief/`. Package receipt: `dash-settings-verified.json`. Focused REA tick evidence: `ev_35f65a830a6c96c55657f90fe09dc32f4bb6f5261fee86f7bc624f1d5071defc`; Hermite callback: `ev_9ba1abea20df239e0d04b722e7c4654d69be4d0552d5b63a187752d4bc7f40f2`. These REA subjects are derived bounded analysis containers; see [setup limitations](REASetup.md).

Still required: prove runtime settings selection, establish world scale, measure normal/exhausted travel and timing, resolve exact collision stop/slide behavior and cancellation ordering, and compare steering/camera/stamina in the original game. No extracted proprietary movement code or assets are shipped with our mod.
