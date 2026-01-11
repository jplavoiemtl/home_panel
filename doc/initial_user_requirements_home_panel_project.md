# Home Panel Project – Initial User Requirements

## 1. Project Overview

> **Note for Claude Code**  
> This document is the authoritative source of requirements and constraints for the Home Panel project.  
> Claude Code must use this file to generate a **separate project plan document** named:
>
> **`home_panel_plan.md`**
>
> The generated plan should:
> - Be derived strictly from the requirements defined in this document
> - Propose clear phases, milestones, and deliverables
> - Stay within the defined scope and exclusions
> - Avoid writing implementation code (planning only)
> - Assume the Companion project is reference-only and temporary
> - Treat this document as stable input unless explicitly updated


The goal of the **Home Panel** project is to create a new, clean, and maintainable code base for an ESP32-based Home Panel by **reusing and adapting proven functionality** from an existing and fully tested **Companion project**.

Both projects target the **ESP32-S3 MCU**, but they differ in:
- Hardware module
- Display and touch controller
- Power and sensor capabilities

The Companion project serves strictly as a **reference implementation**. Its code will not be compiled or shipped as part of the Home Panel. Instead, selected modules, architecture patterns, and logic will be reused and adapted to form the foundation of the new Home Panel firmware.

The current Home Panel firmware is a **minimal, hardware-validation project**. It compiles and runs correctly on the new hardware and includes updated SquareLine Studio UI files specific to the new display and touch driver.

---

## 2. Reference Code Base (Companion Project)

The folder `companion_reference_code/` contains the complete, working Companion project. It exists only to:
- Understand the existing architecture
- Reuse stable and tested logic
- Port relevant modules to the Home Panel project

Key characteristics of the Companion project:
- Fully tested and stable
- Designed for a different hardware module
- Uses the same ESP32-S3 MCU
- Includes battery management, IMU, motion detection, and multiple LVGL screens

This folder:
- **Must not be tracked in Git**
- **Will eventually be removed** once the Home Panel transition is complete

---

## 3. Home Panel Scope and Constraints

### 3.1 Hardware Constraints

The Home Panel hardware differs from the Companion device in the following ways:

**Not present on Home Panel:**
- Battery and battery management circuitry
- IMU (accelerometer / gyroscope)
- Motion detection capabilities

**Always present:**
- USB-C power (always powered)

### 3.2 Features Explicitly Excluded

The following Companion features will **not** be ported:
- Battery management and indicators
- Sleep mode and shutdown logic
- IMU module
- G-meter functionality
- Inclinometer functionality
- IMU calibration screens
- Motion-based events

All related code, tasks, screens, and LVGL objects should be removed or excluded from the Home Panel code base.

---

## 4. Features to Reuse from Companion Project

The Home Panel will reuse the **core, proven functionality** from the Companion project, including:

### 4.1 Networking
- Wi-Fi connection management
- MQTT client setup and messaging
- Network initialization patterns and error handling

### 4.2 Image Handling
- HTTP image fetching
- Image display logic
- Button-driven image requests

### 4.3 MQTT Integration

The Home Panel will continue to use MQTT for:
- Displaying **power and energy data** on the Home screen
- Triggering HTTP requests to fetch the **latest camera image**

---

## 5. User Interface (LVGL)

### 5.1 UI Source of Truth

- The Home Panel UI is managed entirely using **SquareLine Studio**
- UI files generated for the Home Panel **must not be manually modified**
- Companion UI files exist only for reference and understanding

### 5.2 Screens in Current Phase

The Home Panel currently contains **two LVGL screens**:

1. **Home Screen**
   - Buttons to request and view camera images
   - Power and energy indicators (via MQTT)

2. **Image Display Screen**
   - Displays images retrieved via HTTP requests

### 5.3 LVGL Object Consistency

To minimize porting effort:
- Existing LVGL variable names, buttons, and screen object patterns from the Companion project should be reused where applicable
- All LVGL elements related to battery, motion, IMU, G-meter, inclinometer, and calibration must be removed

---

## 6. Initial Code Structure and Reused Files

The following files were copied from the Companion project into the Home Panel project and are expected to be reused and adapted:

### 6.1 Credentials and Secrets
- `secrets.h`
- `secrets_private.h`
- `secrets_private.cpp`
- `secrets_private.example.h`

### 6.2 Networking and Image Modules
- `src/image/image_fetcher.h`
- `src/image/image_fetcher.cpp`
- `src/net/net_module.h`
- `src/net/net_module.cpp`

### 6.3 Excluded Modules

The following types of files were intentionally **not copied**:
- IMU-related modules (e.g. `imu_module.cpp`)
- Battery management modules
- Power-saving and sleep-related modules

---

## 7. Git and Source Control Strategy

### 7.1 Git Setup

- Git is not yet fully configured
- A base `.gitignore` file already exists

### 7.2 Git Requirements

The following must be ensured:
- **No credential files** (real secrets) are tracked or pushed to GitHub
- `secrets_private.*` files must be excluded
- The entire `companion_reference_code/` folder must be excluded from version control

The `.gitignore` file must be reviewed and updated accordingly before publishing the repository.

---

## 8. Proposed Porting Strategy (Planning Only)

> **No code should be written at this stage.**

A suggested high-level approach:

1. **Architecture Review**
   - Analyze Companion project structure and initialization flow
   - Identify reusable core modules and dependencies

2. **Define a Minimal Home Panel Baseline**
   - Networking (Wi-Fi + MQTT)
   - Image fetching and display
   - Two-screen LVGL UI

3. **Module-by-Module Porting**
   - Port network and image modules first
   - Remove unused functionality during porting (not after)

4. **UI Integration**
   - Bind existing LVGL objects to reused logic
   - Ensure MQTT and HTTP interactions update the UI correctly

5. **Cleanup Phase**
   - Remove unused Companion references
   - Validate that no Companion-only logic remains

6. **Validation and Testing**
   - Confirm feature parity for the defined scope
   - Test stability on Home Panel hardware

---

## 9. Future Evolution (Out of Scope)

- Additional screens
- New buttons and features
- Extended Home Panel functionality

These will be defined and implemented in **later phases** once a stable base firmware is established.

---

## 10. Non-Goals and Assumptions

### 10.1 Non-Goals (Explicitly Out of Scope for This Phase)

The following items are **not goals** of the current Home Panel transition phase and must **not** be planned or implemented in `home_panel_plan.md`:

- Redesigning or reworking the Home Panel UI or UX
- Adding new Home Panel features beyond those listed in this document
- Refactoring Companion code for elegance, optimization, or abstraction
- Performance tuning beyond achieving functional parity
- Introducing new frameworks, libraries, or architectural patterns
- Adding OTA updates, provisioning flows, or advanced security mechanisms
- Supporting battery-powered operation or power-saving modes
- Reintroducing IMU, motion detection, or related visualizations

The objective is strictly to **reuse, strip down, and adapt** proven Companion functionality — not to innovate or expand scope.

---

### 10.2 Assumptions

The following assumptions should be treated as true and should **not** be questioned or redesigned during planning:

- The Companion project code is correct, stable, and trusted
- The Home Panel hardware has already been validated and is stable
- The ESP32-S3 toolchain and build environment are functional
- SquareLine Studio UI files for the Home Panel are final for this phase
- Wi-Fi infrastructure, MQTT broker, and HTTP endpoints already exist and are reachable
- The credential management pattern used in the Companion project is acceptable
- Only the features explicitly listed in this document are required for initial success

These assumptions are intended to keep the project plan focused and lightweight.

---

## 11. Open Points and Collaboration

- This document represents the **initial planning phase**
- Alternative approaches or simplifications are welcome
- Clarifying questions should be raised early if any assumptions are unclear

Once the Home Panel successfully replaces the needed Companion functionality and testing is complete, the `companion_reference_code/` folder can be safely removed from the project.

