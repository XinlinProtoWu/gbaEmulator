#include "THUMBOps.h"
#include "ALUHelpers.h"
#include "ARM7TDMI.h"
#include "memoryBus.h"
#include <cstdint>
#include <iostream>
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
void THUMBOps::addAndSub(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::MCASImm(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::ALU(ARM7TDMI &cpu, uint16_t thumbInstr) {}
void THUMBOps::hiRegOpBE(ARM7TDMI &cpu, uint16_t thumbInstr) {}
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
