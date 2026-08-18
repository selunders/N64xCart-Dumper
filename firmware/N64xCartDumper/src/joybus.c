
/**
 * SPX-License-Identifier: BSD-2-Clause 
 * Copyright (c) 2023 - NopJne
 * 
 * N64CartInterface
 * Enables cartridge reading (0x1000'0000)
 * FlashRam/SRAM support     (0x0800'0000)
 * SI EEPROM support
 */

//Command Description   Console Devices  Tx Bytes Rx Bytes
//0xFF    Reset & info  N64 Cartridge    1        3
//0x04    Read EEPROM   N64 Cartridge    2	      8
//0x05    Write EEPROM  N64 Cartridge    10	      1

#include <string.h>
#include "pico/stdlib.h"

#include "pico/platform.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "bsp/board.h"
#include "generated/joybus.pio.h"
#include "joybus.h"

uint32_t ReadCount = 0;
uint32_t gEepromSize = 0;
uint8_t gEepromCache[EEPROM_CACHE_SIZE];
bool gEepromDirty = false;
bool gEepromWriteSizeMismatch = false;
uint32_t gEepromLastWriteMs = 0;

void FlushEepromCache(void)
{
    if (!gEepromDirty) {
        return;
    }
    for (uint32_t blockOffset = 0; (blockOffset * 8) < gEepromSize && (blockOffset * 8) < EEPROM_CACHE_SIZE; blockOffset += 64) {
        WriteEepromData(blockOffset, gEepromCache + (blockOffset * 8));
    }
    gEepromDirty = false;
}

void EepromIdleFlushTask(void)
{
    if (gEepromDirty && (board_millis() - gEepromLastWriteMs) >= EEPROM_FLUSH_IDLE_MS) {
        FlushEepromCache();
    }
}

void __time_critical_func(convertToPio)(const uint8_t* command, const int len, uint32_t* result, int* resultLen) {
    if (len == 0) {
        *resultLen = 0;
        return;
    }
    *resultLen = len/2 + 1;
    int i;
    for (i = 0; i < *resultLen; i++) {
        result[i] = 0;
    }
    for (i = 0; i < len; i++) {
        for (int j = 0; j < 8; j++) {
            result[i / 2] += (uint32_t)(1 << (2 * (8 * (i % 2) + j) + 1));
            result[i / 2] += (uint32_t)((!!(command[i] & (0x80u >> j))) << (2 * (8 * (i % 2) + j)));
        }
    }
    // End bit
    result[len / 2] += 3 << (2 * (8 * (len % 2)));
}

PIO pio = pio0;
PIO pio_1 = pio1;
void __time_critical_func(InitEepromClock)(uint clockpin)
{
    gpio_init(clockpin);
    gpio_set_dir(clockpin, GPIO_OUT);

    pio_gpio_init(pio_1, clockpin);

    uint offset_1 = pio_add_program(pio_1, &joybus_program);
    pio_sm_config config1 = joybus_program_get_default_config(offset_1);
    //sm_config_set_out_pins(&config1, clockpin, 1);
    sm_config_set_set_pins(&config1, clockpin, 1);
    sm_config_set_clkdiv(&config1, 5);
    //sm_config_set_out_shift(&config1, true, false, 32);
    //sm_config_set_in_shift(&config1, false, true, 8);
    
    pio_sm_init(pio_1, 1, offset_1 + joybus_offset_clockgen, &config1);
    pio_sm_set_enabled(pio_1, 1, true);
}

uint32_t GetInputWithTimeout(void)
{
    uint32_t lastWriteTime = time_us_32();
    while (1) {
        if(pio_sm_is_rx_fifo_empty(pio, 0)) {
            uint32_t now = time_us_32();
            uint32_t diff = now - lastWriteTime;

            // Send the eeprom data if it's been ?Seconds since the last eeprom write
            // Reset the lastWriteTime to 0 and don't sent the data unless we get another write
            if (lastWriteTime != 0 && diff > 1000) {
                lastWriteTime = 0;
                break;
            }
        } else {
            return pio_sm_get(pio, 0);
        }
    }

    return 0xFFFFFFFF;
}

pio_sm_config config;
uint piooffset;

// Sends a joybus command and returns the first response byte via *firstInputOut.
// Leaves the PIO RX FIFO holding any remaining response bytes for the caller
// to drain (pio_sm_get_blocking). Returns false if the cart never responded
// (10 retries of the full command with no reply) - communication is lost.
static bool __time_critical_func(SendEepromCommand)(const uint8_t* command, int len, uint32_t* firstInputOut)
{
    uint32_t result[10];
    int resultLen;
    convertToPio(command, len, result, &resultLen);

    uint32_t firstInput;
    uint32_t retries = 0;
    do {
        pio_sm_set_enabled(pio, 0, false);
        pio_sm_init(pio, 0, piooffset + joybus_offset_outmode, &config);
        pio_sm_set_enabled(pio, 0, true);

        for (int i = 0; i < resultLen; i++) pio_sm_put_blocking(pio, 0, result[i]);

        firstInput = GetInputWithTimeout();
        if (retries > 10) {
            return false;
        }
        retries += 1;

    } while (firstInput == 0xFFFFFFFF);

    *firstInputOut = firstInput;
    return true;
}

void __time_critical_func(InitEeprom)(uint dataPin)
{
    gpio_init(dataPin);
    gpio_set_dir(dataPin, GPIO_IN);
    gpio_pull_up(dataPin);

    sleep_us(100); // Stabilize voltages

    pio_gpio_init(pio, dataPin);

    piooffset = pio_add_program(pio, &joybus_program);
    config = joybus_program_get_default_config(piooffset);
    sm_config_set_in_pins(&config, dataPin);
    sm_config_set_out_pins(&config, dataPin, 1);
    sm_config_set_set_pins(&config, dataPin, 1);
    sm_config_set_clkdiv(&config, 5);
    sm_config_set_out_shift(&config, true, false, 32);
    sm_config_set_in_shift(&config, false, true, 8);

    pio_sm_init(pio, 0, piooffset, &config);
    pio_sm_set_enabled(pio, 0, true);

    // Send the info command, retrying on timeout - this board's SI signaling
    // has proven marginal (see also the CIC hello), so a single unanswered
    // attempt right after reset shouldn't be taken to mean "no EEPROM".
    uint8_t probeCommand[1] = {0x00};
    uint32_t buffer[3] = {0xFFFFFFFF, 0, 0};
    SendEepromCommand(probeCommand, 1, &buffer[0]);

    if (buffer[0] == 0) {
        buffer[1] = pio_sm_get_blocking(pio, 0);
        buffer[2] = pio_sm_get_blocking(pio, 0);

        // Determine the size of the EEPROM.
        if (buffer[1] == 0x80) {
            // 4K Eeprom.
            ReadCount = 64;
            gEepromSize = 512;
        } else if (buffer[1] == 0xC0) {
            // 16K Eeprom.
            ReadCount = 256;
            gEepromSize = 512 * 4;
        } else {
            // Unknown SI eeprom type.
            ReadCount = 0;
            gEepromSize = 0;
        }
    }
}

void __time_critical_func(ReadEepromData)(uint32_t offset, uint8_t *buffer) 
{
    if (gEepromSize == 0) {
        return;
    }

    // Read the eeprom.
    for (uint32_t ReadIndex = 0; ReadIndex < 64; ReadIndex += 1) {
        // Construct the read command.
        uint8_t probeResponse[] = {0x04, (uint8_t)(ReadIndex + offset)};
        uint32_t result[8];
        int resultLen;
        convertToPio(probeResponse, 2, result, &resultLen);

        uint32_t firstInput;
        uint32_t retries = 0;
        do {
            // Send the read command
            pio_sm_set_enabled(pio, 0, false);
            pio_sm_init(pio, 0, piooffset + joybus_offset_outmode, &config);
            pio_sm_set_enabled(pio, 0, true);

            for (int i = 0; i < resultLen; i++) pio_sm_put_blocking(pio, 0, result[i]);

            firstInput = GetInputWithTimeout();
            if (retries > 10) {
                gEepromSize = 0;
                return;
            }
            retries += 1;

        } while (firstInput == 0xFFFFFFFF);
        // Read the incoming data from the cart.
        buffer[(uint)ReadIndex * 8] = (uint8_t)firstInput;
        for (int i = 1; i < 8; i += 1) {
            buffer[(uint)i + (uint)ReadIndex * 8] = (uint8_t)pio_sm_get_blocking(pio, 0);
        }
        sleep_us(200);
    }
}

void __time_critical_func(WriteEepromData)(uint32_t offset, uint8_t *buffer)
{
    // Write the eeprom, verifying (and retrying) each 8-byte block by reading
    // it back before trusting it - a write ack alone doesn't guarantee the
    // cart's internal write cycle actually finished before we moved on.
    for (uint32_t WriteIndex = 0; WriteIndex < 64; WriteIndex += 1) {
        uint8_t writeCommand[10] = {0x05, (uint8_t)(WriteIndex + offset)};
        for (uint i = 0; i < 8; i += 1) {
            writeCommand[i + 2] = buffer[i + (WriteIndex * 8)];
        }
        uint8_t readCommand[2] = {0x04, (uint8_t)(WriteIndex + offset)};

        bool verified = false;
        for (uint32_t attempt = 0; attempt < 8 && !verified; attempt += 1) {
            uint32_t firstInput;
            if (!SendEepromCommand(writeCommand, 10, &firstInput)) {
                gEepromSize = 0;
                return;
            }

            // Rx=1 for a write command: a nonzero status means the cart is
            // still busy finishing a previous write - back off and retry
            // rather than assuming this one landed.
            if ((uint8_t)firstInput != 0) {
                sleep_ms(10);
                continue;
            }

            sleep_us(200);

            // Verify by reading the same block back.
            if (!SendEepromCommand(readCommand, 2, &firstInput)) {
                gEepromSize = 0;
                return;
            }
            uint8_t readback[8];
            readback[0] = (uint8_t)firstInput;
            for (int i = 1; i < 8; i += 1) {
                readback[i] = (uint8_t)pio_sm_get_blocking(pio, 0);
            }

            verified = (memcmp(readback, writeCommand + 2, 8) == 0);
            if (!verified) {
                sleep_ms(10);
            }
        }

        sleep_us(200);
    }
}