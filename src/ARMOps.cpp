#include "ARMOps.h"
#include "ALUHelpers.h"
#include "ARM7TDMI.h"
#include "memoryBus.h"
#include <cstdint>
#include <iostream>
#include <type_traits>

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
    std::cerr << "Undefined Instruction! (singleDataSwap)" << std::endl;
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
    std::cerr << "Undefined Instruction! (halfwordDataTransReg)" << std::endl;
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
      // Increment by 4 (pc + 12) if rd were pc
      if (rd == 15) {
        rdVal += 4;
      }
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
    std::cerr << "Undefined Instruction! (halfwordDataTransImm)" << std::endl;
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
      if (rd == 15) {
        rdVal += 4;
      }
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
      rnVal &= ~0x01;
    } else {
      // Clear the t bit
      cpu.cpsr = cpu.cpsr & ~(0x20);
      rnVal &= ~0x03;
    }

    // Branched to new address, refresh pipeline
    cpu.forceJump(rnVal);
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

void ARMOps::MRS(ARM7TDMI &cpu, uint32_t instruction) {
  // Immediate Operand Flag (bit 25, 0 for MRS)

  // Source/destination PSR (0=cpsr, 1=spsr_<current_mode>)
  uint8_t psr = (instruction >> 22) & 0x01;
  // Opcode (bit 21): 0 for MRS and 1 for MSR

  // Destination Register (R0-R14)
  uint8_t rd = (instruction >> 12) & 0x0F;

  // legality check
  if ((instruction & 0x0FBF0FFF) != 0x010F0000) {
    std::cerr << "Undefined Instruction! (MRS)" << std::endl;
    return;
  }
  if (rd == 15) {
    std::cerr << "Register Usage Error!" << std::endl;
    return;
  }

  // Rd = Psr
  uint32_t value = (psr == 0) ? cpu.getCPSR() : cpu.getCurrentSPSR();
  cpu.setLogicalRegister(rd, value);
}

void ARMOps::MSR(ARM7TDMI &cpu, uint32_t instruction) {
  // Immediate Operand Flag
  uint8_t i = (instruction >> 25) & 0x01;
  // source/destination PSR (0=cpsr, 1=spsr_<current_mode>)
  uint8_t psr = (instruction >> 22) & 0x01;
  // Opcode (bit 21): 0 for MRS and 1 for MSR

  // Generate the write mask based on the f, s, x, c bits
  uint32_t mask = 0;
  if (instruction & (1 << 19))
    mask |= 0xFF000000; // f: flags field
  if (instruction & (1 << 18))
    mask |= 0x00FF0000; // s: status field
  if (instruction & (1 << 17))
    mask |= 0x0000FF00; // x: extension field
  if (instruction & (1 << 16))
    mask |= 0x000000FF; // c: control field

  uint32_t op = 0;

  // Psr[field] = Op
  if (!i) {
    // MSR Psr, Rm

    // legality check
    if ((instruction & 0x0DB0FFF0) != 0x0120F000) {
      std::cerr << "Undefined Instruction! (MSR)" << std::endl;
      return;
    }

    // Source register <op> (R0-R14)
    uint8_t rm = instruction & 0x0F;
    if (rm == 15) {
      std::cerr << "Register Usage Error!" << std::endl;
      return;
    }
    op = cpu.getLogicalRegister(rm);

  } else {
    // MSR Psr, Imm

    // legality check
    if ((instruction & 0x0DB0F000) != 0x0120F000) {
      std::cerr << "Undefined Instruction! (MSR)" << std::endl;
      return;
    }

    // Shift applied to Imm (ROR in steps of two 0-30)
    uint8_t shift = (instruction >> 8) & 0x0F;
    // Unsigned 8bit Immediate
    uint32_t imm = instruction & 0x0FF;

    if (!shift) {
      op = imm;
    } else {
      uint8_t shiftAmount = shift * 2;
      op = (imm >> shiftAmount) | (imm << (32 - shiftAmount));
    }
  }
  uint8_t currentMode = cpu.getCPSR() & 0x1F;
  bool isPrivileged = (currentMode != static_cast<uint8_t>(CpuMode::User));

  if (psr == 0) {
    // Write to CPSR
    // In user mode, only the condition code flags (bit 31-24) can be changed
    if (!isPrivileged) {
      mask &= 0xFF000000;
    }

    // The T-bit (bit 5) may never be changed via MSR
    mask &= 0xF00000DF;

    uint32_t newCpsr = (cpu.getCPSR() & ~mask) | (op & mask);
    cpu.cpsr = newCpsr;
  } else {
    // Write to SPSR
    if (currentMode == static_cast<uint8_t>(CpuMode::User) ||
        currentMode == static_cast<uint8_t>(CpuMode::System)) {
      std::cerr << "Cannot access SPSR in User or System mode!" << std::endl;
      return;
    }
    uint32_t newSpsr = (cpu.getCurrentSPSR() & ~mask) | (op & mask);
    cpu.getCurrentSPSR() = newSpsr;
  }
}

// // Helper for shift type
// struct shiftResult {
//   uint32_t value;
//   uint8_t carry;
//   bool carryUpdated;
// };
//
// shiftResult shiftOperand(uint32_t value, uint8_t shiftType, uint8_t amount,
//                          uint8_t oldCarry, bool byRegister) {
//   shiftResult out{};
//   out.value = value;
//   out.carry = oldCarry;
//   out.carryUpdated = true;
//
//   switch (shiftType) {
//   case 0x0:
//     // LSL
//     if (!amount) {
//       out.carryUpdated = false;
//     } else if (amount < 32) {
//       out.carry = (value >> (32 - amount)) & 0x01;
//       out.value = value << amount;
//     } else {
//       out.carry = 0;
//       out.value = 0;
//     }
//     break;
//   case 0x1:
//     // LSR
//     if (!amount && !byRegister) {
//       amount = 32;
//     }
//     if (amount < 32) {
//       out.carry = (value >> (amount - 1)) & 0x01;
//       out.value = value >> amount;
//     } else {
//       out.carry = (value >> 31) & 0x01;
//       out.value = 0;
//     }
//     break;
//   case 0x2:
//     // ASR
//     if (!amount && !byRegister) {
//       amount = 32;
//     }
//     if (amount < 32) {
//       out.carry = (value >> (amount - 1)) & 1;
//       out.value = static_cast<int32_t>(value) >> amount;
//     } else {
//       out.carry = value >> 31;
//       out.value = (value & 0x80000000) ? 0xFFFFFFFF : 0x00000000;
//     }
//     break;
//   case 0x3:
//     // ROR / RRX
//     if (!amount && !byRegister) {
//       // RRX
//       out.value = (oldCarry << 31) | (value >> 1);
//       out.carry = value & 0x01;
//     } else {
//       amount &= 0x1F;
//       out.value = (value >> amount) | (value << (32 - amount));
//       out.carry = out.value >> 31;
//     }
//     break;
//   }
//   return out;
// }

void ARMOps::ALU(ARM7TDMI &cpu, uint32_t instruction) {
  // Immediate 2nd Operand flag (0=register, 1=Immediate)
  uint8_t i = (instruction >> 25) & 0x01;
  // Opcode
  uint8_t opcode = (instruction >> 21) & 0x0F;
  // Set condition code (0=No, 1=yes, must be 1 for opcode 8-B)
  uint8_t s = (instruction >> 20) & 0x01;
  // 1st Operand Reg (R0-R15)
  uint8_t rn = (instruction >> 16) & 0x0F;
  // Destination Register (R0-R15)
  uint8_t rd = (instruction >> 12) & 0x0F;

  // Legality check
  if ((instruction & 0x0C000000) != 0x00000000) {
    std::cerr << "Undefined Instruction! (ALU)" << std::endl;
    return;
  }
  if (s == 0 && opcode >= 0x8 && opcode <= 0xB) {
    std::cerr << "ALU Legality Error: S=0 on test instruction!" << std::endl;
    return;
  }

  uint32_t op2 = 0;
  uint8_t carryOut = (cpu.getCPSR() >> 29) & 0x01;

  // PC+12 if shift by reg (i=0, r=1), otherwise PC+8

  // Shift by Reg flag (0=Immediate, 1=Register)
  uint8_t r = (!i) ? (instruction >> 4) & 0x01 : 0;
  // because PC is already 2 steps forward (4 increments per step), take away 8
  // from pcOffset
  uint32_t pcOffset = (!i && r) ? 4 : 0;
  uint32_t rnVal = (rn == 15) ? cpu.getLogicalRegister(15) + pcOffset
                              : cpu.getLogicalRegister(rn);
  uint8_t oldCarry = (cpu.getCPSR() >> 29) & 0x01;
  if (!i) {
    // Register as 2nd Operation

    // Shift Type (0=LSL, 1=LSR, 2=ASR, 3=ROR)
    uint8_t shiftType = (instruction >> 5) & 0x03;
    // 2nd Operand register (R0-R15)
    uint8_t rm = instruction & 0x0F;
    uint32_t rmVal = (rm == 15) ? cpu.getLogicalRegister(15) + pcOffset
                                : cpu.getLogicalRegister(rm);
    // Shift amount, (1-31, 0 is special case)
    uint8_t shiftAmount = 0;

    if (!r) {
      // Shift by Immeidate

      shiftAmount = (instruction >> 7) & 0x1F;
      // Handle shift type
      ALUHelper::shiftResult sr = ALUHelper::shiftOperand(
          rmVal, shiftType, shiftAmount, oldCarry, false);
      op2 = sr.value;
      if (sr.carryUpdated) {
        carryOut = sr.carry;
      }
    } else {
      // Shift by Register

      // Legality for bit 7 - must be 0
      if (((instruction >> 7) & 0x01) != 0x00) {
        std::cerr << "Undefined Instruction! (ALU2)" << std::endl;
        return;
      }
      // Shift register (R0-R14) (Only lower 8bit 0-255 used)
      uint8_t rs = (instruction >> 8) & 0x0F;

      // Legality check
      if (rs == 15) {
        std::cerr << "ALU Legality Error: Rs cannot be R15!" << std::endl;
        return;
      }
      uint32_t rsVal = cpu.getLogicalRegister(rs);
      shiftAmount = rsVal & 0xFF; // Only take lower 8 bits

      // Apply shift type
      ALUHelper::shiftResult sr = ALUHelper::shiftOperand(
          rmVal, shiftType, shiftAmount, oldCarry, true);
      op2 = sr.value;
      carryOut = sr.carry;
    }

  } else {
    // Immediate as 2nd Operand

    // ROR Shift applied to nn (0-30, in steps of 2)
    uint8_t is = (instruction >> 8) & 0x0F;
    // 2nd Operand Unsigned 8Bit Immediate
    uint8_t nn = instruction & 0x0FF;
    uint8_t shift = is * 2;
    uint32_t imm = nn;
    if (!shift) {
      op2 = imm;
    } else {
      op2 = (imm >> shift) | (imm << (32 - shift));
      carryOut = (op2 >> 31) & 0x01; // Carry out is bit 31 of the result
    }
  }

  // Perform ALU Operations
  uint32_t result = 0;
  bool writeBack = true;
  bool isLogical = false;
  bool vFlag = (cpu.getCPSR() >> 28) & 1; // Overflow flag
  uint64_t diff = 0;
  uint64_t sum = 0;
  switch (opcode) {
  case 0x0:
    // AND
    result = rnVal & op2;
    isLogical = true;
    break;
  case 0x1:
    // EOR
    result = rnVal ^ op2;
    isLogical = true;
    break;
  case 0x2:
    // SUB
    diff = static_cast<uint64_t>(rnVal) - static_cast<uint64_t>(op2);
    result = static_cast<uint32_t>(diff);
    carryOut = (rnVal >= op2);
    vFlag = ((rnVal ^ op2) & (rnVal ^ result)) >> 31;
    break;
  case 0x3:
    // RSB
    diff = static_cast<uint64_t>(op2) - static_cast<uint64_t>(rnVal);
    result = static_cast<uint64_t>(diff);
    carryOut = (op2 >= rnVal);
    vFlag = ((op2 ^ rnVal) & (op2 ^ result)) >> 31;
    break;
  case 0x4:
    // ADD
    sum = static_cast<uint64_t>(rnVal) + static_cast<uint64_t>(op2);
    result = static_cast<uint32_t>(sum);
    carryOut = (sum >> 32) & 0x01;
    vFlag = (~(rnVal ^ op2) & (rnVal ^ result)) >> 31;
    break;
  case 0x5:
    // ADC
    sum = static_cast<uint64_t>(rnVal) + static_cast<uint64_t>(op2) +
          static_cast<uint64_t>(carryOut);
    result = static_cast<uint32_t>(sum);
    carryOut = (sum >> 32) & 1;
    vFlag = (~(rnVal ^ op2) & (rnVal ^ result)) >> 31;
    break;
  case 0x6:
    // SBC
    diff = static_cast<uint64_t>(rnVal) - static_cast<uint64_t>(op2) +
           static_cast<uint64_t>(carryOut) - 1;
    result = static_cast<uint32_t>(diff);
    carryOut = static_cast<uint64_t>(rnVal) >=
               static_cast<uint64_t>(op2) + (carryOut ? 0 : 1);
    vFlag = ((rnVal ^ op2) & (rnVal ^ result)) >> 31;
    break;
  case 0x7:
    // RSC
    diff = static_cast<uint64_t>(op2) - static_cast<uint64_t>(rnVal) +
           static_cast<uint64_t>(carryOut) - 1;
    result = static_cast<uint32_t>(diff);
    carryOut = ((static_cast<uint64_t>(op2)) >=
                (static_cast<uint64_t>(rnVal) + (carryOut ? 0 : 1)));
    vFlag = ((op2 ^ rnVal) & (op2 ^ result)) >> 31;
    break;
  case 0x8:
    // TST
    result = rnVal & op2;
    isLogical = true;
    writeBack = false;
    break;
  case 0x9:
    // TEQ
    result = rnVal ^ op2;
    isLogical = true;
    writeBack = false;
    break;
  case 0xA:
    // CMP
    diff = static_cast<uint64_t>(rnVal) - static_cast<uint64_t>(op2);
    result = static_cast<uint32_t>(diff);
    carryOut = (rnVal >= op2);
    vFlag = ((rnVal ^ op2) & (rnVal ^ result)) >> 31;
    writeBack = false;
    break;
  case 0xB:
    // CMN
    sum = static_cast<uint64_t>(rnVal) + static_cast<uint64_t>(op2);
    result = static_cast<uint32_t>(sum);
    carryOut = (sum >> 32) & 0x01;
    vFlag = (~(rnVal ^ op2) & (rnVal ^ result)) >> 31;
    writeBack = false;
    break;
  case 0xC:
    // ORR
    result = rnVal | op2;
    isLogical = true;
    break;
  case 0xD:
    // MOV
    result = op2;
    isLogical = true;
    break;
  case 0xE:
    // BIC
    result = rnVal & ~op2;
    isLogical = true;
    break;
  case 0xF:
    // MVN
    result = ~op2;
    isLogical = true;
    break;
  }

  // Write back
  if (writeBack) {
    cpu.setLogicalRegister(rd, result);
    if (rd == 15) {
      cpu.flushPipeline();
    }
  }

  // Update CPSR if S bit is set
  if (s) {
    if (rd == 15) {
      // S==1 and Rd==15 restores SPSR to CPSR
      uint32_t spsr = cpu.getCurrentSPSR();
      cpu.setLogicalRegister(15, result);
      cpu.cpsr = spsr;
      cpu.updateProcessorMode(spsr & 0x1F);
      cpu.flushPipeline();
    } else {
      uint32_t newCpsr = cpu.getCPSR();

      // Update N and Z (bit 31 and 30)
      newCpsr = ((newCpsr & 0x3FFFFFFF) |
                 ((result & 0x80000000) ? 0x80000000 : 0x00000000));
      newCpsr |= (!result) ? 0x40000000 : 0x00000000;

      // Update C (bit 29)
      newCpsr = ((newCpsr & 0xDFFFFFFF) | (carryOut ? 0x20000000 : 0x00000000));

      // Update V (bit 28) - only for arithmetic
      if (!isLogical) {
        newCpsr = ((newCpsr & 0xEFFFFFFF) | (vFlag ? 0x10000000 : 0x00000000));
      }

      cpu.cpsr = newCpsr;
    }
  }
}

void ARMOps::loadStoreWBImm(ARM7TDMI &cpu, uint32_t instruction) {
  // Immediate Offset Flag (bit 25) will be 0 here since it is immediate

  // Pre/Pose (0=post, add offset after transfer, 1=pre, add to base)
  uint8_t p = (instruction >> 24) & 0x01;
  // Up/down bit (0=down/subtract from base, 1=up/add to base)
  uint8_t u = (instruction >> 23) & 0x01;
  // Byte/word bit (0=32bit word, 1=transfer 8bit byte)
  uint8_t b = (instruction >> 22) & 0x01;
  // Memory Mangement/writeback (depending on whether p is 1 or 0, p == 0
  // writeback always true)
  uint8_t twBit = (instruction >> 21) & 0x01;
  uint8_t l = (instruction >> 20) & 0x01;
  // Base reg (R0-R15, including R15=pc+8)
  uint8_t rn = (instruction >> 16) & 0x0F;
  // Destination reg (R0-R15 including R15=pc+12)
  uint8_t rd = (instruction >> 12) & 0x0F;
  // Unsigned 12 bit immediate offset (0-4095)
  uint32_t immOffset = instruction & 0x0FFF;

  // Legality check
  if ((instruction & 0x0E000000) != 0x04000000) {
    std::cerr << "Undefined Instruction! (loadStoreWBImm)" << std::endl;
    return;
  }
  // PC addr is already incremented by 8 by default
  uint32_t baseAddr = cpu.getLogicalRegister(rn);

  uint32_t effectiveAddr =
      (!u) ? (baseAddr - immOffset) : (baseAddr + immOffset);
  uint32_t transferAddr = (!p) ? baseAddr : effectiveAddr;

  if (!l) {
    // STR: Store to memory
    uint32_t rdVal = cpu.getLogicalRegister(rd);
    if (rd == 15) {
      rdVal += 4;
    }

    if (!b) {
      // Store word

      // Force word alignment
      cpu.memoryBus.write32(transferAddr & ~0x03, rdVal);
    } else {
      // Store byte (don't care about alignment)
      cpu.memoryBus.write8(transferAddr, rdVal & 0x00FF);
    }
  } else {
    // LDR: Load from Memory
    uint32_t val = 0;

    if (!b) {
      // Load word (with misaligned rotated read support)
      uint32_t wordAlignedAddr = transferAddr & ~0x03;
      // How many bits are shifted
      uint32_t shift = (transferAddr & 0x03) * 8;
      val = cpu.memoryBus.read32(wordAlignedAddr);

      if (shift != 0) {
        val = (val >> shift) | (val << (32 - shift));
      }
    } else {
      // Load byte (upper 24 bits zero-extended)
      val = cpu.memoryBus.read8(transferAddr) & 0x000000FF;
    }

    cpu.setLogicalRegister(rd, val);

    // If pc was loaded into, flush the pipeline
    if (rd == 15) {
      cpu.flushPipeline();
    }
  }

  // Writeback
  if (((!p) || (twBit == 1)) && (rn != 15)) {
    cpu.setLogicalRegister(rn, effectiveAddr);
  }
}

void ARMOps::loadStoreWBReg(ARM7TDMI &cpu, uint32_t instruction) {
  // Immediate Offset Flag (bit 25) will be 1 here since it is register

  // Pre/Pose (0=post, add offset after transfer, 1=pre, add to base)
  uint8_t p = (instruction >> 24) & 0x01;
  // Up/down bit (0=down/subtract from base, 1=up/add to base)
  uint8_t u = (instruction >> 23) & 0x01;
  // Byte/word bit (0=32bit word, 1=transfer 8bit byte)
  uint8_t b = (instruction >> 22) & 0x01;
  // Memory Mangement/writeback (depending on whether p is 1 or 0, p == 0
  // writeback always true)
  uint8_t twBit = (instruction >> 21) & 0x01;
  uint8_t l = (instruction >> 20) & 0x01;
  // Base reg (R0-R15, including R15=pc+8)
  uint8_t rn = (instruction >> 16) & 0x0F;
  // Destination reg (R0-R15 including R15=pc+12)
  uint8_t rd = (instruction >> 12) & 0x0F;
  // Shift amount (1-31, 0=special)
  uint8_t is = (instruction >> 7) & 0x1F;
  // Shift type (0=LSL, 1=LSR, 2=ASR, 3=ROR)
  uint8_t shiftType = (instruction >> 5) & 0x03;
  // offset register (R0-R14)
  uint8_t rm = instruction & 0x0F;

  // Legality check
  if ((instruction & 0x0E000010) != 0x06000000) {
    std::cerr << "Undefined Instruction! (loadStoreWBReg)" << std::endl;
    return;
  }
  if (rm == 15) {
    std::cerr
        << "Register Usage Error: Rm cannot be R15 in Shifted Register Offset!"
        << std::endl;
    return;
  }

  uint32_t baseAddr = cpu.getLogicalRegister(rn);

  uint32_t rmVal = cpu.getLogicalRegister(rm);
  uint8_t oldCarry = (cpu.getCPSR() >> 29) & 0x01;

  ALUHelper::shiftResult sr =
      ALUHelper::shiftOperand(rmVal, shiftType, is, oldCarry, false);
  uint32_t offset = sr.value;

  uint32_t effectiveAddr = (!u) ? (baseAddr - offset) : (baseAddr + offset);
  uint32_t transferAddr = (!p) ? baseAddr : effectiveAddr;

  if (l == 0) {
    // STR: Store to memory
    uint32_t rdVal = cpu.getLogicalRegister(rd);
    if (rd == 15) {
      rdVal += 4;
    }

    if (!b) {
      // Store word
      cpu.memoryBus.write32(transferAddr & ~0x03, rdVal);
    } else {
      // Store Byte
      cpu.memoryBus.write8(transferAddr, rdVal & 0x00FF);
    }
  } else {
    // LDR
    uint32_t val = 0;

    if (b == 0) {
      // Load word
      uint32_t wordAlignedAddr = transferAddr & ~0x03;
      uint32_t shift = (transferAddr & 0x03) * 8;
      val = cpu.memoryBus.read32(wordAlignedAddr);

      if (shift != 0) {
        val = (val >> shift) | (val << (32 - shift));
      }
    } else {
      // Load byte
      val = cpu.memoryBus.read8(transferAddr) & 0x000000FF;
    }

    cpu.setLogicalRegister(rd, val);

    if (rd == 15) {
      cpu.flushPipeline();
    }
  }

  if (((!p) || (twBit == 1)) && (rn != 15)) {
    cpu.setLogicalRegister(rn, effectiveAddr);
  }
}

void ARMOps::blockDataTransfer(ARM7TDMI &cpu, uint32_t instruction) {
  // pre/post (0=post, add offset after transfer, 1=pre, before transfer)
  uint8_t p = (instruction >> 24) & 0x01;
  // up/down bit (0=down, subtract, 1=up, add)
  uint8_t u = (instruction >> 23) & 0x01;
  // PSR & force user bit (0=no, 1=load PSR or force user mode)
  uint8_t s = (instruction >> 22) & 0x01;
  // Write back bit (0=no writeback, 1= wrtie address into base)
  uint8_t w = (instruction >> 21) & 0x01;
  // Load/store bit (0=store to memory, 1=load from memory)
  uint8_t l = (instruction >> 20) & 0x01;

  // Base register (R0-R14)
  uint8_t rn = (instruction >> 16) & 0x0F;
  // Register list (offset is meant to be the number of words specified in
  // rList)
  uint32_t rList = instruction & 0x0000FFFF;

  uint32_t baseAddr = cpu.getLogicalRegister(rn);

  // Count number of registers listed in the rList
  uint32_t numRegs = 0;
  for (int regIdx = 0; regIdx < 16; regIdx++) {
    if (rList & (0x1 << regIdx)) {
      numRegs++;
    }
  }

  // Handle empty list (GBA quirk)
  // R15 loaded and Rb=Rb+/-40h
  if (!rList) {
    rList = (0x01 << 15);
    numRegs = 16;
  }

  uint32_t offset = numRegs * 4;
  uint32_t startAddr = baseAddr;

  if (!u) {
    startAddr = (!p) ? baseAddr - offset + 4 : baseAddr - offset;
  } else {
    startAddr = (!p) ? baseAddr : baseAddr + 4;
  }

  uint32_t currentAddr = startAddr;

  bool userBankTransfer = false;
  if (s) {
    if (!l || !(rList & (0x1 << 15))) {
      userBankTransfer = true;
    }
  }

  // Is base the lowest in rList?
  bool baseIsFirst = true;
  bool loadedPC = false;
  for (int regIdx = 0; regIdx < 16; regIdx++) {
    if (rList & (0x1 << regIdx)) {
      if (l) {
        // LDM
        // Read word aligned Data
        uint32_t val = cpu.memoryBus.read32(currentAddr & ~0x03);

        if (userBankTransfer && regIdx != 15) {
          cpu.physicalRegisters[regIdx] = val;
        } else {
          cpu.setLogicalRegister(regIdx, val);
        }
        if (regIdx == 15) {
          loadedPC = true;
          if (s) {
            // LDM and R15 in list with s==1, restore SPSR to CPSR
            cpu.restoreCPSR();
          }
        }
      } else {
        // STM
        uint32_t val;
        if (regIdx == 15) {
          val = cpu.getLogicalRegister(15) + 4;
        } else if (userBankTransfer) {
          val = cpu.physicalRegisters[regIdx];
        } else {
          val = cpu.getLogicalRegister(regIdx);
        }

        // Write-back with Base included in Rlist
        if (regIdx == rn && w) {
          if (baseIsFirst) {
            val = baseAddr;
          } else {
            val = (u) ? baseAddr + offset : baseAddr - offset;
          }
        }

        cpu.memoryBus.write32((currentAddr & ~0x03), val);
      }

      baseIsFirst = false;
      currentAddr += 4;
    }
  }

  bool disableWriteback = false;

  if (userBankTransfer) {
    disableWriteback = true;
  }

  if (l && (rList & (0x1 << rn))) {
    disableWriteback = true;
  }

  if (w && !disableWriteback) {
    uint32_t finalAddr = (u) ? baseAddr + offset : baseAddr - offset;
    cpu.setLogicalRegister(rn, finalAddr);
  }

  if (loadedPC) {
    cpu.flushPipeline();
  }
}

void ARMOps::branch(ARM7TDMI &cpu, uint32_t instruction) {
  // 1=branch, PC=PC+8+nn*4, 0=branch/link, PC=PC+8+nn*4, LR=PC+4
  uint8_t opcode = (instruction >> 24) & 0x01;
  // Signed offset in steps of 4 (-32M..+32M)
  uint32_t nn = instruction & 0x00FFFFFF;
  int32_t signedOffset =
      (!((instruction >> 23) & 0x01)) ? nn : (0xFF000000 | nn);

  uint32_t pc = cpu.getLogicalRegister(15);

  if (opcode) {
    // Branch with link
    cpu.setLogicalRegister(14, pc - 4);
  }
  uint32_t newPC = pc + (signedOffset << 2);

  cpu.setLogicalRegister(15, newPC);

  cpu.flushPipeline();
}

void ARMOps::coprocessorDataTransfer(ARM7TDMI &cpu, uint32_t instruction) {
  // um, coprocessors don't exist on the GBA actually
  std::cerr << "Coprocessor Data Transfer does not exist on the GBA"
            << std::endl;
  return;
}

void ARMOps::coprocessorDataOPP(ARM7TDMI &cpu, uint32_t instruction) {
  // um, coprocessors don't exist on the GBA actually
  std::cerr << "Coprocessor Data Operations do not exist on the GBA"
            << std::endl;
  return;
}

void ARMOps::coprocessorRegTransfer(ARM7TDMI &cpu, uint32_t instruction) {
  // um, coprocessors don't exist on the GBA actually
  std::cerr << "Coprocessor Register Transfer does not exist on the GBA"
            << std::endl;
  return;
}

void ARMOps::SWI(ARM7TDMI &cpu, uint32_t instruction) {
  uint32_t comment = instruction & 0x00FFFFFF;
  const uint32_t pc = cpu.getLogicalRegister(15);
  uint32_t returnAddr = pc - 4;
  uint32_t currentCPSR = cpu.getCPSR();

  uint32_t newCPSR = currentCPSR;
  newCPSR &= ~0x0000003F; // Clear bits 0-5
  newCPSR |= 0x00000013;  // Supervisor mode;
  newCPSR |= 0x00000080;  // Set I bit to disable IRQs
  cpu.cpsr = newCPSR;
  cpu.updateProcessorMode(0x13);
  cpu.getCurrentSPSR() = currentCPSR;
  cpu.setLogicalRegister(14, returnAddr);
  cpu.setLogicalRegister(15, 0x00000008);
  cpu.flushPipeline();
}
