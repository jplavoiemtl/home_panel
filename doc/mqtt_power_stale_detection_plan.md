# MQTT Power Stale Data Detection

## Problem

The power value displayed on the home panel via MQTT updates from Home Assistant can become stale if the MQTT source stops publishing (e.g., meter offline, HA restart, broker issue). The power label continues showing the last received value indefinitely, giving the user no indication that the data is outdated.

## Solution

A 5-minute staleness timeout that replaces the stale power value with "N/A" until fresh data arrives.

## Implementation

### Variables Added

- `lastPowerReceived` — timestamp of the last MQTT power message
- `powerStale` — flag to prevent redundant label writes every 2 seconds
- `MQTT_STALE_TIMEOUT_MS` — 300000 ms (5 minutes)

### Logic

1. **On MQTT power message:** Record `millis()` timestamp and clear stale flag
2. **Every 2 seconds (existing status update loop):** Check if 5 minutes have passed since last power message
3. **On stale detection:** Set label to "N/A", set stale flag, log to Serial
4. **On fresh data after stale:** Normal MQTT callback restores live value and clears stale flag

### Guards

- `lastPowerReceived > 0` — prevents triggering before the first message ever arrives (label keeps its default SquareLine text at boot)
- `!powerStale` — prevents redundant label writes every 2 seconds after already detecting staleness

## Behavior

| Scenario | Power Label |
|---|---|
| Normal MQTT updates arriving | Live value (e.g., "🏠 1234 W") |
| No update for 5+ minutes | "N/A" |
| New MQTT update arrives after stale | Immediately shows new value, resets timer |
| Device just booted, no data yet | Default SquareLine label text (not "N/A") |

## Verification

- Deploy and observe normal MQTT power updates display correctly
- Disconnect/stop the MQTT publisher and wait 5+ minutes — power label should change to "N/A"
- Resume publishing — power label should immediately show live values again
- Check Serial output for stale detection log message

## Files Modified

- `home_panel.ino` — timing variables, callback timestamp, staleness check in loop
