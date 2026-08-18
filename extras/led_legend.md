# Status LED Legend

The N64xCart Dumper has a single onboard LED, so different states are shown as
distinct blink patterns rather than colors. Checked in priority order - if
more than one condition is true at once, the higher one in this table wins.

| Pattern | Meaning |
|---|---|
| 5 quick blinks, then solid | Booting / `cartio_init()` running |
| Solid on, then off, blinking every 100ms forever | Fatal init error - couldn't read a valid ROM header. Device will not enumerate; check the cartridge seating. |
| **Fast blink** (~80ms) | **Error** - EEPROM stopped responding mid-operation, or a write was rejected for landing past the cart's detected EEPROM size. Sticky: stays until the device is reset/power-cycled. |
| **Medium blink** (~150ms) | **Write in progress** - covers both the initial host write and the EEPROM hardware flush that follows it a moment later (the flush can take a couple seconds; the LED stays in this pattern for the whole thing, not just the instant of the host write). |
| **Solid on** | **Read in progress** - any host read (ROM dump, EEPROM, SRAM, etc.). |
| Slow blink, ~1000ms | Idle, USB mass storage mounted. |
| Slow blink, ~250ms | Idle, not mounted / waiting for host. |
| Slow blink, ~2500ms | USB suspended. |

## Notes

- Read/write patterns are based on a short activity window (~150ms) after the
  last read or write event, so brief, closely-spaced host I/O reads as one
  continuous pattern rather than flickering.
- The error state does not distinguish *which* error occurred - check
  `CartTest.txt` on the mounted drive for details (e.g. `EEPROM write rejected: past detected chip size`).
- Implementation: `firmware/N64xCartDumper/src/main.c` (`led_blinking_task`),
  driven by state set in `virtualdisk.c` and `joybus.c`.
