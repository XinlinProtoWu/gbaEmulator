#ifndef MEMORY_BUS_H
#define MEMORY_BUS_H

#include <array>
#include <cstdint>
#include <vector>

class MemoryBus {
public:
  MemoryBus();
  ~MemoryBus();

  // Read Methods
  uint8_t read8(uint32_t address) const;
  uint16_t read16(uint32_t address) const;
  uint32_t read32(uint32_t address) const;

  // Write
  void write8(uint32_t address, uint8_t value);
  void write16(uint32_t address, uint16_t value);
  void write32(uint32_t address, uint32_t value);

private:
  // GBA Memory regions
  std::array<uint8_t, 0x4000> bios;       // BIOS (System ROM)
  std::array<uint8_t, 0x40000> wram;      // On-board Work RAM
  std::array<uint8_t, 0x8000> iram;       // Internal Work RAM
  std::array<uint8_t, 0x400> ioRegisters; // IO Registers
  std::array<uint16_t, 512> paletteRam;   // 1KB (512 entries of 16-bit colors)
  std::array<uint8_t, 0x18000> vram;      // Video RAM
  std::array<uint8_t, 0x400> oam;         // Object Attribute Memory for sprites
  std::vector<uint8_t> gamePakRom; // Game Pak ROM (dynamically sized based on
                                   // the game)
};

#endif // !MEMORY_BUS_H
