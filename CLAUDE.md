# CANBUS Projects — General Notes for Claude

## Projects in this directory

| Folder | Hardware | Description |
|---|---|---|
| `haltech_dash/` | ESP32 + SSD1306 OLED + rotary encoder | Multi-page Haltech CAN dashboard, encoder-navigated |
| `TJR_mini/` | Waveshare ESP32-S3-1.64 AMOLED Touch display 280 x 456 | Single-value touch display with auto-rotation |
| `haltech_sim/` | — | CAN bus simulator / test tool |

Each project has its own `CLAUDE.md` with hardware details, pin assignments, compile commands, and gotchas. Read the project-level file before making changes.

## General Preferences

- Verbose comments — code should be self-explanatory for future editing
- Modular file structure where the project warrants it
- Clean, professional UI layouts — numbers must not overlap headings or shift position as values change
- **Always use advance-based (mono) centering for numeric values** — never tight bounding-box centering. In TJR_mini this means `printCentredMono` for all values in both portrait and landscape. Using bounding-box width causes numbers to jiggle as digits change.
- Compile and verify with `arduino-cli` before reporting work as done
- All projects target Haltech ECU CAN bus (Broadcast CAN v2.0, 1 Mbit/s)
- Haltech protocol reference PDF is in the `Resources/` folder
