# AW20216S-firmware

## How does the chip work? (Key Concepts)
    * LED Activation (Scanning): The chip doesn't turn on all the LEDs at once. It uses a technique called Time Division Multiplexing (TDM).

    * Rows (SW1-SW12): The chip activates switch 1 (SW1), waits a moment, turns it off, activates SW2, and so on up to SW12, repeating the cycle very quickly (thousands of times per second).

    * Columns (CS1-CS18): While SW1 is active, the chip reads its internal memory (the PWM registers). If the memory indicates that the LED at position (SW1, CS1) should be on, the chip internally connects pin CS1 to ground (GND), allowing current to flow and the LED to illuminate.

    * Persistence: Because this happens so quickly (>20kHz if configured), the human eye doesn't perceive a flicker, but rather a constant image.

## Why does the chip have "Pages"?
The concept of pages is used because the chip has too many registers for an 8-bit address.

    * The problem: An 8-bit memory address (the standard in basic I2C/SPI) can only address 256 locations (0x00 to 0xFF).

    * The chip's need: The AW20216S has:

        * 216 registers just for brightness (PWM).

        * 216 registers just for current (scaling).

        * More than 70 registers for configuration and patterns.

        * In total, it exceeds 500 addresses. These cannot fit on a single map from 0 to 255.

### How does it work? 
The chip divides its memory into 5 sections called Pages:

    * Page 0: Global configuration (power on, reset, alarms, pattern settings).

    * Page 1: PWM (Brightness) registers for each individual LED.

    * Page 2: Scaling (Current) registers to adjust the white balance of each LED.

    * Page 3: Breathing pattern selection.

    * Page 4 (Virtual): This one is special. It doesn't have its own memory; it's a "shortcut." If you write here, the chip automatically updates the PWM and Scaling simultaneously, which is very useful for fast animations.

In the SPI protocol: Unlike other chips where you have to send a "Change to Page 1" command, the AW20216S is very efficient: The page number is embedded in the first byte of each SPI command.

    * Bits D3, D2, and D1 of the first byte indicate the page (000 = Page 0, 001 = Page 1, etc.).

    * This means you can write to a configuration register (Page 0) and, a millisecond later, to a brightness register (Page 1) without any additional intermediate commands.
