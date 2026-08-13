/**
 * SPX-License-Identifier: BSD-2-Clause 
 * Copyright (c) 2023 - NopJne
 * 
 * Joybus
 * Provides SI joybus support for EEPROM interaction.
 */

#pragma once
#include <stdbool.h>
void InitEeprom(uint dataPin);
void InitEepromClock(uint clockpin);
void ReadEepromData(uint32_t offset, uint8_t *buffer);
void WriteEepromData(uint32_t offset, uint8_t *buffer);

// Full-chip RAM cache, populated once in cartio_init() (before tud_init()
// starts the USB stack) so that live host reads of ROM.eep never need to
// touch the SI_DAT/SI_CLK hardware again. Sized for the largest supported
// EEPROM (16Kbit = 2048 bytes); only the first gEepromSize bytes are valid.
#define EEPROM_CACHE_SIZE 0x800
extern uint8_t gEepromCache[EEPROM_CACHE_SIZE];

// Set whenever a host write lands in gEepromCache but hasn't been committed
// to the physical EEPROM yet. Writes are staged here rather than written
// through immediately - see FlushEepromCache().
extern bool gEepromDirty;

// Commits gEepromCache to the physical EEPROM if gEepromDirty is set, then
// clears the flag.
void FlushEepromCache(void);

// Set when a host write targets an address at or beyond the actual detected
// EEPROM size (gEepromSize) but still within the cache buffer - e.g. writing
// a 2048-byte (16Kbit) save file onto a cart with only a 512-byte (4Kbit)
// chip. Such writes are rejected (not staged) rather than silently accepted
// and only partially flushed. Surfaced in CartTest.txt.
extern bool gEepromWriteSizeMismatch;

// Timestamp (board_millis()) of the most recent staged write. Updated by the
// write path in virtualdisk.c whenever gEepromDirty is set.
extern uint32_t gEepromLastWriteMs;

// How long (ms) to wait after the last write, with no further writes, before
// auto-flushing to the physical EEPROM.
#define EEPROM_FLUSH_IDLE_MS 1000

// Call once per main loop iteration (see main.c). Flushes gEepromCache once
// EEPROM_FLUSH_IDLE_MS has elapsed since the last write with no new write in
// between - this keeps writes off the shared SI_DAT/SI_CLK hardware during
// bursts of host I/O, without depending on the host ever sending an eject
// command (many OSes, e.g. Windows' default "Quick Removal" policy, let
// users unplug without ever doing so).
void EepromIdleFlushTask(void);

extern uint32_t gEepromSize;