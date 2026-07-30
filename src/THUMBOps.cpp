#include "THUMBOps.h"
#include "ALUHelpers.h"
#include "ARM7TDMI.h"
#include "memoryBus.h"
#include <cstdint>
#include <iostream>
#include <ostream>
#include <sys/types.h>
#include <type_traits>
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

void THUMBOps::loadPCRel(ARM7TDMI &cpu, uint16_t thumbInstr) {
  // R0-R7
  uint8_t rd = (thumbInstr >> 8) & 0x07;

  // Unsigned offset (0-1020 in steps of 4)
  uint32_t nn = (thumbInstr & 0x0FF) * 4;

  // PC is actually PC + 4 (reminder)
  // FORCE word alignment by clearing bit 0 and 1
  uint32_t pc = cpu.getLogicalRegister(15) & ~0x03;

  uint32_t storedResult = cpu.memoryBus.read32(pc + nn);
  cpu.setLogicalRegister(rd, storedResult);
}

void THUMBOps::loadStoreRelOff(ARM7TDMI &cpu, uint16_t thumbInstr) {
  uint8_t opcode = (thumbInstr >> 10) & 0x03;
  // All registers must be between R0-R7
  uint8_t ro = (thumbInstr >> 6) & 0x07;
  uint8_t rb = (thumbInstr >> 3) & 0x07;
  uint8_t rd = thumbInstr & 0x07;

  uint32_t base = cpu.getLogicalRegister(rb);
  uint32_t offset = cpu.getLogicalRegister(ro);
  uint32_t rdVal = cpu.getLogicalRegister(rd);

  uint32_t address = base + offset;
  uint32_t stored32 = 0;
  uint32_t stored8 = 0;
  switch (opcode) {
  case 0x0:
    // STR Rd, [Rb, Ro] (store 32)
    cpu.memoryBus.write32(address & ~0x03, rdVal);
    break;
  case 0x1:
    // STRB Rd, [Rb, Ro] (store 8)
    cpu.memoryBus.write8(address, static_cast<uint8_t>(rdVal & 0x0FF));
    break;
  case 0x2:
    // LDR Rd, [Rb, Ro] (load 32)
    stored32 = ALUHelper::rotatedRead(cpu, address);
    cpu.setLogicalRegister(rd, stored32);
    break;
  case 0x3:
    // LDRB Rd, [Rb, Ro] (load 8)
    stored8 = cpu.memoryBus.read8(address);
    cpu.setLogicalRegister(rd, stored8);
    break;
  }
}

void THUMBOps::loadStoreSBHw(ARM7TDMI &cpu, uint16_t thumbInstr) {
  uint8_t opcode = (thumbInstr >> 10) & 0x03;
  uint8_t ro = (thumbInstr >> 6) & 0x07;
  uint8_t rb = (thumbInstr >> 3) & 0x07;
  uint8_t rd = thumbInstr & 0x07;

  uint32_t base = cpu.getLogicalRegister(rb);
  uint32_t offset = cpu.getLogicalRegister(ro);
  uint32_t rdVal = cpu.getLogicalRegister(rd);

  uint32_t address = base + offset;
  uint32_t storedResult = 0;

  switch (opcode) {
  case 0x0:
    // STRH Rd, [Rb, Ro] (store 16)
    cpu.memoryBus.write16(address & ~0x01, static_cast<uint16_t>(rdVal));
    break;
  case 0x1: {
    // LDSB Rd, [Rb, Ro] (load sign-extended 8)
    uint32_t readResult = cpu.memoryBus.read8(address) & 0x000000FF;
    // Sign determination by looking at bit 7
    storedResult = ((readResult >> 7) & 0x01) ? (0xFFFFFF00 | readResult)
                                              : (0x00000000 | readResult);
    cpu.setLogicalRegister(rd, storedResult);
    break;
  }
  case 0x2:
    // LDRH Rd, [Rb, Ro] (load zero-extended 16 bit)
    storedResult =
        0x00000000 |
        (static_cast<uint16_t>(ALUHelper::rotatedRead(cpu, address)) &
         0x0000FFFF);
    cpu.setLogicalRegister(rd, storedResult);
    break;
  case 0x3: {
    // LDSH Rd, [Rb, Ro] (load sign-extended 16)
    uint32_t readResult = (address & 0x01)
                              ? cpu.memoryBus.read8(address) & 0x000000FF
                              : cpu.memoryBus.read16(address) & 0x0000FFFF;
    // Sign determination by looking at bit 15 or bit 7 if misaligned
    if (address & 0x01) {
      storedResult = ((readResult >> 7) & 0x01) ? (0xFFFFFF00 | readResult)
                                                : (0x00000000) | readResult;
    } else {
      storedResult = ((readResult >> 15) & 0x01) ? (0xFFFF0000 | readResult)
                                                 : (0x00000000 | readResult);
    }
    cpu.setLogicalRegister(rd, storedResult);
    break;
  }
  }
}

void THUMBOps::loadStoreImmOff(ARM7TDMI &cpu, uint16_t thumbInstr) {
  uint8_t opcode = (thumbInstr >> 11) & 0x03;
  // Whether this is a byte operation or word operation can be determined
  // through looking at bit 12.
  bool isByte = (thumbInstr >> 12) & 0x01;
  uint32_t nn = (thumbInstr >> 6) & 0x01F;
  // nn is either 0-31 for byte or 0-124 for word
  nn = (isByte) ? nn : nn * 4;
  uint8_t rb = (thumbInstr >> 3) & 0x07;
  uint8_t rd = thumbInstr & 0x07;

  uint32_t base = cpu.getLogicalRegister(rb);
  uint32_t rdVal = cpu.getLogicalRegister(rd);
  uint32_t transferAddr = base + nn;
  uint32_t storedResult;

  switch (opcode) {
  case 0x0:
    // STR Rd, [Rb, #nn]
    cpu.memoryBus.write32(transferAddr & ~0x03, rdVal);
    break;
  case 0x1:
    // LDR Rd, [Rb, #nn]
    storedResult = ALUHelper::rotatedRead(cpu, transferAddr);
    cpu.setLogicalRegister(rd, storedResult);
    break;
  case 0x2:
    // STRB Rd, [Rb, #nn]
    cpu.memoryBus.write8(transferAddr,
                         static_cast<uint8_t>(rdVal & 0x000000FF));
    break;
  case 0x3:
    // LDRB Rd, [Rb, #nn]
    storedResult = static_cast<uint32_t>(cpu.memoryBus.read8(transferAddr));
    cpu.setLogicalRegister(rd, storedResult);
    break;
  }
}
void THUMBOps::loadStoreHw(ARM7TDMI &cpu, uint16_t thumbInstr) {
  uint8_t opcode = (thumbInstr >> 11) & 0x01;
  // Steps of 2 (0-62)
  uint32_t nn = ((thumbInstr >> 6) & 0x01F) * 2;
  uint8_t rb = (thumbInstr >> 3) & 0x07;
  uint8_t rd = thumbInstr & 0x07;

  uint32_t base = cpu.getLogicalRegister(rb);
  uint32_t rdVal = cpu.getLogicalRegister(rd);
  uint32_t transferAddr = base + nn;
  uint32_t storedResult;

  switch (opcode) {
  case 0x0:
    // STRH Rd, [Rb, #nn]
    cpu.memoryBus.write16(transferAddr & ~0x01,
                          static_cast<uint16_t>(rdVal & 0x0000FFFF));
    break;
  case 0x1:
    // LDRH Rd, [Rb, #nn]
    storedResult =
        static_cast<uint16_t>(ALUHelper::rotatedRead(cpu, transferAddr));
    cpu.setLogicalRegister(rd, storedResult);
    break;
  }
}

void THUMBOps::spRelLoadStore(ARM7TDMI &cpu, uint16_t thumbInstr) {
  uint8_t opcode = (thumbInstr >> 11) & 0x01;
  uint8_t rd = (thumbInstr >> 8) & 0x07;
  // Steps of 4 (0-1020)
  uint32_t nn = (thumbInstr & 0x0FF) * 4;

  uint32_t sp = cpu.getLogicalRegister(13);
  uint32_t rdVal = cpu.getLogicalRegister(rd);
  uint32_t storedResult;
  uint32_t transferAddr = sp + nn;

  switch (opcode) {
  case 0x0:
    // STR Rd, [Sp, #nn]
    cpu.memoryBus.write32(transferAddr & ~0x03, (rdVal));
    break;
  case 0x1:
    // LDR Rd, [Sp, #nn]
    storedResult = ALUHelper::rotatedRead(cpu, transferAddr);
    cpu.setLogicalRegister(rd, storedResult);
    break;
  }
}

void THUMBOps::loadAdr(ARM7TDMI &cpu, uint16_t thumbInstr) {
  uint8_t opcode = (thumbInstr >> 11) & 0x01;
  uint8_t rd = (thumbInstr >> 8) & 0x07;
  // Steps of 4 (0-1020)
  uint32_t nn = (thumbInstr & 0x0FF) * 4;

  // Reminder that PC actually stores currently executing address + 4
  uint32_t pc = cpu.getLogicalRegister(15);
  uint32_t sp = cpu.getLogicalRegister(13);
  uint32_t storedResult;

  // Opcode 0: ADD Rd, PC, #nn
  // Opcode 1: ADD Rd, Sp, #nn
  storedResult = (opcode) ? (sp + nn) : ((pc & ~0x02) + nn);
  cpu.setLogicalRegister(rd, storedResult);
}

void THUMBOps::addOffSP(ARM7TDMI &cpu, uint16_t thumbInstr) {
  uint8_t opcode = (thumbInstr >> 7) & 0x01;
  // unsigned offset (0-508) steps of 4
  uint32_t nn = (thumbInstr & 0x07F) * 4;

  uint32_t sp = cpu.getLogicalRegister(13);

  // Opcode 0: ADD SP, #nn 1: ADD SP, #-nn
  sp = (opcode) ? (sp - nn) : (sp + nn);
  cpu.setLogicalRegister(13, sp);
}

void THUMBOps::ppReg(ARM7TDMI &cpu, uint16_t thumbInstr) {
  uint8_t opcode = (thumbInstr >> 11) & 0x01;
  uint8_t pcLr = (thumbInstr >> 8) & 0x01;
  uint16_t rList = thumbInstr & 0x0FF;

  uint32_t sp = cpu.getLogicalRegister(13);

  if (opcode) {
    // POP {Rlist}{PC} {load from memory}
    for (int regIdx = 0; regIdx <= 7; regIdx++) {
      if ((rList >> regIdx) & 0x01) {
        cpu.setLogicalRegister(regIdx, cpu.memoryBus.read32(sp & ~0x03));
        sp += 4;
      }
    }
    if (pcLr) {
      uint32_t popPC = cpu.memoryBus.read32(sp & ~0x03);
      sp += 4;

      // ARMv4T ignores the LSB of the loaded address for POP {pc}
      cpu.setLogicalRegister(15, popPC & ~0x01);
      // Write into reg13 before flushing pipeline
      cpu.setLogicalRegister(13, sp);
      cpu.flushPipeline();
    }
  } else {
    // PUSH {Rlist}{LR} (store in memory)
    if (pcLr) {
      sp -= 4;
      uint32_t lr = cpu.getLogicalRegister(14);
      cpu.memoryBus.write32(sp & ~0x03, lr);
    }
    for (int regIdx = 7; regIdx >= 0; regIdx--) {
      if ((rList >> regIdx) & 0x01) {
        sp -= 4;
        cpu.memoryBus.write32(sp & ~0x03, cpu.getLogicalRegister(regIdx));
      }
    }
  }
  cpu.setLogicalRegister(13, sp);
}

void THUMBOps::mulLoadStore(ARM7TDMI &cpu, uint16_t thumbInstr) {
  uint8_t opcode = (thumbInstr >> 11) & 0x01;
  uint8_t rb = (thumbInstr >> 8) & 0x07;
  uint32_t rlist = thumbInstr & 0x0FF;

  uint32_t rbVal = cpu.getLogicalRegister(rb);
  uint32_t currentAddr = rbVal;

  bool emptyList = (!rlist);
  uint32_t finalBaseAddr = rbVal;

  if (emptyList) {
    rlist = 0x00008000;
    finalBaseAddr += 0x40;
  } else {
    for (int regIdx = 0; regIdx <= 7; regIdx++) {
      if ((rlist >> regIdx) & 0x01) {
        finalBaseAddr += 4;
      }
    }
  }

  bool baseIsFirst = true;
  bool loadedPC = false;

  if (emptyList) {
    if (opcode) {
      // LDMIA
      uint32_t val = cpu.memoryBus.read32(currentAddr & ~0x03);
      cpu.setLogicalRegister(15, val);
      loadedPC = true;
    } else {
      // STMIA
      cpu.memoryBus.write32(currentAddr & ~0x03, cpu.getLogicalRegister(15));
    }
    currentAddr += 4;
    baseIsFirst = false;
  } else {
    for (int regIdx = 0; regIdx <= 7; regIdx++) {
      if ((rlist >> regIdx) & 0x01) {

        if (opcode) {
          // LDMIA load from memory and increment rb
          uint32_t val = cpu.memoryBus.read32(currentAddr & ~0x03);
          cpu.setLogicalRegister(regIdx, val);
        } else {
          // STMIA store to memory and increment rb
          uint32_t val = cpu.getLogicalRegister(regIdx);
          // Writeback quirk if rb is the first in list then store old
          if (regIdx == rb && !emptyList) {
            val = baseIsFirst ? rbVal : finalBaseAddr;
          }
          cpu.memoryBus.write32(currentAddr & ~0x03, val);
        }
        currentAddr += 4;
        baseIsFirst = false;
      }
    }
  }

  // LDMIA ignores writeback if the base reg was in the load list
  bool disableWriteback = opcode && (!emptyList && ((thumbInstr >> rb) & 0x01));

  if (!disableWriteback) {
    cpu.setLogicalRegister(rb, finalBaseAddr);
  }

  if (loadedPC) {
    cpu.flushPipeline();
  }
}

void THUMBOps::condBranch(ARM7TDMI &cpu, uint16_t thumbInstr) {
  uint8_t opcode = (thumbInstr >> 8) & 0x0F;
  uint32_t offset = (thumbInstr & 0x0FF);
  int32_t signedOffset = (offset >> 7 & (0x01)) ? (0xFFFFFF00 | offset) << 1
                                                : (0x00000000 | offset) << 1;
  uint8_t vFlag = (cpu.getCPSR() >> 28) & 0x01;
  uint8_t cFlag = (cpu.getCPSR() >> 29) & 0x01;
  uint8_t zFlag = (cpu.getCPSR() >> 30) & 0x01;
  uint8_t nFlag = (cpu.getCPSR() >> 31) & 0x01;
  // Note that pc stored in R15 is actually pc + 4
  uint32_t branchAddr = cpu.getLogicalRegister(15) + signedOffset;
  bool branchTaken = false;
  switch (opcode) {
  case 0x0:
    // BEQ label
    branchTaken = zFlag;
    break;
  case 0x1:
    // BNE label
    branchTaken = !zFlag;
    break;
  case 0x2:
    // BCS/BHS label
    branchTaken = cFlag;
    break;
  case 0x3:
    // BCC/BLO label
    branchTaken = !cFlag;
    break;
  case 0x4:
    // BMI label
    branchTaken = nFlag;
    break;
  case 0x5:
    // BPL label
    branchTaken = !nFlag;
    break;
  case 0x6:
    // BVS label
    branchTaken = vFlag;
    break;
  case 0x7:
    // BVC label
    branchTaken = !vFlag;
    break;
  case 0x8:
    // BHI label
    branchTaken = (cFlag && !zFlag);
    break;
  case 0x9:
    // BLS label
    branchTaken = (!cFlag || zFlag);
    break;
  case 0xA:
    // BGE label
    branchTaken = (nFlag == vFlag);
    break;
  case 0xB:
    // BLT label
    branchTaken = (nFlag != vFlag);
    break;
  case 0xC:
    // BGT label
    branchTaken = (!zFlag && (nFlag == vFlag));
    break;
  case 0xD:
    // BLE label
    branchTaken = (zFlag || (nFlag != vFlag));
    break;
  case 0xE:
    std::cerr
        << "CONDITIONAL BRANCH ERROR: undefined condition, should not be used!"
        << std::endl;
    return;
    break;
  case 0xF:
    // SWI is handled in executeTHUMB before condBranch is ever called.
    // If we reach here, it's a decoding routing error.
    std::cerr
        << "CONDITIONAL BRANCH ERROR: SWI (0xF) should not be routed here!"
        << std::endl;
    return;
    break;
  }

  if (branchTaken) {
    cpu.setLogicalRegister(15, branchAddr);
    cpu.flushPipeline();
  }
}

void THUMBOps::SWI(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::uncondBranch(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::longBranchWLink(ARM7TDMI &cpu, uint16_t thumbInstr) {}
