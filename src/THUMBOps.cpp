#include "THUMBOps.h"
#include "ARM7TDMI.h"
#include "memoryBus.h"
#include <cstdint>
#include <iostream>
void THUMBOps::moveShiftedReg(ARM7TDMI &cpu, uint16_t thumbInstr) {}
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
