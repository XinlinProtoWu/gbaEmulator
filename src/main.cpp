#include "ARM7TDMI.h"
#include "memoryBus.h"
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
  uint32_t lastPC = 0xffffffff;

  std::cout << std::hex << "PC=" << cpu.getPC()
            << " instr=" << cpu.getCurrentInstruction()
            << " r12=" << cpu.getLogicalRegister(12) << '\n';

  while (true) {
    cpu.step();

    uint32_t pc = cpu.getLogicalRegister(15);
    std::cout << std::hex << "PC=" << cpu.getPC()
              << " instr=" << cpu.getCurrentInstruction() << std::dec
              << " r12=" << cpu.getLogicalRegister(12) << '\n';
    if (pc == lastPC) {
      std::cout << std::dec;
      std::cout << "Finished!\n";
      std::cout << "r12 = " << cpu.getLogicalRegister(12) << '\n';
      break;
    }

    lastPC = pc;
  }
}
