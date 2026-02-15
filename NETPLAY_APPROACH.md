# Netplay Implementation Approach: melonDS

This document outlines the technical strategy used for the Netplay implementation in this fork of melonDS.

## Overview
The implementation utilizes a **Rollback-based Input Synchronization** model (similar to GGPO or Fightcade) rather than traditional network-level hardware emulation. Instead of syncing sensitive WiFi packets, the system synchronizes deterministic human inputs across identical emulator states.

## Key Components

### 1. Deterministic Mirroring (The "Blob" System)
For rollback netplay to work, both instances must be 100% identical. 
* **Initial Sync:** When a game starts, the Host captures its entire state (RAM, VRAM, CPU registers) and sends it to the Client as a "Blob" (using `SendBlob`).
* **Consistency:** This ensures both players start with identical memory and clock states.

### 2. Input Synchronization & Dual-Frame Recording
The system captures local inputs and processes them in two ways simultaneously:
* **Immediate Frame:** Recorded for the local instance at the current frame count (`nds->NumFrames`). This ensures the local history is accurate for potential rollbacks.
* **Delayed Frame:** Recorded for a future frame (`nds->NumFrames + Settings.Delay`). This provides a buffer to hide network latency.
* **Input Reports:** Inputs are bundled into reports and broadcasted over ENet. Reports include the current frame index, the last completed frame, and a **Sequence Counter (`seq`)** to handle out-of-order or dropped UDP packets.

### 3. Connection Management (Endpoint Mapping)
To support reliable networking through NATs and multiple players:
* **64-bit Keys:** The system maps remote players using a unique key generated from their IP address and Port (`MakeEndpointKey`).
* **Dynamic Indexing:** ENet peers are dynamically mapped to internal Player IDs, allowing for cleaner multi-instance support.

### 4. Rollback & Recovery
The system handles network jitter by allowing the emulator to run ahead, then "fixing" history if remote data arrives late:
* **Savestate Checkpoints:** If remote inputs are missing for the current frame, the system saves a "checkpoint" savestate (`PendingFrame`).
* **The Rollback Loop:** When the delayed remote inputs finally arrive:
    1. The emulator loads the `PendingFrame` savestate.
    2. It rewinds the frame counter.
    3. It re-simulates all frames up to the present in a fast-forward loop, applying the newly received inputs for each step.
* **Metrics:** The implementation tracks `load_ms` and `save_ms` to monitor the performance impact of rollbacks on the user experience.

### 5. LocalMP Integration
The implementation inherits from `LocalMP`. This allows the system to:
* Leverage existing multi-instance logic for DS-to-DS wireless communication.
* Map remote network players to "local instances" within the emulator's logic, allowing local shared memory communication to handle the actual Wireless hardware emulation.

## Summary of Logic Flow
1. **Capture:** `ProcessInput()` records local keys (both immediate and delayed) and broadcasts them with a sequence ID.
2. **Receive:** `ReceiveInputs()` validates the sender endpoint, stores remote keys, and triggers a rollback if a timeline gap is filled.
3. **Apply:** `ApplyInput()` injects the synchronized keys into the specific NDS instance associated with that player's ID.
4. **Stall:** If the remote player falls too far behind, `StallFrame` is triggered to maintain a reasonable synchronization window.
