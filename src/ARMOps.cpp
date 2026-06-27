#include "ARMOps.h"
#include "ARM7TDMI.h"
#include "memoryBus.h"
#include <cstdint>
#include <iostream>

// Helper function to check for illegal register usage in multiplies
bool isIllegalMultiply(uint8_t rd, uint8_t rn, uint8_t rs, uint8_t rm,
                       bool isLong) {
  // Check if any register is R15 (Forbidden for all multiply ops)
  if (rd == 15 || rn == 15 || rs == 15 || rm == 15)
    return true;

  // Check for Rd == Rm (Forbidden for all multiply ops)
  if (rd == rm)
    return true;

  // Additional checks for Long Multiplies
  if (isLong) {
    if (rd == rn)
      return true; // RdHi must not equal RdLo
    if (rn == rm)
      return true; // RdLo must not equal Rm
  }
  return false;
}

// Helper function to set flags for multiply
void ARMOps::setMultiFlag(ARM7TDMI &cpu, uint8_t rd, uint64_t longResult,
                          bool isLong) {
  uint32_t nFlag;
  uint32_t zFlag;
  if (!isLong) {
    nFlag = (cpu.getLogicalRegister(rd) >> 31) & 0x00000001;
    zFlag = (cpu.getLogicalRegister(rd) == 0) ? 0x00000001 : 0x00000000;
  } else {
    nFlag = static_cast<uint32_t>((longResult >> 63)) & 0x00000001;
    zFlag = (longResult == 0) ? 0x00000001 : 0x00000000;
  }
  cpu.cpsr = (cpu.cpsr & 0x3FFFFFFF) | (nFlag << 31) | (zFlag << 30);
}

void ARMOps::muliply(ARM7TDMI &cpu, uint32_t instruction) {
  uint8_t opcode = (instruction >> 21) & 0x0F;
  // Set condition
  uint8_t s = (instruction >> 20) & 0x01;
  // Destination Register (R0-R14) (also RdHi)
  uint8_t rd = (instruction >> 16) & 0x0F;
  // Accumulate REgister (R0-R14, set to 0000b if unused) (also RdLo)
  uint8_t rn = (instruction >> 12) & 0x0F;
  // Operand Register (R0-R14)
  uint8_t rs = (instruction >> 8) & 0x0F;
  // Operand Register (R0-R14)
  uint8_t rm = instruction & 0x0F;
  // whether long multiplication is executed
  bool isLong;

  uint32_t rnVal = cpu.getLogicalRegister(rn);
  uint32_t rsVal = cpu.getLogicalRegister(rs);
  uint32_t rmVal = cpu.getLogicalRegister(rm);
  uint32_t rdVal = cpu.getLogicalRegister(rd);
  uint64_t rdHiLoVal =
      ((static_cast<uint64_t>(rdVal) << 32) | (static_cast<uint64_t>(rnVal)));
  uint32_t result;
  uint64_t longResult;
  uint32_t loResult;
  uint32_t hiResult;
  int64_t signedLongResult;
  switch (opcode) {
  case 0x00:
    // Check registers for legality
    isLong = false;
    if (isIllegalMultiply(rd, rn, rs, rm, isLong)) {
      std::cerr << "Register Usage Error!" << std::endl;
      return;
    }
    // MUL{cond}{s} Rd, Rm, Rs; Rd=Rm*Rs
    result = rmVal * rsVal;
    cpu.setLogicalRegister(rd, result);
    if (s == 1) {
      ARMOps::setMultiFlag(cpu, rd, longResult, isLong);
    }
    break;
  case 0x01:
    // Check registers for legality
    isLong = false;
    if (isIllegalMultiply(rd, rn, rs, rm, isLong)) {
      std::cerr << "Register Usage Error!" << std::endl;
      return;
    }
    // MLA{cond}{s} Rd, Rm, Rs, Rn; Rd=Rm*Rs+Rn
    result = rmVal * rsVal + rnVal;
    cpu.setLogicalRegister(rd, result);
    if (s == 1) {
      ARMOps::setMultiFlag(cpu, rd, longResult, isLong);
    }
    break;
  case 0x04:
    // Check registers for legality
    isLong = true;
    if (isIllegalMultiply(rd, rn, rs, rm, isLong)) {
      std::cerr << "Register Usage Error!" << std::endl;
      return;
    }
    // UMULL{cond}{s} RdLo, RdHi, Rm, Rs; RdHiLo=Rm*Rs
    longResult = static_cast<uint64_t>(rmVal) * static_cast<uint64_t>(rsVal);
    // Set RdHi
    hiResult = static_cast<uint32_t>((longResult >> 32) & 0xFFFFFFFF);
    cpu.setLogicalRegister(rd, hiResult);
    // Set RdLo
    loResult = static_cast<uint32_t>(longResult & 0xFFFFFFFF);
    cpu.setLogicalRegister(rn, loResult);
    if (s == 1) {
      ARMOps::setMultiFlag(cpu, rd, longResult, isLong);
    }
    break;
  case 0x05:
    // Check registers for legality
    isLong = true;
    if (isIllegalMultiply(rd, rn, rs, rm, isLong)) {
      std::cerr << "Register Usage Error!" << std::endl;
      return;
    }
    // UMLAL{cond}{s} RdLo, RdHi, Rm, Rs; RdHiLo = RdHiLo=Rm*Rs+RdHiLo
    longResult =
        static_cast<uint64_t>(rmVal) * static_cast<uint64_t>(rsVal) + rdHiLoVal;
    // Set RdHi
    hiResult = static_cast<uint32_t>((longResult >> 32) & 0xFFFFFFFF);
    cpu.setLogicalRegister(rd, hiResult);
    // Set RdLo
    loResult = static_cast<uint32_t>(longResult & 0xFFFFFFFF);
    cpu.setLogicalRegister(rn, loResult);
    if (s == 1) {
      ARMOps::setMultiFlag(cpu, rd, longResult, isLong);
    }
    break;
  case 0x06:
    // Check registers for legality
    isLong = true;
    if (isIllegalMultiply(rd, rn, rs, rm, isLong)) {
      std::cerr << "Register Usage Error!" << std::endl;
      return;
    }
    // SMULL{cond}{s} RdLo, RdHi, Rm, Rs; RdHiLo=Rm*Rs (signed)
    signedLongResult = static_cast<int64_t>(static_cast<int32_t>(rmVal)) *
                       static_cast<int64_t>(static_cast<int32_t>(rsVal));
    // Cast back to unsigned int to store into registers
    longResult = static_cast<uint64_t>(signedLongResult);
    // Set RdHi
    hiResult = static_cast<uint32_t>((longResult >> 32) & 0xFFFFFFFF);
    cpu.setLogicalRegister(rd, hiResult);
    // Set RdLo
    loResult = static_cast<uint32_t>(longResult & 0xFFFFFFFF);
    cpu.setLogicalRegister(rn, loResult);
    if (s == 1) {
      ARMOps::setMultiFlag(cpu, rd, longResult, isLong);
    }
    break;
  case 0x07:
    // Check registers for legality
    isLong = true;
    if (isIllegalMultiply(rd, rn, rs, rm, isLong)) {
      std::cerr << "Register Usage Error!" << std::endl;
      return;
    }
    // SMLAL{cond}{s} RdLo, RdHi, Rm, Rs; RdHiLo=Rm*Rs+RdHiLo (signed)
    signedLongResult = static_cast<int64_t>(static_cast<int32_t>(rmVal)) *
                           static_cast<int64_t>(static_cast<int32_t>(rsVal)) +
                       static_cast<int64_t>(rdHiLoVal);
    // Cast back to unsigned int to store into registers
    longResult = static_cast<uint64_t>(signedLongResult);
    // Set RdHi
    hiResult = static_cast<uint32_t>((longResult >> 32) & 0xFFFFFFFF);
    cpu.setLogicalRegister(rd, hiResult);
    // Set RdLo
    loResult = static_cast<uint32_t>(longResult & 0xFFFFFFFF);
    cpu.setLogicalRegister(rn, loResult);
    if (s == 1) {
      ARMOps::setMultiFlag(cpu, rd, longResult, isLong);
    }
    break;
  default:
    std::cerr << "Undefined Multiply Opcode Format" << std::endl;
    return;
  }
}

void ARMOps::singleDataSwap(ARM7TDMI &cpu, uint32_t instruction) {
  // SWP{cond}{B} Rd, Rm, [Rn]; Rd=[Rn]=Rm, [Rn]=Rm
  // Byte/word bit: 0=swap 32b word, 1 = swap 8b byte
  uint8_t b = (instruction >> 22) & 0x01;
  // Base reg
  uint8_t rn = (instruction >> 16) & 0x0F;
  // Destination reg
  uint8_t rd = (instruction >> 12) & 0x0F;
  // Source reg
  uint8_t rm = instruction & 0x0F;

  // Register check
  if (rn == 15 || rd == 15 || rm == 15) {
    std::cerr << "Register Usage Error!" << std::endl;
    return;
  }
  // Instruction legality check
  if ((instruction & 0x0FB00FF0) != 0x01000090) {
    std::cerr << "Undefined Instruction!" << std::endl;
    return;
  }

  // Address
  uint32_t rnAddress = cpu.getLogicalRegister(rn);
  // Value stored in rm
  uint32_t rmVal = cpu.getLogicalRegister(rm);
  if (!b) {
    // SWP
    // Get word alligned address by clearing bottom 2 bits and determining if
    // there will be a shift
    uint32_t wordAlignedAddr = rnAddress & ~0x03;
    uint32_t shift = (rnAddress & 0x03) * 8;
    uint32_t oldVal = cpu.memoryBus.read32(wordAlignedAddr);
    // If it was shifted, handle unaligned read rotation, this was a notorious
    // ARM7TDMI BUG
    if (shift != 0) {
      oldVal = (oldVal >> shift) | (oldVal << (32 - shift));
    }
    // write rmVal to rnAddress
    cpu.memoryBus.write32(rnAddress, rmVal);
    // put old value in rd
    cpu.setLogicalRegister(rd, oldVal);
  } else {
    // SWPB
    uint32_t oldVal = cpu.memoryBus.read8(rnAddress);
    // write the lowest byte of rmVal to memory
    cpu.memoryBus.write8(rnAddress, rmVal & 0xFF);
    // Place the zero-expanded byte into rd
    oldVal = oldVal & 0x000000FF;
    cpu.setLogicalRegister(rd, oldVal);
  }
}

void ARMOps::halfwordDataTransReg(ARM7TDMI &cpu, uint32_t instruction) {
  // Pre/Post(0 add offset POST transfer, 1 add offset PRE transfer)
  uint8_t p = (instruction >> 24) & 0x01;
  // Up/Down (0=down, subtarct offset from base. 1=up, add to base)
  uint8_t u = (instruction >> 23) & 0x01;
  // i bit (22 bit) will be 0 since it is REG offset
  // Write-back bit (0=no write, 1=write address into base)
  uint8_t w = (instruction >> 21) & 0x01;
  // Load/store bit, 0 = store 1 = load
  uint8_t l = (instruction >> 20) & 0x01;
  // base register
  uint8_t rn = (instruction >> 16) & 0x0F;
  // source/destination register
  uint8_t rd = (instruction >> 12) & 0x0F;
  // Opcode
  uint8_t opcode = (instruction >> 5) & 0x03;
  // offset register
  uint8_t rm = instruction & 0x0F;

  // Instruction legality check:
  if ((instruction & 0x0E400F90) != 0x00000090) {
    std::cerr << "Undefined Instruction!" << std::endl;
    return;
  }
  if (p == 0 && w != 0) {
    std::cerr << "Halfword Data Transfer Instruction Error, when p bit 0 w bit "
                 "will be unused.";
    w = 0;
  }
  if (rm == 15) {
    std::cerr << "Register Usage Error!" << std::endl;
    return;
  }

  // Offset calculation
  uint32_t offset = cpu.getLogicalRegister(rm);
  uint32_t baseAddr = cpu.getLogicalRegister(rn);
  uint32_t effectiveAddr = (u == 1) ? (baseAddr + offset) : (baseAddr - offset);
  // Pre/post indexing
  uint32_t transferAddr = (p == 1) ? effectiveAddr : baseAddr;
  // Decode opcode
  switch (opcode) {
  case 0x01:
    if (l == 0) {
      // STR{cond}H rd,<address>; [a]=rd
      // Lower 16 bits of Rd
      uint32_t rdVal = cpu.getLogicalRegister(rd) & 0xFFFF;
      cpu.memoryBus.write16(transferAddr, rdVal);
    } else {
      // LDR{cond}H rd,<address>; load unsigned halfword (0 extended)
      uint32_t val = cpu.memoryBus.read16(transferAddr);
      cpu.setLogicalRegister(rd, val);
    }
    break;
  case 0x02:
    if (l == 0) {
      // Load double word does not exist on ARM7TDMI
      std::cerr << "LDRD not supported on ARM7TDMI" << std::endl;
      return;
    } else {
      // LDR{cond}B rd, <address>; load signed byte (sign extended)
      uint8_t val = cpu.memoryBus.read8(transferAddr);
      uint32_t signExtended = (val & 0x80) ? (val | 0xFFFFFF00) : val;
      cpu.setLogicalRegister(rd, signExtended);
    }
    break;
  case 0x03:
    if (l == 0) {
      // Store double word does not exist on ARM7TDMI
      std::cerr << "STRD not supported on ARM7TDMI" << std::endl;
      return;
    } else {
      // LDR{cond}SH rd, <address>; load signed halfword (sign extended)
      uint16_t val = cpu.memoryBus.read16(transferAddr);
      uint32_t signExtended = (val & 0x8000) ? (val | 0xFFFF0000) : val;
      cpu.setLogicalRegister(rd, signExtended);
    }
    break;
  default:
    std::cerr << "Undefined Data Transfer Opcode Format" << std::endl;
    return;
  }

  // Write back if P=0 or W=1
  if (((p == 0) || (w == 1)) && (rn != 15)) {
    cpu.setLogicalRegister(rn, effectiveAddr);
  }
}

void ARMOps::halfwordDataTransImm(ARM7TDMI &cpu, uint32_t instruction) {
  // Pre/Post(0 add offset POST transfer, 1 add offset PRE transfer)
  uint8_t p = (instruction >> 24) & 0x01;
  // Up/Down (0=down, subtarct offset from base. 1=up, add to base)
  uint8_t u = (instruction >> 23) & 0x01;
  // i bit (22 bit) will be 1 since it is Imm offset
  // Write-back bit (0=no write, 1=write address into base)
  uint8_t w = (instruction >> 21) & 0x01;
  // Load/store bit, 0 = store 1 = load
  uint8_t l = (instruction >> 20) & 0x01;
  // base register
  uint8_t rn = (instruction >> 16) & 0x0F;
  // source/destination register
  uint8_t rd = (instruction >> 12) & 0x0F;
  // Imm offset High
  uint8_t immHi = (instruction >> 8) & 0x0F;
  // Opcode
  uint8_t opcode = (instruction >> 5) & 0x03;
  // Imm offset Lo
  uint8_t immLo = instruction & 0x0F;

  // Instruction legality check:
  if ((instruction & 0x0E400090) != 0x00400090) {
    std::cerr << "Undefined Instruction!" << std::endl;
    return;
  }
  if (p == 0 && w != 0) {
    std::cerr << "Halfword Data Transfer Instruction Error, when p bit 0 w bit "
                 "will be unused.";
    w = 0;
  }

  // Calcutate offset
  uint32_t offset = (immHi << 4) | immLo;
  uint32_t baseAddr = cpu.getLogicalRegister(rn);
  uint32_t effectiveAddr = (u == 1) ? (baseAddr + offset) : (baseAddr - offset);
  uint32_t transferAddr = (p == 1) ? effectiveAddr : baseAddr;

  // Decode opcode
  switch (opcode) {
  case 0x01:
    if (l == 0) {
      // STR{cond}H rd,<address>; [a]=rd
      // Lower 16 bits of Rd
      uint32_t rdVal = cpu.getLogicalRegister(rd) & 0xFFFF;
      cpu.memoryBus.write16(transferAddr, rdVal);
    } else {
      // LDR{cond}H rd,<address>; load unsigned halfword (0 extended)
      uint32_t val = cpu.memoryBus.read16(transferAddr);
      cpu.setLogicalRegister(rd, val);
    }
    break;
  case 0x02:
    if (l == 0) {
      // Load double word does not exist on ARM7TDMI
      std::cerr << "LDRD not supported on ARM7TDMI" << std::endl;
      return;
    } else {
      // LDR{cond}B rd, <address>; load signed byte (sign extended)
      uint8_t val = cpu.memoryBus.read8(transferAddr);
      uint32_t signExtended = (val & 0x80) ? (val | 0xFFFFFF00) : val;
      cpu.setLogicalRegister(rd, signExtended);
    }
    break;
  case 0x03:
    if (l == 0) {
      // Store double word does not exist on ARM7TDMI
      std::cerr << "STRD not supported on ARM7TDMI" << std::endl;
      return;
    } else {
      // LDR{cond}SH rd, <address>; load signed halfword (sign extended)
      uint16_t val = cpu.memoryBus.read16(transferAddr);
      uint32_t signExtended = (val & 0x8000) ? (val | 0xFFFF0000) : val;
      cpu.setLogicalRegister(rd, signExtended);
    }
    break;
  default:
    std::cerr << "Undefined Data Transfer Opcode Format" << std::endl;
    return;
  }

  // Write-back only occurs if P=0 or W=1
  if (((p == 0) || (w == 1)) && (rn != 15)) {
    cpu.setLogicalRegister(rn, effectiveAddr);
  }
}

void ARMOps::branchAndExchange(ARM7TDMI &cpu, uint32_t instruction) {
  uint8_t opcode = (instruction >> 4) & 0x0F;
  // Operand Register (R0-R14)
  uint8_t rn = instruction & 0x0F;
  if (rn == 15) {
    std::cerr << "Register Usage Error!" << std::endl;
    return;
  }

  uint32_t rnVal = cpu.getLogicalRegister(rn);
  uint8_t t;
  const uint8_t pc = 15;

  switch (opcode) {
  case 0x01:
    // BX{cond} Rn; PC=Rn, T=Rn.0 (ARMv4T)
    t = rnVal & 0x01;
    if (t) {
      // Set t bit to 1 to switch to THUMB
      cpu.cpsr = cpu.cpsr | (0x20);
      // Set alignment to halfword
      cpu.setLogicalRegister(pc, (rnVal & ~0x01));
    } else {
      // Clear the t bit
      cpu.cpsr = cpu.cpsr & ~(0x20);
      // Align program counter to word (32 bit);
      cpu.setLogicalRegister(pc, (rnVal & ~0x03));
    }

    // Branched to new address, refresh pipeline
    cpu.flushPipeline();
    cpu.fillPipeline();
    break;
  case 0x03:
    // BLX does not exist on ARM7TDMI
    std::cerr << "BLX Instruction is not supported on ARM7TDMI!" << std::endl;
    return;
  default:
    std::cerr << "Undefined BX Opcode Format!" << std::endl;
    return;
  }
}

void ARMOps::MRS(ARM7TDMI &cpu, uint32_t instruction) {}

void ARMOps::MSR(ARM7TDMI &cpu, uint32_t instruction) {}

void ARMOps::ALU(ARM7TDMI &cpu, uint32_t instruction) {}

void ARMOps::loadStoreWBImm(ARM7TDMI &cpu, uint32_t instruction) {}

void ARMOps::loadStoreWBReg(ARM7TDMI &cpu, uint32_t instruction) {}

void ARMOps::blockDataTransfer(ARM7TDMI &cpu, uint32_t instruction) {}

void ARMOps::branch(ARM7TDMI &cpu, uint32_t instruction) {}

void ARMOps::coprocessorDataTransfer(ARM7TDMI &cpu, uint32_t instruction) {}

void ARMOps::coprocessorDataOPP(ARM7TDMI &cpu, uint32_t instruction) {}

void ARMOps::coprocessorRegTransfer(ARM7TDMI &cpu, uint32_t instruction) {}

void ARMOps::SWI(ARM7TDMI &cpu, uint32_t instruction) {}
