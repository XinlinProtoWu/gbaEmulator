#include "ARM7TDMI.h"
#include "ARMOps.h"
#include "THUMBOps.h"
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

void ARM7TDMI::setLogicalRegister(int index, uint32_t value) {
  physicalRegisters[getPhysicalRegisterIndex(index)] = value;
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

void ARM7TDMI::step() {
  // Grab instruction currently in the decode stage (pipeline[0])
  uint32_t currentInstruction = pipeline[0];

  // Fetch the next instruction from memory to keep the pipeline full
  fillPipeline();

  // Bit 5 of CPSR 0 = ARM 1 = THUMB
  bool isThumb = (cpsr & 0x20);

  if (isThumb) {
    executeTHUMB(currentInstruction);
  } else {
    executeARM(currentInstruction);
  }
}

void ARM7TDMI::executeARM(uint32_t instruction) {
  // Evaluate Condition Codes (Bits 28-31)
  uint8_t cond = (instruction >> 28) & 0xF;

  // TODO: Implement a checkCondition(cond) helper

  // Main Instruction Decode bits (25-27)
  uint8_t opcodeGroup = (instruction >> 25) & 0x07;

  switch (opcodeGroup) {
  case 0x00:
    // Multiply
    // Multiply Long
    // Single Data Swap
    // Halfword data transfer register/immediate offset
    // Where bits 7 and 4 are 1 for sure
    if ((instruction & 0x00000090) == 0x00000090) {
      // Bit 7 & 4 both 1 means mul, single data swap, or halfword data transfer
      if ((instruction & 0x000000F0) == 0x00000090) {
        // bit 5 and 6 00b must be
        // Multiplication or Single Data Swap
        if ((instruction & 0x01B00000) == 0x01000000) {
          // Single data swap
          ARMOps::singleDataSwap(*this, instruction);
          break;
        } else {
          // Multiplication
          ARMOps::muliply(*this, instruction);
          break;
        }
      } else {
        // must be Halfword data transfer
        if ((instruction & 0x00400F00) == 0x00000000) {
          // bit 22 being 0 and bits 11-8 being 0
          // halfword data transfer register offset
          ARMOps::halfwordDataTransReg(*this, instruction);
          break;
        } else {
          // Halfword data transfer imm offset
          ARMOps::halfwordDataTransImm(*this, instruction);
          break;
        }
      }
    }
    if ((instruction & 0x01FFFF00) == 0x012FFF00) {
      // Branch and Exchange,
      ARMOps::branchAndExchange(*this, instruction);
      break;
    }
    // PSR Transfer (MRS, MSR)
    if ((instruction & 0x019F0FFF) == 0x010F0000) {
      // MRS
      ARMOps::MRS(*this, instruction);
      break;
    }
    if ((instruction & 0x0190F000) == 0x0100F000) {
      // MSR
      ARMOps::MSR(*this, instruction);
      break;
    } else {
      // ALU
      ARMOps::ALU(*this, instruction);
      break;
    }
  case 0x01:
    // Data processing and FSR transfer
    // PSR Transfer (MRS, MSR)
    if ((instruction & 0x019F0FFF) == 0x010F0000) {
      // MRS
      ARMOps::MRS(*this, instruction);
      break;
    }
    if ((instruction & 0x0190F000) == 0x0100F000) {
      // MSR
      ARMOps::MSR(*this, instruction);
      break;
    } else {
      // ALU
      ARMOps::ALU(*this, instruction);
      break;
    }
  case 0x02:
    // Load/store word or unsigned byte (immediate)
    ARMOps::loadStoreWBImm(*this, instruction);
    break;
  case 0x03:
    // Load/store word or unsigned byte (register)
    ARMOps::loadStoreWBReg(*this, instruction);
    break;
  case 0x04:
    // Block data transfer
    ARMOps::blockDataTransfer(*this, instruction);
    break;
  case 0x05:
    // Branch
    ARMOps::branch(*this, instruction);
    break;
  case 0x06:
    // Coprocessor Data Transfer
    ARMOps::coprocessorDataTransfer(*this, instruction);
    break;
  case 0x07:
    // Coprocessor Data Operation
    // Coprocessor Register Transfer,
    // Software Interrrupt
    if ((instruction & 0x0F000010) == 0x0E000010) {
      // Coprocessor Register Transfer
      ARMOps::coprocessorRegTransfer(*this, instruction);
      break;
    }
    if ((instruction & 0x0F000010) == 0x0E000000) {
      // Coprocessor Data Operations
      ARMOps::coprocessorDataOPP(*this, instruction);
      break;
    } else {
      // Software Interrupt
      ARMOps::SWI(*this, instruction);
      break;
    }
  }
}

void ARM7TDMI::executeTHUMB(uint32_t instruction) {
  // THUMB instructions are only 16 bits unlike ARM's 32
  uint16_t thumbInstr = static_cast<uint16_t>(instruction);

  // Decode bits 13-15 for operation
  uint8_t opcodeGroup = (thumbInstr >> 13) & 0x07;

  switch (opcodeGroup) {
  case 0x00:
    // Move shifted registers
    // Add and subtract
    if ((thumbInstr & 0xF800) == 0x1800) {
      // Add and Subtract
      THUMBOps::addAndSub(*this, thumbInstr);
      break;
    } else {
      // Move shifted register
      THUMBOps::moveShiftedReg(*this, thumbInstr);
      break;
    }
    break;
  case 0x01:
    // Move, compare, add, and sub immediate
    THUMBOps::MCASImm(*this, thumbInstr);
    break;
  case 0x02:
    // ALU Operation
    // High registers operations and branch exchange
    // PC-relative load
    // Load and store with relative offset
    // Load and store sign-extended byte and Halfword
    if ((thumbInstr & 0xFC00) == 0x4000) {
      // ALU
      THUMBOps::ALU(*this, thumbInstr);
      break;
    } else if ((thumbInstr & 0xFC00) == 0x4400) {
      // High register operations and branch exchange
      THUMBOps::hiRegOpBE(*this, thumbInstr);
      break;
    } else if ((thumbInstr & 0xF800) == 0x4800) {
      // load PC relative
      THUMBOps::loadPCRel(*this, thumbInstr);
      break;
    } else if ((thumbInstr & 0xF200) == 0x5000) {
      // load/store with relative offset
      THUMBOps::loadStoreRelOff(*this, thumbInstr);
      break;
    } else if ((thumbInstr & 0xF200) == 0x5200) {
      // load/store sign-extended byte and halfword
      THUMBOps::loadStoreSBHw(*this, thumbInstr);
      break;
    }
    break;
  case 0x03:
    // Load and store with immediate offset
    THUMBOps::loadStoreImmOff(*this, thumbInstr);
    break;
  case 0x04:
    // Load and store halfword
    // SP-relative load and store
    if ((thumbInstr & 0xF000) == 0x8000) {
      // Load and store halfword
      THUMBOps::loadStoreHw(*this, thumbInstr);
      break;
    } else if ((thumbInstr & 0xF000) == 0x9000) {
      // SP-relative load and store
      THUMBOps::spRelLoadStore(*this, thumbInstr);
      break;
    }
    break;
  case 0x05:
    // Load address
    // Add offset to stack pointer
    // Push and pop registers
    if ((thumbInstr & 0xF000) == 0xA000) {
      // Load address
      THUMBOps::loadAdr(*this, thumbInstr);
      break;
    } else if ((thumbInstr & 0xFF00) == 0xB000) {
      // Add offset to stack pointer
      THUMBOps::addOffSP(*this, thumbInstr);
      break;
    } else if ((thumbInstr & 0xF600) == 0xB400) {
      // push and pop registers
      THUMBOps::ppReg(*this, thumbInstr);
      break;
    }
    break;
  case 0x06:
    // Multple load and store
    // Conditional branch
    // Software Interrrupt
    if ((thumbInstr & 0xF000) == 0xC000) {
      // Multple load and store
      THUMBOps::mulLoadStore(*this, thumbInstr);
      break;
    } else if ((thumbInstr & 0xFF00) == 0xDF00) {
      // Software Interrupt
      THUMBOps::SWI(*this, thumbInstr);
      break;
    } else if ((thumbInstr & 0xF000) == 0xD000) {
      // Conditional Branch
      THUMBOps::condBranch(*this, thumbInstr);
      break;
    }
    break;
  case 0x07:
    // Unconditional Branch
    // Long Branch with Link
    if ((thumbInstr & 0xF800) == 0xE000) {
      // Unconditional Branch
      THUMBOps::uncondBranch(*this, thumbInstr);
      break;
    } else if ((thumbInstr & 0xF000) == 0xF000) {
      // Long Branch with Link
      THUMBOps::longBranchWLink(*this, thumbInstr);
      break;
    }
    break;
  }
}
