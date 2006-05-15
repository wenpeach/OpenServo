/*
    Copyright (c) 2006 Michael P. Thompson <mpthompson@gmail.com>

    Permission is hereby granted, free of charge, to any person 
    obtaining a copy of this software and associated documentation 
    files (the "Software"), to deal in the Software without 
    restriction, including without limitation the rights to use, copy, 
    modify, merge, publish, distribute, sublicense, and/or sell copies 
    of the Software, and to permit persons to whom the Software is 
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be 
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
    DEALINGS IN THE SOFTWARE.

    $Id$
*/

#include <inttypes.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#include "openservo.h"
#include "config.h"
#include "registers.h"
#include "seek.h"

// Persistant seek variables.
static uint8_t seek_state;
static uint32_t seek_accumulator;

void seek_init(uint16_t init_position)
// Initialize the seek module.
{
    // Initialize the speed state.
    seek_state = 0;

    // Initialize the seek related registers.
    registers_write_word(REG_SEEK_HI, REG_SEEK_LO, init_position);
    registers_write_word(REG_SEEK_NEXT_HI, REG_SEEK_NEXT_LO, 0x0000);
    registers_write_word(REG_SEEK_SPEED_HI, REG_SEEK_SPEED_LO, 0x0000);
    registers_write_word(REG_SEEK_TIME_HI, REG_SEEK_TIME_LO, 0x0000);
}


void seek_update(void)
// Update the seek position as needed.
{
    uint16_t seek;
    uint16_t seek_next;
    uint16_t seek_time;
    uint16_t seek_speed;

    // Get the speed seek information.
    seek = (int16_t) registers_read_word(REG_SEEK_HI, REG_SEEK_LO);
    seek_next = (int16_t) registers_read_word(REG_SEEK_NEXT_HI, REG_SEEK_NEXT_LO);
    seek_time = (int16_t) registers_read_word(REG_SEEK_TIME_HI, REG_SEEK_TIME_LO);
    seek_speed = (int16_t) registers_read_word(REG_SEEK_SPEED_HI, REG_SEEK_SPEED_LO);

    // Do we have a time?
    if (seek_time > 0)
    {
        // Are we seeking upward or downward?
        if (seek < seek_next)
        {
            // Automatically determine the speed.
            uint32_t delta = seek_next - seek;

            // Adjust for 8:8 fixed point value.
            delta <<= 8;

            // Divide by time.
            delta /= seek_time;

            // Set the speed.
            seek_speed = (uint16_t) delta;

            // Write the new speed.
            registers_write_word(REG_SEEK_SPEED_HI, REG_SEEK_SPEED_LO, seek_speed);
        }
        else if (seek > seek_next)
        {
            // Automatically determine the speed.
            uint32_t delta = seek - seek_next;

            // Adjust for 8:8 fixed point value.
            delta <<= 8;

            // Divide by time.
            delta /= seek_time;

            // Set the speed.
            seek_speed = (uint16_t) delta;

            // Write the new speed.
            registers_write_word(REG_SEEK_SPEED_HI, REG_SEEK_SPEED_LO, seek_speed);
        }        

        // The time has been converted to speed so it is reset to zero.
        registers_write_word(REG_SEEK_TIME_HI, REG_SEEK_TIME_LO, 0x0000);
    }

    // Do we have a speed?
    if (seek_speed > 0)
    {
        // Are we just starting.
        if (seek_state == 0)
        {
            // Reset the accumulator.
            seek_accumulator = seek;
            seek_accumulator <<= 8;

            // We have started.
            seek_state = 1;
        }

        // Are we seeking upward or downward?
        if (seek < seek_next)
        {
            // Add speed to position accumulator.
            seek_accumulator += (int16_t) seek_speed;

            // Move accumulator to seek position.
            seek = seek_accumulator >> 8;

            // Adjust seek as needed.
            if (seek >= seek_next)
            {
                // Reset the seek position.
                seek = seek_next;

                // Reset the speed.
                registers_write_word(REG_SEEK_SPEED_HI, REG_SEEK_SPEED_LO, 0x0000);

                // We are finished.
                seek_state = 0;
            }

            // Update the seek position.
            registers_write_word(REG_SEEK_HI, REG_SEEK_LO, seek);
        }
        else if (seek > seek_next)
        {
            // Subtract speed from position accumulator.
            seek_accumulator -= seek_speed;

            // Make sure we haven't underflowed to less than zero.
            if (seek_accumulator < 0) seek_accumulator = 0;

            // Move accumulator to seek position.
            seek = seek_accumulator >> 8;

            // Adjust seek as needed.
            if (seek <= seek_next)
            {
                // Reset the seek position.
                seek = seek_next;

                // Reset the speed.
                registers_write_word(REG_SEEK_SPEED_HI, REG_SEEK_SPEED_LO, 0x0000);

                // We are finished.
                seek_state = 0;
            }

            // Update the seek position.
            registers_write_word(REG_SEEK_HI, REG_SEEK_LO, seek);
        }
        else
        {
            // Reset the speed.
            registers_write_word(REG_SEEK_SPEED_HI, REG_SEEK_SPEED_LO, 0x0000);

            // We are finished.
            seek_state = 0;
        }
    }
}

