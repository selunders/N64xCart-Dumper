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

// Full-chip RAM cache, populated once at boot so live reads never touch
// hardware again. Only the first gEepromSize bytes are valid.
#define EEPROM_CACHE_SIZE 0x800
extern uint8_t gEepromCache[EEPROM_CACHE_SIZE];

// Set when gEepromCache has unflushed writes - see FlushEepromCache().
extern bool gEepromDirty;

// Commits gEepromCache to the physical EEPROM if gEepromDirty is set.
void FlushEepromCache(void);

// Set when a write targets past the detected EEPROM size (e.g. a 16Kbit
// save on a 4Kbit chip) - rejected outright rather than partially flushed.
// Surfaced in CartTest.txt.
extern bool gEepromWriteSizeMismatch;

// Timestamp (board_millis()) of the most recent staged write - see
// virtualdisk.c.
extern uint32_t gEepromLastWriteMs;

// Idle time (ms) after the last write before auto-flushing.
#define EEPROM_FLUSH_IDLE_MS 1000

// Flushes gEepromCache after EEPROM_FLUSH_IDLE_MS of inactivity - doesn't
// rely on an eject command, since many OSes (e.g. Windows' Quick Removal)
// let users unplug without ever sending one. Call once per main loop
// iteration.
void EepromIdleFlushTask(void);

extern uint32_t gEepromSize;

// Set when EEPROM stops responding after being detected present - distinct
// from "no EEPROM chip" (normal, never sets this). Sticky until reboot.
extern bool gEepromCommError;

// Timestamp (board_millis()) of the most recent read/write activity, for
// the status LED. Initialized far in the past so nothing looks active
// before the first real event.
extern volatile uint32_t gLastCartReadMs;
extern volatile uint32_t gLastCartWriteMs;