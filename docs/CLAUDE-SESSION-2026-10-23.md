# Claude session, 2026-10-23 night (after Codex's usage limit at 22:01)

Source root: D:\Call of Duty 3 (USA, Europe)\CoD3Recomp-main\CoD3Recomp-main
Game: D:\Call of Duty 3 (USA, Europe)\CoD3-PC-Port-0.4.0
Backups of every file before it was changed, and each test's log:
C:\Users\hovgt\Documents\Codex\2026-10-22\you-re-picking-up-my-call\work\claude-410

## How the loop ran

Source and docs written first; then test-request.txt (a BENCH-*.bat name),
then build-request.txt. AUTO-BUILD.ps1 builds when the game is closed
(about 3 minutes) and BUILD-MULTIPLAYER.ps1 starts the test at the end.
The next build can be queued while a test runs (it waits for the game to
close), but its sources must not be written while a build is compiling.

## The benchmark camera (0.5.410-0.5.412)

- setviewpos places the host but does not turn him (0.5.410: resent every
  second for a whole run, yaw stayed 90).
- COD3_BENCH_CLIENTTURN (the local client's view angles) does not turn
  him either (0.5.412: the client's yaw grew to 5760 while his stayed
  -180).
- The facing differs between runs and can change within one (0.5.411:
  -180, then 163.7). So: compare runs by draw-count bins
  (tools: summ.py logic in NEXT-SESSION), and by profiler shares.
- What does turn him is still unknown: something server-side sets his
  angles (a spawn's?). Not needed for the speed work.

## Builds tonight

| Build | Change | Switch back | Result |
|---|---|---|---|
| 0.5.410 | Benchmark: setviewpos re-sent when the facing drifts; "bench: hold check" per view | COD3_BENCH_HOLDTURN=0 | Clean. Resend did not turn him (yaw 90 all run). Baseline 5000-5500 draws: 54.75 fps, 3.35 us/draw (n=72) |
| 0.5.411 | Texture lookup front cache (render_resources.cpp TextureForUnchecked); resend off by default | COD3_TEXTURELOOKUPCACHE=0; COD3_BENCH_HOLDTURN=1 | Clean. 4000-4500 draws 3.43 us/draw, 54.2 fps vs 0.5.408's 3.65 us, 53.2 fps at a similar facing. Lookup share of the packet loop 7.8% -> 6.2% |
| 0.5.412 | CheckView asks GetDesc once per view (render_commands.cpp) | COD3_VIEWCHECKALL=1 | Clean. D3D11CoreRegisterLayers (GetDesc) 0.8-0.9% -> 0 of the command thread |
| 0.5.413 | Texture/vertex/index tables in fixed blocks of 256 instead of std::deque (render_resources.cpp StableArray) | restore render_resources.before413.cpp | Clean. Same facing as 0.5.410 (matched): busy views command thread 17.0 -> 16.2 ms (-5%), view 3 -8%. FPS unchanged: the main thread (17.4 ms/frame) now holds the frame |
| 0.5.414 | Shader constants copied 16 bytes at a time instead of memcpy per run (render_commands.cpp Constants) | COD3_CONSTLIBCOPY=1 | Clean. Facing -180 like 0.5.412: busy view command thread 15.3 -> 14.2 ms (-8% with 0.5.413); memcpy 3.8% -> 2.3% |
| 0.5.415 | NativeCull::Walk stack as a plain array (mp_fixes.cpp) - main thread | COD3_CULL_VECTORSTACK=1 | Clean. Facing -180 like 0.5.412/414: fps v1 59.3->60.7, v2 66.4->68.0, v3 (busy) 52.2->60.0, v4 62.2->66.5; main CPU v3 18.7->16.0 ms; walk 4.9% -> 3.0% of main |
| 0.5.416 | Bots' grenade check remembers each weapon's kind (bots.cpp NadeKindOf) - main thread | COD3_NADEKINDCACHE=0 | Clean (read 23:40). BotNadeAmmo/strstr gone from the profile; fps views 1-3 63.7/65.4/57.4 vs 415's 60.9/68.0/60.3 at similar draws (match noise; view 4 faced elsewhere) |
| 0.5.417 | Native sub_824851F8 (slab test) and sub_824850D0 (piece sort) of the world trace, with 3000-call self-checks (mp_fixes.cpp NativeTrace) | COD3_NATIVE_SLAB=0, COD3_NATIVE_SORT=0 | Built together with the sampler cache below as 0.5.417 (23:45 session); offline 6M segments + 1500 lists identical (work/claude-410/trace-native-harness.cpp). Test: BENCH-MARSEILLE-CITY; check "trace native:" lines |
| (0.5.417) | Sampler handle front cache (render_pipeline.cpp SamplerFor); folded into 0.5.417, no separate 0.5.418 | COD3_SAMPLERFRONT=0; restore work/claude-410/render_pipeline.before417.cpp | In 0.5.417 |
| 0.5.417 result | | | Clean: trace native 3000/3000 both; slab 0.7-1.1%, sort 0.8-1.0% of main (were 1.8/1.6). Camera looked down (pitch 38), fps not comparable |
| 0.5.418 | Killcam freeze: scene-list tail check on every add, not only in the replay (bots.cpp SceneAddGuard). 416's 2-player test froze the frame a killcam ended, main thread looping in 8243E198 on instance 5F586190 | COD3_SCENETAILGUARD=0 | Queued with BENCH-MARSEILLE-CITY; real proof needs a killcam in a 2-player game. Why the double add happens: still open |
| 0.5.418 result | | | Clean (city bench, no killcam in it). Yaw 90: 56.2/56.6/61.4/69.2 fps at 4528/4899/3753/1899 draws |
| 0.5.419 | Native world-trace tree walk sub_824A0940 (mp_fixes.cpp NativeWalk), checked by record-and-replay against the game's own walk (leaf tests never run twice) | COD3_NATIVE_WALK=0; before: work/claude-410/next/mp_fixes-417.cpp | Clean: 3000/3000 walks matched (233 calls in the last). Walk 2.31% -> 1.63% of main. Yaw 0 this run: 71.6/75.5/75.5/66.9 fps, not comparable |
| 0.5.420 | Native segment-box test sub_82490A30 (mp_fixes.cpp NativeBox), pure; 5000-call compare | COD3_NATIVE_BOX=0; before: work/claude-410/next/mp_fixes-419.cpp | Clean: box 5000/5000, walk 3000/3000, slab/sort 3000/3000. Box test 0.86% -> under 0.1% of main. Yaw -180 (like 0.5.415): 63.4/69.4/58.2/68.4 fps at 4260/2897/4242/2280 draws |
| 0.5.421 | Native box sweep sub_8248BB48 (mp_fixes.cpp NativeSweep); 5000-call compare of r3 + the 48 bytes written | COD3_NATIVE_SWEEP=0; before: work/claude-410/next/mp_fixes-420.cpp | Clean: all five native checks passed. Sweep 1.1% -> 0.09% of main. Yaw 90 like 0.5.418: main CPU a frame 13.8/13.6/13.5/15.9 ms vs 14.3/13.3/15.5/17.6; fps 57.6/55.6/61.7/71.1 at 4920/4791/3710/1873 draws |
| 0.5.422 | Native packed-vector unpack sub_82181520 (mp_fixes.cpp NativeUnpack); dry-run stores compared with the game's real run | COD3_NATIVE_UNPACK=0; before: work/claude-410/next/mp_fixes-421.cpp | Clean: unpack 3000/3000 (27 stores in the last), the other five checks passed. Unpack 1.65% -> 0.60% of main. Yaw 63.5 / pitch 7 (no earlier run at this facing): main CPU 14.2/14.7/15.4/17.1 ms; 58.4/57.8/63.6/66.1 fps at 4413/4450/2876/3269 draws |
| 0.5.423 | Native table search by 32-byte key sub_820B64D0 (mp_fixes.cpp NativeTableSearch); 3000-call compare | COD3_NATIVE_TABLESEARCH=0; before: work/claude-410/next/mp_fixes-422.cpp | Clean: all seven checks passed. Search 1.10% -> 0.02% of main. Yaw 90 like 0.5.421: main CPU 12.7/12.8/13.9/16.3 ms vs 13.8/13.6/13.5/15.9; fps 58.2/56.3/63.3/75.0 vs 57.6/55.6/61.7/71.1 at about the same draws. NOTE: the bench .bat's copy to bench-ordnance-city.log did not happen this run; read CoD3MP.log |
| 0.5.424 | pshufb byte swaps (gpu.cpp WriteRegisterRunFrom, render_commands.cpp Swap16Into/Swap32Into) + native rotate sub_82147008 (mp_fixes.cpp NativeRotate) | COD3_NATIVE_ROTATE=0; before: work/claude-410/gpu.before424.cpp, render_commands.before424.cpp, next/mp_fixes-423.cpp | Clean: all eight checks passed. WriteRegisterRunFrom 6.5% -> 4.4% of the command thread. New facing yaw -115: view 3 the heaviest scene yet, 5740 draws, 16080 texture switches: 45.6 fps, command thread 19.6-22 ms (3.4-3.9 us/draw), drawing thread the same |

## What limits Marseille now

In the busy views (5000 draws) after 0.5.413:
- command thread 16.2 ms a frame (was 17.0 at 0.5.410)
- game main thread 17.4 ms of CPU a frame: server + bots 6.9, client and its
  drawing 11.5. This is now the limit (54.5 fps either way).
- The main thread's own top: NativeCull::Walk 3.3-3.5%, memcpy 1.3% (callers
  include Killcam::Record), vector push_back 1.1% (the walk's stack, 0.5.415),
  BotNadeAmmo + strstr ~1.5% (0.5.416). The rest is the game's own code
  (sub_82186740, 4200 lines of vector math, 2.1%; sub_824A0940 2.0%;
  sub_82164160 / sub_8215CE50 chains of virtual calls). Big main-thread wins
  now need native versions of game functions, verified against the
  originals as NativeCull is.

## Notes for the next agent

- summ.py (Codex work/cod3-407) or the per-bin summary: compare runs at the
  same draw counts; the facing is not under our control.
- Each test's log is in work/claude-410 as bench-city-4xx.log.
- The recompiled game code (CoD3RecompLib/ppc_mp) is generated from the
  game: never upload it.

## Next main-thread targets (the world trace)

After the walk, the trace is the main thread's busiest host-replaceable
work: sub_824A0940 (the recursive tree walk, 2.3%, ~360 lines), its leaf
tests sub_82499F58 (~520 lines) and sub_8249DDC8 (~840), the plane from a
triangle sub_8248C708 (1.0%; VMX with flush-mode switches each call, so a
native one must keep the vector ops' denormal flushing), sub_8248BB48
(1.2%), sub_82490A30 (0.9%). Pattern to follow: NativeTrace in mp_fixes.cpp
(exact rounding, offline harness against a transcription, in-game
3000-call check).

## Where it stands (2026-10-24 01:25, 0.5.424)

- Main thread: 12.7-14 ms a frame in Marseille's city views at 32
  players (was 17.4 at 0.5.410); fights near the camera push it to
  16-17. Its busiest pieces are now each about 1% or less (the world
  search NativeCull::Walk 3.3%, already native; a loop of virtual calls
  sub_82164160 1.7%; the game's own vector math). More native pieces
  are worth 1% each at most.
- Command thread and drawing thread: about 3.1-3.4 us a draw each.
  60 fps holds up to about 5000 draws; the heaviest scene seen (5740
  draws, 0.5.424's view 3) is about 45 fps with both threads at 20-22
  ms. The drawing thread is mostly Direct3D 11's own cost a draw. Not a
  render-scale or GPU problem: the GPU time measured is the GPU waiting
  for them.
- Without profilers (0.5.423, BENCH-MARSEILLE-CITY-PLAIN, yaw 0): 81 fps
  in the light views, 68.5 at 3928 draws.
- What would move the heavy views further is structural, not another
  small native piece: the command thread's translation split over two
  threads, Direct3D 11 command lists on more than one drawing thread, or
  fewer draws / texture binds a frame (16000 binds at 5740 draws). Each
  is a project of its own.
- Still open: 0.5.418's killcam fix wants a two-player game
  ("killcam guard: ... killcam not playing" in both logs); the friend's
  GPU slowdown (0.5.416) has no cause in the log.

## 0.5.425 (2026-10-24 17:10): co-op v38 merged

- coop.cpp = Team Friend's v38 diff applied on our v36 + 0.5.316 mortar
  probe (kept); their SHARED-LOG entry inserted at the top of ours; notes
  in docs/COOP-V38-NOTES.txt, diff in docs/build-logs/. Campaign only:
  CoD3MP.exe is the same as 0.5.424 but for the version.
- First city benchmark CRASHED (work/claude-410/bench-city-425.log): a
  72 s stall at startup (frames 61 -> 62, "watchdog: no swap ... after
  16"), "freed memory read at 0xA0D90004" in the vertical blank's
  handler chain (sub_825A1430 -> sub_825A15F4 -> sub_825A17E4), guest
  writes running on into unallocated memory (942 MB committed on demand),
  then C0000005 in VideoThread (kernel_video.cpp:604, the call to the
  title's interrupt callback) while ordnance loaded.
- The same startup stall + freed read appeared in 0.5.422 (36 s) and
  0.5.423 (23 s) and those runs recovered. Rerun of 0.5.425: clean, all
  eight native checks passed (bench-city-425-rerun.log). OPEN: an
  intermittent startup race between the vertical blank interrupt and the
  title's setup (memory 0xA0D90000 given by 8231430C, freed by 822C1AC8,
  still read by the handler).
