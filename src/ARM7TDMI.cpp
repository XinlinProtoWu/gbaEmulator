#include "ARM7TDMI.h"
#include "memoryBus.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>

// Contructor
ARM7TDMI::ARM7TDMI(MemoryBus &bus) : memoryBus(bus) { reset(); }

void ARM7TDMI::reset() {
  // Upon reset, Cpu is set to Supervisor mode 0x10111, and the I (7th, IRQ) and
  // F (6th, FIQ) bits are set to 1
  cpsr = 0x000000D3;
  currentMode = static_cast<uint8_t>(CpuMode::Supervisor);

  // After reset, all register values except the PC and CPSR are indeterminate.
  physicalRegisters.fill(0);

  // The processor forces PC to fetch the next instruction
  physicalRegisters[getPhysicalRegisterIndex(15)] = 0x00000000;

  flushPipeline();
}

uint32_t ARM7TDMI::getCPSR() const { return cpsr; }

uint8_t ARM7TDMI::getPhysicalRegisterIndex(int logicalIndex) const {
  // R0-R7 and R15(PC) are unbanked and shared across all operating modes.
  if (logicalIndex < 8 || logicalIndex == 15) {
    return logicalIndex;
  }

  switch (static_cast<CpuMode>(currentMode)) {
  case CpuMode::User:
  case CpuMode::System:
    // User and System share the standard R8-R14 registers
    return logicalIndex;
  case CpuMode::FIQ:
    // FIQ mode banks R8 through R14
    // Maps to physical R16-22
    if (logicalIndex >= 8 && logicalIndex <= 14)
      return logicalIndex + 8;
    break;
  case CpuMode::Supervisor:
    // Supervisor mode banks R13 and R14
    // Maps to physical R23-24
    if (logicalIndex == 13 || logicalIndex == 14)
      return logicalIndex + 10;
    break;
  case CpuMode::Abort:
    // Abort mode banks R13 and R14
    // Maps to physical R25-26
    if (logicalIndex == 13 || logicalIndex == 14)
      return logicalIndex + 12;
    break;
  case CpuMode::IRQ:
    // IRQ mode banks R13 and R14
    // Maps to physical R27-28
    if (logicalIndex == 13 || logicalIndex == 14)
      return logicalIndex + 14;
    break;
  case CpuMode::Undefined:
    // Undefined mode banks R13 and R14
    // Maps to physical R29-30
    if (logicalIndex == 13 || logicalIndex == 14)
      return logicalIndex + 16;
    break;
  }
  // Default fallback
  return logicalIndex;
}

uint32_t ARM7TDMI::getLogicalRegister(int index) const {
  return physicalRegisters[getPhysicalRegisterIndex(index)];
}

uint8_t ARM7TDMI::getSPSRIndex() {
  switch (static_cast<CpuMode>(currentMode)) {
  case CpuMode::FIQ:
    return 0;
  case CpuMode::Supervisor:
    return 1;
  case CpuMode::Abort:
    return 2;
  case CpuMode::IRQ:
    return 3;
  case CpuMode::Undefined:
    return 4;
  default:
    return 0;
  }
}

uint32_t &ARM7TDMI::getCurrentSPSR() {
  if (currentMode == static_cast<uint8_t>(CpuMode::User) ||
      currentMode == static_cast<uint8_t>(CpuMode::System)) {
    throw std::logic_error("User/System mode has no SPSR!");
  }
  return spsr[getSPSRIndex()];
}

void ARM7TDMI::flushPipeline() {
  // Check the T-bit (5th) in CPSR for operating state
  // Arm state: Execute 32 bit, word aligned
  // Thumb state: Execute 16 bit, halfword-aligned
  bool isThumb = (cpsr & 0x20);
  uint32_t pc = physicalRegisters[getPhysicalRegisterIndex(15)];

  if (isThumb) {
    // Fill the 2 stage array with 16 bit Thumb instruction
    // The PC must increment by 2 (a half word)
    pipeline[0] = memoryBus.read16(pc);
    pc += 2;
    pipeline[1] = memoryBus.read16(pc);
    pc += 2;
  } else {
    // Fill the 2 stage array with 32 bit ARM instruction
    // The PC must increment by 4 (a word)
    pipeline[0] = memoryBus.read32(pc);
    pc += 4;
    pipeline[1] = memoryBus.read32(pc);
    pc += 4;
  }

  // Save the advanced PC back to register bank
  physicalRegisters[getPhysicalRegisterIndex(15)] = pc;
}

void ARM7TDMI::fillPipeline() {
  bool isThumb = (cpsr & 0x20);
  uint32_t pc = physicalRegisters[getPhysicalRegisterIndex(15)];

  // Shift the pipeline forward
  // Instruction B (pipeline 0) is tanslated, Instruction C (pipeline [1]) is
  // fetched
  pipeline[0] = pipeline[1];

  // Fetch new instruction into the empty fetch page
  if (isThumb) {
    pipeline[1] = memoryBus.read16(pc);
    pc += 2;
  } else {
    pipeline[1] = memoryBus.read32(pc);
    pc += 4;
  }
  physicalRegisters[getPhysicalRegisterIndex(15)] = pc;
}
