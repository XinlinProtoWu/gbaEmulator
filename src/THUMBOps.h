#ifndef THUMB_OPS_H
#define THUMB_OPS_H

// Forward declaration
#include <cstdint>
class ARM7TDMI;

class THUMBOps {
public:
  static void moveShiftedReg(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void addAndSub(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void MCASImm(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void ALU(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void hiRegOpBE(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void loadPCRel(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void loadStoreRelOff(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void loadStoreSBHw(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void loadStoreImmOff(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void loadStoreHw(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void spRelLoadStore(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void loadAdr(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void addOffSP(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void ppReg(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void mulLoadStore(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void SWI(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void condBranch(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void uncondBranch(ARM7TDMI &cpu, uint16_t thumbInstr);
  static void longBranchWLink(ARM7TDMI &cpu, uint16_t thumbInstr);
};

#endif // !THUMB_OPS_H
