# Review of #512

**Verdict:** Needs changes

The PR moves frame pacing to display vblank and passes a presentation deadline to the renderer, but the wait requests a timestamp one target period before the scheduled frame. The PR also introduces a drop-counting condition that can add zero for a missed frame and a clamp test whose exact-equality assertion fails by construction. The shutdown log needs a defined frame total.

## Must fix

### issue (blocking): The wait targets an earlier vblank

`src/render/frame_pacer.cpp:36`

The new wait subtracts a full target period from the scheduled start. At 60 Hz on a 144 Hz display, the second call requests a timestamp at or before the next available vblank even though the scheduled start rounds up to the third refresh after the first frame. Wait on the scheduled start and extend the startup-only test at `tests/render/frame_pacer_tests.cpp:17-19` to check successive frame starts.

```cpp
SwapChain.wait_until(NextFrame - FramePeriod);
```

```suggestion
		SwapChain.wait_until(NextFrame);
```

### issue (blocking): The clamp test compares a rounded rate to an exact value

`tests/render/frame_pacer_tests.cpp:40`

The new test expects exactly `60.0`, but the pacer rounds the period to `16,666,667 ns` and converts it back to about `59.9999988 Hz`. Use a narrow tolerance that covers nanosecond quantization and still catches failure to clamp.

```cpp
CHECK(pacer.target_rate() == 60.0);
```

## Worth a look

### issue (non-blocking): The drop counter can detect a miss and add zero

`src/render/frame_pacer.cpp:30-32`

The PR lowers the threshold from a full target period to half a period, but the integer quotient still truncates. At a 60 Hz target on a 144 Hz display, a vblank two refreshes past `NextFrame` enters the branch, adds zero, and then advances the schedule past the missed start. Count missed scheduled frame slots consistently and add a test at mismatched target and display rates.

```cpp
if (vblank > NextFrame + FramePeriod / 2) {
	// The display moved on without us: count every refresh we missed.
	FramesDropped += static_cast<std::uint64_t>((vblank - NextFrame) / FramePeriod);
```

### issue (non-blocking): The shutdown log has no frame count maintained by the loop

`src/app/game_loop.cpp:15-17`

The PR adds `FrameCount` to the shutdown log, but the loop submits frames without updating that count. Track submissions in the loop and label the total as submitted frames, or retain the original drop-only log.

```cpp
	SwapChain.present();
}
Log.info("dropped {} of {} frames", pacer.frames_dropped(), FrameCount);
```

## Not reviewed

- Native refresh queries, vblank wait behavior, and the latency setting after removal of `VSync`. The Git tree contains only the six changed files and omits swap-chain implementations and configuration callers.
- `World.update` argument units, the renderer’s deadline handling, and the declaration or initialization of `FrameCount`. The corresponding headers and implementations are absent.
- Test execution and the reported 144 Hz soak result. The workspace lacks the build configuration, test doubles, hardware, and soak output.

```bpr-findings
{"findings":[
  {"path":"src/render/frame_pacer.cpp","startLine":36,"endLine":36,"severity":"major","category":"correctness","confidence":"high","claim":"The new wait targets a vblank earlier than the scheduled frame start.","evidence":"The second call at a 60 Hz target requests a timestamp one target period before the scheduled start, over two refresh intervals on a 144 Hz display.","suggestedComment":"The wait can release the second frame before its scheduled refresh. Pass `NextFrame` to `wait_until` and assert the vblank reached by successive calls at `60 Hz` on a `144 Hz` display."},
  {"path":"tests/render/frame_pacer_tests.cpp","startLine":40,"endLine":40,"severity":"major","category":"correctness","confidence":"high","claim":"The new clamp test expects an exact rate that the pacer cannot return from its rounded period.","evidence":"A clamped 60 Hz rate produces a 16,666,667 ns period, and dividing 1e9 by that period gives approximately 59.9999988.","suggestedComment":"Nanosecond quantization makes this exact comparison fail after a successful clamp. Use an approximate comparison with an absolute tolerance sized for the period rounding."},
  {"path":"src/render/frame_pacer.cpp","startLine":30,"endLine":32,"severity":"minor","category":"correctness","confidence":"high","claim":"The new half-period threshold can detect a missed frame without incrementing the drop counter.","evidence":"A delay greater than half a period and less than a full period enters the branch, truncates to zero in the duration division, and is discarded when NextFrame advances.","suggestedComment":"The drop count loses a missed start when the scheduler advances past it. Count skipped target slots consistently and cover a mixed-rate case that crosses the half-period threshold."},
  {"path":"src/app/game_loop.cpp","startLine":15,"endLine":17,"severity":"minor","category":"correctness","confidence":"high","claim":"The PR logs a frame total that the game loop never updates.","evidence":"The loop calls SwapChain.present() on each iteration and then logs FrameCount on exit without incrementing it in the loop.","suggestedComment":"The shutdown message has no count tied to the loop’s submissions. Increment a local counter after `SwapChain.present()` and identify it as submitted frames, or keep the drop-only message."}
]}
```