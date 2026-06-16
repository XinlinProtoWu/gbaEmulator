#include "memoryBus.h"
#include <cstdint>
#include <iomanip>  // <-- Add this line for std::setw and std::left
#include <iostream> // For testing only

MemoryBus::MemoryBus() {
  // Initialize the Game Pak ROM with some dummy size for testing
  gamePakRom.resize(0x1000000, 0);

  // BIOS and ROM will be loaded in here
  bios.fill(0);
  wram.fill(0);
  iram.fill(0);
  ioRegisters.fill(0);
  paletteRam.fill(0);
  vram.fill(0);
  oam.fill(0);
}

MemoryBus::~MemoryBus() {}

// Read

uint8_t MemoryBus::read8(uint32_t address) const {
  uint8_t region =
      (address >> 24) & 0xFF; // Top 2 most significant bytes decide the region

  switch (region) {
  case 0x00: // BIOS: Sits from 0x00000000-0x00003FFF
    if (address < 0x4000)
      return bios[address];
    break;
  case 0x02: // WRAM: Sits from 0x02000000-0x0203FFFF
    return wram[address & 0x3FFFF];
  case 0x03: // IRAM: Sits from 0x03000000-0x03007FFF
    return iram[address & 0x7FFF];
  case 0x04: // IO Reg: Sits from 0x04000000-0x040003FE
    return ioRegisters[address & 0x3FF];
  case 0x05: { // Palette RAM: Sits from 0x05000000-0x050003FF
    uint32_t offset = address & 0x3FF;
    // Because paletteRam contains 16 bit elements, offset/2 essentially makes
    // each index point to 2 elements
    uint16_t val = paletteRam[offset / 2];
    // Since each index contains 2 elements, whether the address is even or odd
    // determines which one to retrieve
    return (offset % 2 == 0) ? (val & 0xFF) : (val >> 8);
  }
  case 0x06: // VRAM: Sits from 0x06000000-0x06017FFF
    return vram[address & 0x17FFF];
  case 0x07: // OAM: Sits from 0x07000000-0x070003FF
    return oam[address & 0x3FF];
  case 0x08: // Game Pak ROM
  case 0x09:
  case 0x0A:
  case 0x0B:
  case 0x0C:
  case 0x0D: {
    uint32_t offset = address & 0x01FFFFFF;
    // Use modulo to handle mirroring if the ROM is smaller than 32MB
    if (gamePakRom.size() > 0) {
      return gamePakRom[offset % gamePakRom.size()];
    }
    break;
  }
  default:
    return 0; // Unmapped memory returns 0
  }
  return 0;
}

// For read16 and read32, read the lower and higher 8/16 bits respectively and
// or them to get the full 16/32 bit result
uint16_t MemoryBus::read16(uint32_t address) const {
  // GBA is little endian:
  uint16_t lo = read8(address);
  uint16_t hi = read8(address + 1);
  return lo | (hi << 8);
}

uint32_t MemoryBus::read32(uint32_t address) const {
  uint32_t lo = read16(address);
  uint32_t hi = read16(address + 2);
  return lo | (hi << 16);
}

// Write

void MemoryBus::write8(uint32_t address, uint8_t value) {
  uint8_t region = (address >> 24) & 0xFF;
  switch (region) {
  case 0x02: // WRAM
    wram[address & 0x3FFFF] = value;
    break;
  case 0x03: // IRAM
    iram[address & 0x7FFF] = value;
    break;
  case 0x04: // IO Registers
    ioRegisters[address & 0x3FF] = value;
    break;
  case 0x05: { // Palette RAM (1KB)
    uint32_t offset = address & 0x3FF;
    uint16_t &val = paletteRam[offset / 2];
    val = (offset % 2 == 0) ? ((val & 0xFF00) | value)
                            : (val & 0x00FF | (value << 8));
    break;
  }
  case 0x06: // VRAM
    vram[address & 0x17FFF] = value;
    break;
  case 0x07: // OAM
    oam[address & 0x3FF] = value;
    break;
  default:
    break; // Ignore writes to unmapped/read-only memory
  }
}

void MemoryBus::write16(uint32_t address, uint16_t value) {
  write8(address, value & 0xFF);
  write8(address + 1, (value >> 8) & 0xFF);
}

void MemoryBus::write32(uint32_t address, uint32_t value) {
  write16(address, value & 0xFFFF);
  write16(address + 2, (value >> 16) & 0xFFFF);
}

// Simple helper to print test results
void assertTest(const std::string &testName, bool condition) {
  std::cout << std::left << std::setw(30) << testName
            << (condition ? "[ PASS ]" : "[ FAIL ]") << std::endl;
}

int main() {
  std::cout << "--- GBA Memory Bus Initialization Test ---" << std::endl;

  MemoryBus bus;

  // Test 1: WRAM Read/Write (8-bit)
  uint32_t wramAddr = 0x02001234;
  bus.write8(wramAddr, 0xAB);
  assertTest("WRAM 8-bit Write/Read", bus.read8(wramAddr) == 0xAB);

  // Test 2: IRAM Read/Write (32-bit)
  uint32_t iramAddr = 0x03004560;
  bus.write32(iramAddr, 0xDEADBEEF);
  assertTest("IRAM 32-bit Write/Read", bus.read32(iramAddr) == 0xDEADBEEF);

  // Test 3: Little-Endian Storage Verification
  // If we wrote 0xDEADBEEF to iramAddr, the byte at iramAddr should be 0xEF
  assertTest("Little-Endian Validation", bus.read8(iramAddr) == 0xEF);

  // Test 4: Palette RAM (16-bit boundaries mapped correctly)
  // 0x05000000 is the start of Palette RAM
  uint32_t paletteAddr = 0x05000200;
  bus.write16(paletteAddr, 0x7FFF); // White color in GBA format
  assertTest("Palette RAM 16-bit Access", bus.read16(paletteAddr) == 0x7FFF);

  // Test 5: ROM Read-Only Protection
  // Trying to write to the Game Pak ROM should not modify it.
  uint32_t romAddr = 0x08000100;
  bus.write32(romAddr, 0xFFFFFFFF);
  assertTest("ROM Read-Only Protection", bus.read32(romAddr) == 0x00000000);

  // Test 6: VRAM 16-bit Write
  uint32_t vramAddr = 0x06000000;
  bus.write16(vramAddr, 0x1234);
  assertTest("VRAM 16-bit Access", bus.read16(vramAddr) == 0x1234);

  std::cout << "--- Testing Complete ---" << std::endl;

  return 0;
}
