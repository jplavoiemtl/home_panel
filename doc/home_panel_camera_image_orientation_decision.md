# Home Panel – Camera Image Orientation Decision

## Context

While working on image display for the ESP32 Home Panel, I have encountered an issue similar to one previously solved in the *Companion Reference Module*. This document is intended to clearly describe the situation and request a recommended course of action so it can be used by Claude Code to guide the next project steps.

## Background: Companion Reference Module

On the Companion Reference Module, displaying camera images correctly required several iterations and fine-tuning. The final working solution involved **rotating camera images by 90 degrees on the backend** before sending them to the device.

- The camera naturally produced images in a **landscape orientation**.
- To match the display constraints and behavior of the companion hardware, images were **pre-rotated to portrait mode** on the backend.
- This approach was not ideal, but it was the *only reliable solution* found at the time.
- Because the images were already rotated, the companion firmware had to implement **additional screen and coordinate transformations**, increasing code complexity.
- Despite these drawbacks, the solution proved stable and functional on the companion hardware.

## Transition to the Home Panel

For the Home Panel project:

- I will generate new UI files using **SquareLine Studio**.
- These files are intended to *replace* the interim UI assets created during the early transition phase.
- The Home Panel uses **new hardware**, which may behave differently from the companion module in terms of display orientation, memory, and rendering.

This creates an opportunity to reconsider the image preparation strategy.

## Two Possible Approaches

### Option 1 – Reuse the Rotated Image Strategy (Known Working Solution)

- Continue generating camera images on the backend with a **90-degree rotation** (portrait orientation).
- Leverage a strategy that is already proven to work on the companion module.
- Likely requires similar screen-rotation logic in the Home Panel firmware.

**Pros:**
- Known, tested, and reliable approach.
- Lower risk of unexpected rendering issues.
- Faster path to a working result.

**Cons:**
- Not an ideal image pipeline.
- Additional complexity in firmware (screen rotation, coordinate handling).
- Locks the Home Panel into a workaround driven by older hardware constraints.

### Option 2 – Use Native Landscape Images (Ideal Architecture)

- Generate **properly oriented landscape images** on the backend, without rotation.
- Resize images normally while preserving the camera’s natural orientation.
- Attempt to display them correctly using the Home Panel’s hardware and LVGL configuration.

**Pros:**
- Clean and intuitive image pipeline.
- Simplifies backend image generation.
- Reduces display and transformation complexity in firmware.
- Results in correctly oriented, visually natural images.

**Cons:**
- Unproven on current ESP32 + LVGL setup.
- No existing working reference from the companion module.
- Higher uncertainty and potential debugging effort.

## Open Question / Decision Request

Given the above, the key question is:

> **Should the Home Panel continue using the rotated-image approach that worked on the Companion Module, or should we attempt the cleaner, ideal solution using native landscape images on the new hardware?**

A recommendation is requested that balances:
- Technical risk
- Development effort
- Long-term maintainability
- Alignment with the new Home Panel hardware capabilities

## Next Steps (Dependent on Decision)

Once a direction is chosen:

- I will proceed with the **SquareLine Studio UI design** based on the selected image orientation.
- The **backend image generation pipeline** will be updated accordingly.
- Firmware display logic will be aligned with the chosen strategy.

---

## Decision Outcome

**Date:** 2026-01-12

**Decision:** Option 2 – Use Native Landscape Images

### Rationale

1. **Clean architecture**: The Home Panel's logical display is 480×320 landscape. Native landscape images fit directly without rotation workarounds.

2. **Fresh hardware, fresh approach**: The Home Panel should not inherit constraints from the Companion module's specific hardware limitations.

3. **Simpler firmware**: No coordinate transformations or screen rotation logic required during image display.

4. **Better maintainability**: Future developers won't need to understand legacy rotation workarounds.

5. **Current testing confirmed the path**: Initial tests with companion-sized images (368×448 portrait) showed visual corruption due to resolution/stride mismatch, confirming that properly sized landscape images are needed.

### Implementation Plan

1. **Backend Changes:**
   - Create new HTTP endpoints specific to Home Panel (separate from Companion endpoints)
   - Generate camera images as **480×320 landscape JPEG** (no rotation)
   - Companion module continues using existing rotated-image endpoints

2. **UI Development:**
   - Design new UI in SquareLine Studio for 480×320 landscape orientation
   - Replace placeholder UI files with SquareLine Studio generated files

3. **Firmware:**
   - No changes required to image decoding logic
   - Current implementation already expects 480×320 landscape images

### Endpoint Strategy

| Module | Endpoints | Image Format |
|--------|-----------|--------------|
| Companion | Existing endpoints | 368×448 portrait (rotated) |
| Home Panel | New dedicated endpoints | 480×320 landscape (native) |

This separation ensures both modules can coexist without impacting each other.

---

**Decision recorded and implementation proceeding.**

