# Raycaster-Arduino-Demo-SSD1306-

A highly optimized, buffer-less 3D raycasting engine running on a standard Arduino Nano (ATmega328P) and a 128x64 I2C OLED display.

https://github.com/user-attachments/assets/574962cd-1f20-4cf6-a347-4f5579b9ab11

Instead of relying on a standard 1KB SRAM frame buffer, this engine streams 64-bit vertical column slices directly into the OLED's hardware registers using non-blocking asynchronous I2C (TWI) burst writes. Combined with Q8.8 fixed-point arithmetic and a 256-byte division lookup table, it pushes the 16MHz AVR chip to its absolute limits to achieve high frame rates.

## ✨ Features

* **Zero-Buffer Rendering:** Uses SSD1306 Vertical Addressing Mode to construct and stream single-pixel columns on the fly, saving 1,024 bytes of SRAM.
* **Asynchronous TWI Pipeline:** Custom bare-metal TWI (I2C) implementation. The hardware shifts out the previous column's bytes in the background while the CPU simultaneously calculates the DDA algorithm for the next ray.
* **Q8.8 Fixed-Point Math:** Floating-point trigonometry and division are completely eliminated from the render loop.
* **Division LUT:** A 256-byte PROGMEM lookup table replaces the AVR's extremely slow 16-bit software division routines for perspective floor projection.
* **Procedural Bitwise Shading:**
* Wall texturing, gaps, and Y-axis dithering.
* Dynamically receding wireframe floor grid mapped to DDA intersections.
* Translucent floor reflections generated via binary masking.


* **Deterministic Skybox:** A scrolling starry sky generated using coordinate hashing and bitwise masking (clipped seamlessly behind walls).
* **Direct Port Manipulation:** GPIO button inputs are read directly from `PIND` and `PINC` in a single CPU cycle.

## 🛠️ Hardware Setup

| Component | Pin (Arduino Nano) | Notes |
| --- | --- | --- |
| **SSD1306 OLED (SDA)** | `A4` | I2C Data |
| **SSD1306 OLED (SCL)** | `A5` | I2C Clock |
| **Move Forward** | `D2` | Connect to GND (Internal Pull-up) |
| **Turn Left (Look)** | `A2` | Connect to GND (Internal Pull-up) |
| **Turn Right (Look)** | `D7` | Connect to GND (Internal Pull-up) |
| **Strafe Left** | `D6` | Connect to GND (Internal Pull-up) |
| **Strafe Right** | `A3` | Connect to GND (Internal Pull-up) |
| **Move Backward** | `D6` + `A3` | *Combo:* Strafe Left + Strafe Right |

*(Note: No external pull-up resistors are required for the buttons. The code enables the ATmega328P's internal pull-ups.)*

## 🧠 How the Buffer-less Rendering Works

Standard Arduino graphics libraries (like Adafruit_GFX) allocate a 128x64 pixel array (1KB) in RAM, draw to it, and push it to the display at the end of the frame. The ATmega328P only has 2KB of total RAM, making this highly restrictive for game development.

This engine works differently:

1. It configures the SSD1306 to **Vertical Addressing Mode**.
2. For each of the 128 X-coordinates, the CPU shoots a ray and calculates the wall height, floor projection, and skybox clipping.
3. It packages the entire 64-pixel vertical column into a single `uint64_t` bitmask.
4. It casts that 64-bit integer into an 8-byte array (`uint8_t*`) and fires it directly into the hardware TWI (I2C) data register.
5. While the TWI hardware physically clocks the bits out to the display over the I2C bus, the CPU immediately begins calculating the math for the next column.

## 🚀 Installation & Usage

1. Clone this repository.
2. Open the `.ino` file in the Arduino IDE.
3. No external display libraries (like `Wire.h` or `Adafruit_SSD1306.h`) are required! Everything is written bare-metal to the AVR registers.
4. Compile and upload to an Arduino Nano / Uno (ATmega328P).

## 🔮 Future Roadmap

This demo serves as the foundational rendering engine. The next phase of development will focus on:

* Implementing a 1D Z-buffer logic overlay for rendering 2D sprites (enemies, NPCs, and props) inside the 3D maze.
* Adding a DOOM-style bottom-screen UI overlay (Health, Ammo, Weapon).
* Game logic and interactive map elements.

---

*(Let me know when the repo is live, I'd love to see it out in the wild! Good luck on the sprite system implementation—you've got the perfect foundation for it now.)*
