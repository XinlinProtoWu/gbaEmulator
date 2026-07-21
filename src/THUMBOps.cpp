#include "THUMBOps.h"
#include "ALUHelpers.h"
#include "ARM7TDMI.h"
#include "memoryBus.h"
#include <cstdint>
#include <iostream>
#include <ostream>
#include <sys/types.h>
void THUMBOps::moveShiftedReg(ARM7TDMI &cpu, uint16_t thumbInstr) {
  uint8_t opcode = (thumbInstr >> 11) & 0x03;
  // Offset (0-31)
  uint8_t offset = (thumbInstr >> 6) & 0x1F;
  // Both source register and destination registers from R0-R7
  uint8_t rs = (thumbInstr >> 3) & 0x07;
  uint8_t rd = thumbInstr & 0x07;

  // Legality Check
  if (opcode == 0x03) {
    std::cerr
        << "Undefined THUMB Instruction: Reserved move shifted register opcode"
        << std::endl;
    return;
  }
  if (rs > 7 || rd > 7) {
    std::cerr << "Register Usage Error at THUMB move shifted register!"
              << std::endl;
    return;
  }

  uint32_t rsVal = cpu.getLogicalRegister(rs);
  uint8_t oldCarry = (cpu.getCPSR() >> 29) & 0x01;

  ALUHelper::shiftResult sr =
      ALUHelper::shiftOperand(rsVal, opcode, offset, oldCarry, false);

  cpu.setLogicalRegister(rd, sr.value);

  uint32_t cpsr = cpu.getCPSR();

  cpsr = (cpsr & 0x3FFFFFFF) | ((sr.value & 0x80000000) ? 0x80000000 : 0);
  cpsr |= (sr.value == 0) ? 0x40000000 : 0;

  // Update Carry flag (bit 29) if the shift updated it
  if (sr.carryUpdated) {
    cpsr = (cpsr & 0xDFFFFFFF) | (sr.carry ? 0x20000000 : 0);
  }

  cpu.cpsr = cpsr;
}

void THUMBOps::addAndSub(ARM7TDMI &cpu, uint16_t thumbInstr) {
  // 0: addReg, 1: subReg, 2: addImm, 3: subImm
  uint8_t opcode = (thumbInstr >> 9) & 0x03;
  // 0-7, reg or imm
  uint8_t operand = (thumbInstr >> 6) & 0x07;
  // Source and destination registers only from R0-R7
  uint8_t rs = (thumbInstr >> 3) & 0x07;
  uint8_t rd = thumbInstr & 0x07;

  // Legality Check
  if (rd > 7 || rs > 7 || operand > 7) {
    std::cerr << "Register Usage Error in THUMB addAndSub!" << std::endl;
    return;
  }

  uint32_t rsVal = cpu.getLogicalRegister(rs);
  uint32_t rnVal;
  uint64_t result = 0;
  uint32_t storedResult = 0;

  uint8_t carryOut = 0;
  uint8_t vFlag = 0;

  switch (opcode) {
  case 0x0:
    // ADD Rd, Rs, Rn
    rnVal = cpu.getLogicalRegister(operand);
    result = static_cast<uint64_t>(rsVal) + static_cast<uint64_t>(rnVal);
    storedResult = static_cast<uint32_t>(result);

    carryOut = (result >> 32) & 0x01;
    vFlag = ((~(rsVal ^ rnVal) & (storedResult ^ rsVal)) >> 31) & 0x01;
    break;
  case 0x1:
    // Sub Rd, Rs, Rn
    rnVal = cpu.getLogicalRegister(operand);
    result = static_cast<uint64_t>(rsVal) - static_cast<uint64_t>(rnVal);
    storedResult = static_cast<uint32_t>(result);

    carryOut = (rsVal >= rnVal);
    vFlag = (((rsVal ^ rnVal) & (rsVal ^ storedResult)) >> 31) & 0x01;
    break;
  case 0x2:
    // ADD, Rd, Rs, #nn
    result = static_cast<uint64_t>(rsVal) + static_cast<uint64_t>(operand);
    storedResult = static_cast<uint32_t>(result);

    carryOut = (result >> 32) & 0x01;
    vFlag = ((~(rsVal ^ operand) & (storedResult ^ rsVal)) >> 31) & 0x01;
    break;
  case 0x3:
    // SUB Rd, Rs, #nn
    result = static_cast<uint64_t>(rsVal) - static_cast<uint64_t>(operand);
    storedResult = static_cast<uint32_t>(result);

    carryOut = (rsVal >= operand);
    vFlag = (((rsVal ^ operand) & (rsVal ^ storedResult)) >> 31) & 0x01;
    break;
  }

  // Store destination with result
  cpu.setLogicalRegister(rd, storedResult);

  uint32_t newCpsr = cpu.getCPSR() & 0x0FFFFFFF;

  // N flag
  newCpsr |= (storedResult & 0x80000000);
  // Z flag
  newCpsr |= (!storedResult) ? 0x40000000 : 0x00000000;
  // C flag
  newCpsr |= (carryOut) ? 0x20000000 : 0x00000000;
  // V flag
  newCpsr |= (vFlag) ? 0x10000000 : 0x00000000;

  cpu.cpsr = newCpsr;
}

void THUMBOps::MCASImm(ARM7TDMI &cpu, uint16_t thumbInstr) {
  // 0: MOV, 1: CMP, 2: ADD, 3: SUB
  uint8_t opcode = (thumbInstr >> 11) & 0x03;
  // R0-R7
  uint8_t rd = (thumbInstr >> 8) & 0x07;
  // 0-255
  uint16_t nn = thumbInstr & 0x0FF;

  // Legality Check
  if (rd > 7) {
    std::cerr << "Register Usage Error in THUMB MCASImm!" << std::endl;
    return;
  }

  uint64_t result = 0;
  uint32_t storedResult = 0;
  uint32_t rdVal = cpu.getLogicalRegister(rd);

  uint8_t carryOut = 0;
  uint8_t vFlag = 0;
  bool isCMP = false;
  switch (opcode) {
  case 0x0:
    // MOV Rd, #nn
    storedResult = static_cast<uint32_t>(nn);

    carryOut = (cpu.getCPSR() >> 29) & 0x01;
    vFlag = (cpu.getCPSR() >> 28) & 0x01;
    break;
  case 0x1:
    // CMP Rd, #nn
    result = static_cast<uint64_t>(rdVal) - static_cast<uint64_t>(nn);
    storedResult = static_cast<uint32_t>(result);
    isCMP = true;

    carryOut = (rdVal >= nn);
    vFlag = (((rdVal ^ nn) & (rdVal ^ storedResult)) >> 31) & 0x01;
    break;
  case 0x2:
    // ADD Rd, #nn
    result = static_cast<uint64_t>(rdVal) + static_cast<uint64_t>(nn);
    storedResult = static_cast<uint32_t>(result);

    carryOut = (result >> 32) & 0x01;
    vFlag = ((~(rdVal ^ nn) & (rdVal ^ storedResult)) >> 31) & 0x01;
    break;
  case 0x3:
    // SUB Rd, #nn
    result = static_cast<uint64_t>(rdVal) - static_cast<uint64_t>(nn);
    storedResult = static_cast<uint32_t>(result);

    carryOut = (rdVal >= nn);
    vFlag = (((rdVal ^ nn) & (rdVal ^ storedResult)) >> 31) & 0x01;
    break;
  }

  if (!isCMP) {
    cpu.setLogicalRegister(rd, storedResult);
  }
  uint32_t newCpsr = cpu.getCPSR() & 0x0FFFFFFF;

  // N bit (Negative or less than)
  newCpsr |= (storedResult & 0x80000000);
  // Z bit (Zero or Equal)
  newCpsr |= (!storedResult) ? 0x40000000 : 0x00000000;
  // C bit (carry/borrow/extend)
  newCpsr |= (carryOut) ? 0x20000000 : 0x00000000;
  // V bit (overflow)
  newCpsr |= (vFlag) ? 0x10000000 : 0x00000000;

  cpu.cpsr = newCpsr;
}

void THUMBOps::ALU(ARM7TDMI &cpu, uint16_t thumbInstr) {
  uint8_t opcode = (thumbInstr >> 6) & 0x0F;
  uint8_t rs = (thumbInstr >> 3) & 0x07;
  uint8_t rd = thumbInstr & 0x07;

  // Legality Check
  if (rs > 7 || rd > 7) {
    std::cerr << "Register Usage Error in THUMB MCASImm!" << std::endl;
    return;
  }

  uint64_t result = 0;
  uint32_t storedResult = 0;
  uint32_t rsVal = cpu.getLogicalRegister(rs);
  uint32_t rdVal = cpu.getLogicalRegister(rd);

  uint32_t oldCpsr = cpu.getCPSR();
  uint8_t carry = (oldCpsr >> 29) & 0x01;
  uint8_t oldVFlag = (oldCpsr >> 28) & 0x01;

  // Should not write back to Rd if TST, CMP, CMN
  bool writeBack = true;

  uint8_t carryOut = carry;
  uint8_t vFlag = oldVFlag;

  switch (opcode) {
  case 0x0:
    // AND Rd, Rs
    storedResult = rdVal & rsVal;
    break;
  case 0x1:
    // EOR Rd, Rs (XOR)
    storedResult = rdVal ^ rsVal;
    break;
  case 0x2:
    // LSL Rd, Rs (logical shift left)
  case 0x3:
    // LSR (logical shift right)
  case 0x4:
    // ASR Rd, Rs (arit shift right)
    // Rd = Rd SAR (Rs AND 0FFh)
  case 0x7:
    // ROR Rd, Rs
    {
      uint8_t shiftType = 0;
      if (opcode == 0x2)
        shiftType = 0x0; // LSL
      else if (opcode == 0x3)
        shiftType = 0x1; // LSR
      else if (opcode == 0x4)
        shiftType = 0x2; // ASR
      else if (opcode == 0x7)
        shiftType = 0x3; // ROR

      // Hardware accurate shift using the bottom byte of Rs
      ALUHelper::shiftResult sr =
          ALUHelper::shiftOperand(rdVal, shiftType, rsVal & 0x0FF, carry, true);
      storedResult = sr.value;
      carryOut = sr.carryUpdated ? sr.carry : carry;
      break;
    }
  case 0x5:
    // ADC Rd, Rs (add with carry)
    result = static_cast<uint64_t>(rdVal) + static_cast<uint64_t>(rsVal) +
             static_cast<uint64_t>(carry);
    storedResult = static_cast<uint32_t>(result);

    carryOut = (result >> 32) & 0x01;
    vFlag = ((~(rdVal ^ rsVal) & (rdVal ^ storedResult)) >> 31) & 0x01;
    break;
  case 0x6:
    // SBC Rd, Rs (sub with carry)
    result = static_cast<uint64_t>(rdVal) - static_cast<uint64_t>(rsVal) -
             (carry ? 0 : 1);
    storedResult = static_cast<uint32_t>(result);

    carryOut = static_cast<uint64_t>(rdVal) >=
               (static_cast<uint64_t>(rsVal) + (carry ? 0 : 1));
    vFlag = (((rdVal ^ rsVal) & (rdVal ^ storedResult)) >> 31) & 0x01;
    break;

  case 0x8:
    // TST Rd, Rs
    writeBack = false;
    storedResult = rdVal & rsVal;
    break;
  case 0x9:
    // NEG Rd, Rs (negate)
    result = 0x0000000000000000 - static_cast<uint64_t>(rsVal);
    storedResult = static_cast<uint32_t>(result);

    carryOut = (rsVal == 0); // CY is 1 only if 0 >= rsVal
    // If the result is somehow the same sign as rs, then it is determined to be
    // overflow?
    vFlag = ((rsVal & storedResult) >> 31) & 0x01;
    break;
  case 0xA:
    // CMP Rd, Rs
    writeBack = false;
    result = static_cast<uint64_t>(rdVal) - static_cast<uint64_t>(rsVal);
    storedResult = static_cast<uint32_t>(result);

    carryOut = (rdVal >= rsVal);
    vFlag = (((rdVal ^ rsVal) & (rdVal ^ storedResult)) >> 31) & 0x01;
    break;
  case 0xB:
    // CMN Rd, Rs
    writeBack = false;
    result = static_cast<uint64_t>(rdVal) + static_cast<uint64_t>(rsVal);
    storedResult = static_cast<uint32_t>(result);

    carryOut = (result >> 32) & 0x01;
    vFlag = ((~(rdVal ^ rsVal) & (rdVal ^ storedResult)) >> 31) & 0x01;
    break;
  case 0xC:
    // ORR Rd, Rs (orr logical)
    storedResult = rdVal | rsVal;
    break;
  case 0xD:
    // MUL Rd, Rs (multiply)
    result = static_cast<uint64_t>(rdVal) * static_cast<uint64_t>(rsVal);
    storedResult = static_cast<uint32_t>(result);
    // Carry flag destroyed in ARMv4
    break;
  case 0xE:
    // BIC Rd, Rs (bit clear)
    storedResult = rdVal & ~rsVal;
    break;
  case 0xF:
    // MVN Rd, Rs (not)
    storedResult = ~rsVal;
    break;
  }

  if (writeBack) {
    cpu.setLogicalRegister(rd, storedResult);
  }

  uint32_t newCpsr = cpu.getCPSR() & 0x0FFFFFFF;

  // N bit (Negative or less than)
  newCpsr |= (storedResult & 0x80000000);
  // Z bit (Zero or Equal)
  newCpsr |= (!storedResult) ? 0x40000000 : 0x00000000;
  // C bit (carry/borrow/extend)
  newCpsr |= (carryOut) ? 0x20000000 : 0x00000000;
  // V bit (overflow)
  newCpsr |= (vFlag) ? 0x10000000 : 0x00000000;

  cpu.cpsr = newCpsr;
}

void THUMBOps::hiRegOpBE(ARM7TDMI &cpu, uint16_t thumbInstr) {
  // 0: Add, 1: CMP, 2: MOV, 3: BX
  uint8_t opcode = (thumbInstr >> 8) & 0x03;
  uint8_t MSBd = (thumbInstr >> 7) & 0x01;
  uint8_t MSBs = (thumbInstr >> 6) & 0x01;
  // together with MSBs and MSBd, R0-R15
  // A reminder that PC is actually referring to current instruction + 4 in
  // thumb, unlike ARM which is + 8
  uint8_t rs = ((thumbInstr >> 3) & 0x07) | (MSBs << 3);
  uint8_t rd = (thumbInstr & 0x07) | (MSBd << 3);

  uint32_t rsVal = cpu.getLogicalRegister(rs);
  uint32_t rdVal = cpu.getLogicalRegister(rd);
  uint64_t result = 0;
  uint32_t storedResult = 0;
  bool isCMP = false;
  bool isBX = false;

  uint32_t oldCpsr = cpu.getCPSR();
  uint8_t carryOut = (oldCpsr >> 29) & 0x01;
  uint8_t vFlag = (oldCpsr >> 28) & 0x01;

  switch (opcode) {
  case 0x0:
    // ADD Rd, Rs
    result = static_cast<uint64_t>(rdVal) + static_cast<uint64_t>(rsVal);
    storedResult = static_cast<uint32_t>(result);
    break;
  case 0x1:
    // CMP Rd, Rs
    isCMP = true;
    result = static_cast<uint64_t>(rdVal) - static_cast<uint64_t>(rsVal);
    storedResult = static_cast<uint32_t>(result);

    carryOut = (rdVal >= rsVal);
    vFlag = (((rdVal ^ rsVal) & (storedResult ^ rdVal)) >> 31) & 0x01;
    break;
  case 0x2:
    // MOV Rd, Rs
    // NOP gets processed naturally with the instruction MOV R8, R8
    storedResult = rsVal;
    break;
  case 0x3:
    // BX Rs (no BLX since ARM7TDMI does not have ARM9 instruction sets)
    isBX = true;

    if (MSBd) {
      std::cerr << "THUMB BLX Instruction does not exist on ARM7TDMI!"
                << std::endl;
      return;
    }

    // Switch state and update PC
    if (rsVal & 0x01) {
      // Switch to THUMB
      cpu.cpsr |= 0x20;
      cpu.setLogicalRegister(15, rsVal & ~0x01);
    } else {
      // Switch to ARM
      cpu.cpsr &= ~0x20;
      cpu.setLogicalRegister(15, rsVal & ~0x03);
    }
    cpu.flushPipeline();
    break;
  }

  // Writeback should no happen in bx or cmp
  if (!isBX && !isCMP) {
    cpu.setLogicalRegister(rd, storedResult);
    if (rd == 15) {
      cpu.flushPipeline();
    }
  }

  if (isCMP) {
    uint32_t newCpsr = cpu.getCPSR() & 0x0FFFFFFF;

    // N bit (Negative or less than)
    newCpsr |= (storedResult & 0x80000000);
    // Z bit (Zero or Equal)
    newCpsr |= (!storedResult) ? 0x40000000 : 0x00000000;
    // C bit (carry/borrow/extend)
    newCpsr |= (carryOut) ? 0x20000000 : 0x00000000;
    // V bit (overflow)
    newCpsr |= (vFlag) ? 0x10000000 : 0x00000000;

    cpu.cpsr = newCpsr;
  }
}
void THUMBOps::loadPCRel(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::loadStoreRelOff(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::loadStoreSBHw(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::loadStoreImmOff(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::loadStoreHw(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::spRelLoadStore(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::loadAdr(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::addOffSP(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::ppReg(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::mulLoadStore(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::SWI(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::condBranch(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::uncondBranch(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::longBranchWLink(ARM7TDMI &cpu, uint16_t thumbInstr) {}
