#include "ARM7TDMI.h"
#include "memoryBus.h"
#include <cstdint>
#include <ios>
#include <iostream>

int main() {
  MemoryBus bus;

  if (!bus.loadROM("../../gba-tests/arm/arm.gba")) {
    return 1;
  }

  ARM7TDMI cpu(bus);

  // Initialize CPU state (sets PC to 0x00000000 and fills pipeline)
  cpu.reset();

  // Force jump to Game Pak and resync the pipeline
  cpu.forceJump(0x08000000);
  cpu.initializeGBAState();
  uint32_t lastExecutingAddr = 0xffffffff;

  std::cout << std::hex << "PC=" << cpu.getPC()
            << " instr=" << cpu.getCurrentInstruction()
            << " r12=" << cpu.getLogicalRegister(12) << '\n';

  while (true) {
    cpu.step();

    uint32_t currentModeOffset = (cpu.getCPSR() & 0x20) ? 4 : 8;
    uint32_t executingAddr = cpu.getLogicalRegister(15) - currentModeOffset;
    std::cout << std::hex << "PC=" << cpu.getPC()
              << " instr=" << cpu.getCurrentInstruction() << std::dec
              << " r12=" << cpu.getLogicalRegister(12) << '\n';
    if (executingAddr == lastExecutingAddr ||
        cpu.getLogicalRegister(12) == 152) {
      std::cout << std::dec;
      std::cout << "Finished!\n";
      std::cout << "r12 = " << cpu.getLogicalRegister(12) << '\n';
      break;
    }

    lastExecutingAddr = executingAddr;
  }
  return 0;
}
