# GBA Emulator Project

This repository contains the ongoing C++ implementation of a Game Boy Advance (GBA) emulator. The current development phase establishes the foundational project architecture, specifically focusing on memory mapping, the ARM7TDMI CPU core state management, and the instruction execution pipeline.

## Current Progress & Implemented Features

### 1. Memory Architecture (Memory Bus)

Because the GBA does not utilize a traditional hard drive, all hardware components—including graphics and inputs—are mapped to specific memory addresses. The project relies on a centralized `MemoryBus` class acting as the primary state machine mediator.

* **Memory Map Allocation:** Arrays and `std::vector` structures simulate physical chips, allocating space for the System ROM (BIOS), On-board Work RAM (WRAM), Internal Work RAM (IRAM), IO Registers, Video RAM (VRAM), Object Attribute Memory (OAM), and the Game Pak ROM.


* **Read/Write Interfaces:** The memory bus safely handles component communication via implemented `read8`, `read16`, `read32`, and their corresponding write methods.



### 2. ARM7TDMI CPU Core Foundation

The core logic for the ARM7TDMI processor has been laid out, handling both physical hardware state and the dual instruction sets (32-bit ARM mode and 16-bit THUMB mode) used by commercial GBA games.

* **Register Management:** The CPU implements the 16 primary logical registers (R0-R15), including the Stack Pointer (R13), Link Register (R14), and Program Counter (R15), alongside the Current Program Status Register (CPSR).


* **Register Banking:** The architecture isolates logical registers from physical registers, allowing seamless state translation and automatic fetching of banked shadow registers during hardware mode switches (like IRQ or FIQ).


* **3-Stage Pipeline:** A simulated Fetch-Decode-Execute pipeline perfectly mimics hardware behavior. The `step()` function executes the current instruction while the Program Counter remains physically located two instructions ahead.



### 3. Opcode Decoding Switchboard

To ensure scalability across hundreds of opcodes, the instruction decoding relies on extensive lookup tables and switch statements partitioned by specific opcode bits.

* **THUMB Routing:** THUMB instructions are 16 bits wide and highly rigid. Decoding initially categorizes instructions using bits 13-15 (`opcodeGroup`), followed by nested conditional logic utilizing specific bitmasks (e.g., `0xF800`, `0xFC00`) to safely isolate sub-formats and route them to their execution functions.



### 4. Instruction Implementations

Several core ARM and THUMB operations have been successfully implemented and tested against architectural specifications:

* **Branch and Exchange (BX):** Supports swapping the CPU state between ARM and THUMB modes. This updates the CPSR 'T' bit (bit 5), properly realigns the Program Counter to either halfword or word boundaries depending on the target mode, and actively flushes and refills the 3-stage pipeline upon branching.


* **Memory Multiple Load/Store:** Implemented THUMB `PUSH` (stores in memory, decrements SP) and `POP` (loads from memory, increments SP) instructions. The stack operates as 'full descending', correctly adjusting the Stack Pointer while handling arbitrary register lists (R0-R7, LR, and PC).


* **ALU Operations:** Core arithmetic operations including `ADD` and `SUB` for THUMB mode are implemented, supporting both register-to-register calculations and immediate offset additions.

## 5. Next Steps

* **Pass All GBA CPU Tests:** To validate the CPU core, the emulator must pass the full `jsmolka/gba-tests` suite. To avoid potential BIOS bugs during testing, the ROMs will be mapped directly to `0x08000000` to bypass the boot sequence. Because a display window is not yet implemented, pass/fail states are being verified by detecting infinite loops and printing standard CPU registers (like R0 or R1) to the console.


* **Implement Direct Memory Access (DMA):** Before rendering graphics or playing sound, the four DMA channels must be implemented to instantly blast data across the bus. Many commercial games will hang at a white screen without DMA, as they rely on it to load their initial graphics and audio data.


* **Implement Video (PPU):** Graphics implementation will begin with the simpler Bitmap Modes (Modes 3, 4, and 5). The initial goal is to successfully render Mode 3, which acts as a single, flat framebuffer of 16-bit colors, using an industry-standard library like SDL2 or SFML.


* **Implement Audio (APU) and Input:** The GBA utilizes two direct sound channels (FIFOs) for sampled audio and four legacy Game Boy channels for square waves and noise. This will be output using an SDL2 audio queue. Concurrently, standard keyboard or USB controller inputs will be mapped to the `KEYINPUT` memory register.
