#include "ARMOps.h"
#include "ARM7TDMI.h"
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
    nFlag = (cpu.physicalRegisters[cpu.getPhysicalRegisterIndex(rd)] >> 31) &
            0x00000001;
    zFlag = cpu.physicalRegisters[cpu.getPhysicalRegisterIndex(rd)] == 0
                ? 0x00000001
                : 0x00000000;
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
  uint64_t longResult;
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
    cpu.physicalRegisters[cpu.getPhysicalRegisterIndex(rd)] = rmVal * rsVal;
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
    cpu.physicalRegisters[cpu.getPhysicalRegisterIndex(rd)] =
        rmVal * rsVal + rnVal;
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
    cpu.physicalRegisters[cpu.getPhysicalRegisterIndex(rd)] =
        static_cast<uint32_t>((longResult >> 32) & 0xFFFFFFFF);
    // Set RdLo
    cpu.physicalRegisters[cpu.getPhysicalRegisterIndex(rn)] =
        static_cast<uint32_t>(longResult & 0xFFFFFFFF);
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
    cpu.physicalRegisters[cpu.getPhysicalRegisterIndex(rd)] =
        static_cast<uint32_t>((longResult >> 32) & 0xFFFFFFFF);
    // Set RdLo
    cpu.physicalRegisters[cpu.getPhysicalRegisterIndex(rn)] =
        static_cast<uint32_t>(longResult & 0xFFFFFFFF);
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
    cpu.physicalRegisters[cpu.getPhysicalRegisterIndex(rd)] =
        static_cast<uint32_t>((longResult >> 32) & 0xFFFFFFFF);
    // Set RdLo
    cpu.physicalRegisters[cpu.getPhysicalRegisterIndex(rn)] =
        static_cast<uint32_t>(longResult & 0xFFFFFFFF);
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
    cpu.physicalRegisters[cpu.getPhysicalRegisterIndex(rd)] =
        static_cast<uint32_t>((longResult >> 32) & 0xFFFFFFFF);
    // Set RdLo
    cpu.physicalRegisters[cpu.getPhysicalRegisterIndex(rn)] =
        static_cast<uint32_t>(longResult & 0xFFFFFFFF);
    if (s == 1) {
      ARMOps::setMultiFlag(cpu, rd, longResult, isLong);
    }
    break;
  }
}

void ARMOps::singleDataSwap(ARM7TDMI &cpu, uint32_t instruction) {}

void ARMOps::halfwordDataTransReg(ARM7TDMI &cpu, uint32_t instruction) {}

void ARMOps::halfwordDataTransImm(ARM7TDMI &cpu, uint32_t instruction) {}

void ARMOps::branchAndExchange(ARM7TDMI &cpu, uint32_t instruction) {}

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
