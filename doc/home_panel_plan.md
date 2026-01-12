# Home Panel Project Plan

> **Document Purpose**
> This project plan is derived from `initial_user_requirements_home_panel_project.md` and defines the phases, milestones, deliverables, and success criteria for transitioning the Home Panel firmware from a minimal hardware-validation state to a fully functional application.

---

## 1. Executive Summary

The Home Panel project will create a clean, maintainable ESP32-S3 firmware by selectively porting proven functionality from the Companion reference project. The scope is deliberately constrained: reuse stable code, exclude hardware features not present on the Home Panel, and deliver a two-screen LVGL interface with networking, MQTT, and HTTP image capabilities.

**Target Outcome:** A stable Home Panel firmware with:

- Wi-Fi and MQTT connectivity
- HTTP-based image fetching and display
- Power/energy data display via MQTT
- Two functional LVGL screens (Home and Image Display)

---

## 2. Project Phases

### Phase 1: Foundation and Repository Setup

**Objective:** Establish a clean, properly configured repository and document the architecture.

#### Milestone 1.1: Git Configuration

- Review and update `.gitignore` to exclude:
  - `secrets_private.h`
  - `secrets_private.cpp`
  - `companion_reference_code/` folder
  - Any other credential or sensitive files
- Verify no secrets are currently tracked
- Confirm repository is ready for public/team access

#### Milestone 1.2: Architecture Documentation

- Create architecture documentation describing:
  - Project folder structure
  - Module responsibilities
  - Data flow between components
  - LVGL screen hierarchy
  - Build and flash process

#### Phase 1 Deliverables

- Updated `.gitignore`
- `doc/architecture.md` (or similar)
- Clean git history with no secrets

---

### Phase 2: Companion Project Analysis

**Objective:** Thoroughly understand the Companion project structure to inform porting decisions.

#### Milestone 2.1: Code Structure Analysis

- Document the Companion project's folder layout
- Identify initialization flow and startup sequence
- Map module dependencies (what depends on what)

#### Milestone 2.2: Module Classification

- Categorize all Companion modules into:
  - **Reuse:** Network, MQTT, Image fetching
  - **Exclude:** Battery, IMU, motion, sleep, G-meter, inclinometer
  - **Adapt:** Any modules requiring modification for Home Panel hardware

#### Milestone 2.3: LVGL Object Inventory

- List all LVGL objects, screens, and variables in Companion
- Identify objects that can be reused with same naming
- Flag objects to exclude (battery indicators, IMU screens, calibration UI)

#### Phase 2 Deliverables

- `doc/companion_analysis.md` summarizing findings
- Module dependency diagram (text or visual)
- LVGL object mapping table

---

### Phase 3: Network Module Porting

**Objective:** Port and validate Wi-Fi and MQTT connectivity.

#### Milestone 3.1: Wi-Fi Module Integration

- Adapt `net_module.h` and `net_module.cpp` for Home Panel
- Remove any battery-aware or sleep-related networking logic
- Implement connection management and error handling
- Validate Wi-Fi connection on Home Panel hardware

#### Milestone 3.2: MQTT Client Integration

- Port MQTT client setup from Companion
- Configure connection to existing MQTT broker
- Implement message subscription for power/energy topics
- Implement message handling callbacks
- Validate MQTT message reception

#### Phase 3 Deliverables

- Functional `src/net/net_module.h` and `.cpp`
- MQTT client operational and receiving messages
- Connection status visible in serial logs

---

### Phase 4: Image Fetcher Module Porting

**Objective:** Port HTTP image fetching capability.

#### Milestone 4.1: Image Fetcher Integration

- Adapt `image_fetcher.h` and `image_fetcher.cpp` for Home Panel
- Remove any dependencies on excluded modules
- Implement HTTP GET for image retrieval
- Handle image data buffering and error cases

#### Milestone 4.2: Image Display Logic

- Integrate image data with LVGL image display widget
- Ensure proper memory management for image buffers
- Validate image display on Home Panel screen

#### Phase 4 Deliverables

- Functional `src/image/image_fetcher.h` and `.cpp`
- Images successfully fetched and displayed
- No memory leaks during repeated image fetches

---

### Phase 5: LVGL UI Integration

**Objective:** Connect ported modules to LVGL screens and widgets.

#### Milestone 5.1: Home Screen Bindings

- Bind camera request buttons to image fetcher triggers
- Bind power/energy MQTT data to display labels/indicators
- Implement screen refresh logic for live data updates

#### Milestone 5.2: Image Display Screen Bindings

- Bind fetched image data to image display widget
- Implement screen navigation (Home ↔ Image Display)
- Handle image loading states and errors gracefully

#### Milestone 5.3: UI Cleanup

- Remove or disable any LVGL objects related to:
  - Battery indicators
  - IMU/motion displays
  - G-meter and inclinometer
  - Calibration screens
- Verify no orphaned event handlers or callbacks

#### Phase 5 Deliverables

- Fully functional Home Screen with live MQTT data
- Fully functional Image Display Screen
- Clean UI with no excluded-feature artifacts

---

### Phase 6: Cleanup and Consolidation

**Objective:** Remove all unused code and finalize the codebase.

#### Milestone 6.1: Code Removal

- Delete or exclude any remaining Companion-only logic
- Remove unused includes, defines, and declarations
- Ensure no references to battery, IMU, sleep, or motion remain

#### Milestone 6.2: Credential Management Review

- Verify `secrets.h` pattern works correctly
- Confirm `secrets_private.example.h` provides clear template
- Document credential setup for new developers

#### Milestone 6.3: Build Verification

- Ensure clean compilation with no warnings
- Verify binary size is appropriate
- Confirm no unused code remains linked

#### Phase 6 Deliverables

- Clean codebase with no excluded-feature code
- Documented credential setup process
- Warning-free build output

---

### Phase 7: Validation and Testing

**Objective:** Confirm all features work correctly on Home Panel hardware.

#### Milestone 7.1: Functional Testing

- [ ] Wi-Fi connects reliably on power-up
- [ ] MQTT client connects and receives messages
- [ ] Power/energy data displays correctly on Home Screen
- [ ] Camera image request buttons trigger HTTP fetch
- [ ] Images display correctly on Image Display Screen
- [ ] Screen navigation works bidirectionally
- [ ] System remains stable over extended operation

#### Milestone 7.2: Stress Testing

- Repeated image fetches (memory stability)
- MQTT reconnection after broker restart
- Wi-Fi reconnection after network interruption

#### Milestone 7.3: Final Validation

- Compare feature parity against requirements document
- Confirm all "Reuse" features are functional
- Confirm all "Exclude" features are absent

#### Phase 7 Deliverables

- Test results log
- Confirmation of feature parity
- Sign-off on Phase 1 completion

---

## 3. Module Dependency Order

The following order minimizes integration risk:

```
1. Git/Repository Setup (independent)
2. Architecture Documentation (independent)
3. Companion Analysis (independent)
4. Network Module (Wi-Fi) ← foundation for all connectivity
5. Network Module (MQTT) ← depends on Wi-Fi
6. Image Fetcher Module ← depends on Wi-Fi
7. LVGL Home Screen Bindings ← depends on MQTT
8. LVGL Image Screen Bindings ← depends on Image Fetcher
9. Cleanup ← depends on all above
10. Validation ← depends on all above
```

---

## 4. Files to Port and Adapt

| File | Action | Notes |
|------|--------|-------|
| `secrets.h` | Already present | Review for completeness |
| `secrets_private.h` | Already present | Excluded from git |
| `secrets_private.cpp` | Already present | Excluded from git |
| `secrets_private.example.h` | Already present | Template for developers |
| `src/net/net_module.h` | Adapt | Remove battery/sleep logic |
| `src/net/net_module.cpp` | Adapt | Remove battery/sleep logic |
| `src/image/image_fetcher.h` | Adapt | Remove excluded dependencies |
| `src/image/image_fetcher.cpp` | Adapt | Remove excluded dependencies |

---

## 5. Files/Modules to Exclude

The following must NOT be ported from Companion:

- IMU modules (`imu_module.*`)
- Battery management modules
- Power-saving/sleep modules
- G-meter functionality
- Inclinometer functionality
- Calibration screens
- Motion detection handlers

---

## 6. Success Criteria

The project is complete when:

1. **Connectivity:** Wi-Fi and MQTT connect reliably on power-up
2. **Data Display:** Power/energy MQTT data appears on Home Screen
3. **Image Fetching:** Button press triggers HTTP fetch and displays image
4. **UI Navigation:** Both screens are accessible and functional
5. **Code Cleanliness:** No excluded-feature code remains
6. **Repository Hygiene:** No secrets in git history; `.gitignore` correct
7. **Documentation:** Architecture documented; credential setup clear
8. **Stability:** System runs without crashes over extended periods

---

## 7. Risks and Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Hidden dependencies on excluded modules | Medium | Medium | Thorough Companion analysis in Phase 2 |
| LVGL object name mismatches | Low | Low | Reuse Companion naming conventions |
| Memory issues with image handling | Medium | High | Test with repeated fetches; monitor heap |
| MQTT/Wi-Fi reconnection failures | Low | Medium | Port proven reconnection logic from Companion |
| Secrets accidentally committed | Low | High | Pre-commit verification; .gitignore review |

---

## 8. Out of Scope (Explicit Exclusions)

Per the requirements document, the following are NOT part of this plan:

- UI/UX redesign
- New features beyond those listed
- Code refactoring for elegance
- Performance optimization beyond functionality
- New frameworks or architectural patterns
- OTA updates or provisioning
- Battery/power-saving support
- IMU/motion features

---

## 9. Post-Completion Actions

Once all success criteria are met:

1. Remove `companion_reference_code/` folder from the project
2. Archive this plan as completed
3. Begin planning for future evolution phases (new screens, features)

---

## 10. Document History

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | Initial | Created from initial_user_requirements_home_panel_project.md |
