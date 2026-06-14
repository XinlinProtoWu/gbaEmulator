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
  uint8_t write8(uint32_t address) const;
  uint16_t write16(uint32_t address) const;
  uint32_t write32(uint32_t address) const;

private:
  // GBA Memory regions
  // Arrays/vectors will be implemented for these regions [cite: 164]
  std::array<uint8_t, 0x4000> bios;       // BIOS (System ROM) [cite: 164]
  std::array<uint8_t, 0x40000> wram;      // On-board Work RAM [cite:164]
  std::array<uint8_t, 0x8000> iram;       // Internal Work RAM [cite: 164]
  std::array<uint8_t, 0x400> ioRegisters; // IO Registers [cite: 164]
  std::array<uint8_t, 0x18000> vram;      // Video RAM [cite: 164]
  std::array<uint8_t, 0x400> oam;         // Object Attribute Memory
                                          // for sprites [cite: 164]
  std::vector<uint8_t> gamePakRom; // Game Pak ROM (dynamically sized based on
                                   // the game) [cite: 164]
};

#endif // !MEMORY_BUS_H
