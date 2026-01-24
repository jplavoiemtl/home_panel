# Feature Proposal: Cycling Temperature Display by Location (ESP32 Home Panel)

## Overview

This feature adds support for displaying temperature data from **multiple locations** on a **limited screen area** by cycling through locations with a **single button press**.

Each button press advances to the next configured location and updates the UI to show:
- Location name
- Latest temperature
- Time of the latest sample (hh:mm)

Temperature data is received via **MQTT**, and the currently selected location is persisted across reboots using **ESP32 NVS**.

Claude Code: please analyze this feature and propose the most robust and clean implementation, including data structures and logic flow.

---

## Functional Goals

- Display temperatures for **multiple locations** using a single UI area
- Cycle through locations with each press of a button
- Store the **currently selected location** in NVS so it is restored at boot
- Avoid excessive writes to NVS by debouncing saves
- Show **live data only** (no historical persistence of temperature samples)

---

## MQTT Data Source

All temperature data is received on the same MQTT topic:

```cpp
msg.topic = "weather";
```

Payload examples (one key per message):

```cpp
msg.payload = {
    OutsideTemp: temp
}
```

```cpp
msg.payload = {
    AmbientTemp: temp
}
```

Each payload contains:
- A **location identifier** (key)
- A **temperature value**

The Home Panel calculates the **time of reception** using its internal clock, synchronized via NTP.

---

## Time Handling

- Time format: `hh:mm`
- Seconds are intentionally omitted to save screen space
- Time is calculated locally when the MQTT message is received

---

## UI Components (SquareLine Studio)

### Button
- **ButtonTempLocation**
- Event handler:
```cpp
buttonTempLocation_event_handler()
```
- Triggered on each press to cycle to the next location

### Labels
- `tempLocLabel` → Displays the location name (e.g. "Outside")
- `tempLabel` → Displays the temperature (e.g. "-21.6 °C")
- `tempTimeLabel` → Displays the sample time (e.g. "14:52")

---

## Startup Behavior

- On boot:
  - Load the **last selected location** from NVS
  - Display the location name immediately
- Until MQTT data is received:
  - Temperature and time may be shown as:
    - `--`
    - `-`
    - or blank

Claude Code: please recommend the **most professional UI behavior** here.

---

## NVS Persistence Strategy

- Only the **currently selected location index or ID** is stored
- Temperature and time are **NOT persisted**
- NVS write debounce: **30 seconds**
  - Prevents excessive flash wear
  - Save only after the location has remained unchanged for 30 seconds

---

## Example Data

| Location | Temperature | Time  |
|--------|------------|-------|
| Kitchen | 20.2 °C | 04:26 |
| Ambient | 21.3 °C | 22:03 |
| Outside | -15.8 °C | 14:57 |

---

## Data Structure Design

Each temperature data point must logically bind:
- Location
- Temperature
- Time of last update

The structure must support:
- Fast lookup when MQTT data arrives
- Simple cycling order
- Low memory usage
- Clean UI binding

---

## Reference C++ Data Model (Proposal)

> This section provides a **reference implementation** to guide Claude Code.
> Claude may refine or optimize this proposal if needed.

### Location Enumeration

Use an enum to define known locations and enforce ordering for cycling.

```cpp
enum class TempLocation : uint8_t {
    Kitchen = 0,
    Ambient,
    Outside,
    COUNT
};
```

- `COUNT` is used to determine array size and wrap-around logic
- Enum values double as array indices

---

### Temperature Sample Structure

```cpp
struct TempSample {
    TempLocation location;
    float temperatureC;
    char timeHHMM[6];   // "hh:mm" + null terminator
    bool valid;         // false until first MQTT update
};
```

- `valid` indicates whether live data has been received
- Time is stored as a small fixed-size string to avoid dynamic allocation

---

### Global Storage Container

```cpp
TempSample tempSamples[static_cast<uint8_t>(TempLocation::COUNT)];
```

This allows:
- O(1) access when updating data
- Easy cycling using a simple index
- Minimal RAM overhead

---

### Location Metadata Mapping

Optional helper table for UI labels and MQTT key matching:

```cpp
struct LocationMeta {
    TempLocation location;
    const char* label;
    const char* mqttKey;
};

const LocationMeta locationMap[] = {
    { TempLocation::Kitchen, "Kitchen", "KitchenTemp" },
    { TempLocation::Ambient, "Ambient", "AmbientTemp" },
    { TempLocation::Outside, "Outside", "OutsideTemp" }
};
```

This avoids hard-coded strings scattered throughout the codebase.

---

### Selected Location State

```cpp
TempLocation currentLocation;
```

- Persisted to NVS (after debounce)
- Restored at boot

---

## UI Update Logic

When temperature data is received:
1. Match MQTT payload key to a `TempLocation`
2. Update the corresponding `TempSample`
3. Store temperature and formatted time
4. Mark sample as `valid = true`
5. If this location is currently selected:
   - Update `tempLocLabel`
   - Update `tempLabel`
   - Update `tempTimeLabel`

---

## Button Press Logic

On each `ButtonTempLocation` press:
1. Increment location index
2. Wrap using `TempLocation::COUNT`
3. Update UI using stored data
4. Start / reset the **30-second debounce timer**
5. Save location to NVS only when debounce expires

---

## Questions for Claude Code

Please provide:
- Final recommended data structures (if different)
- Full MQTT parsing logic
- Button event handler implementation
- NVS debounce strategy
- UI behavior for:
  - No data yet
  - Stale data
  - Partial data availability
- Any improvements you would suggest for robustness or UX

---

## Constraints

- ESP32 (limited RAM and flash)
- LVGL UI
- MQTT-driven data
- Single screen, limited space
- Reliability and flash longevity are priorities
