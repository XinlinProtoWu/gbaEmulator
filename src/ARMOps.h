#ifndef ARM_OPS_H
#define ARM_OPS_H

#include <cstdint>

// ARM7TDMI forward declaration
class ARM7TDMI;

class ARMOps {
public:
  static void setMultiFlag(ARM7TDMI &cpu, uint8_t rd, uint64_t longResult,
                           bool isLong);
  static void muliply(ARM7TDMI &cpu, uint32_t instruction);
  static void singleDataSwap(ARM7TDMI &cpu, uint32_t instruction);
  static void halfwordDataTransReg(ARM7TDMI &cpu, uint32_t instruction);
  static void halfwordDataTransImm(ARM7TDMI &cpu, uint32_t instruction);
  static void branchAndExchange(ARM7TDMI &cpu, uint32_t instruction);
  static void MRS(ARM7TDMI &cpu, uint32_t instruction);
  static void MSR(ARM7TDMI &cpu, uint32_t instruction);
  static void ALU(ARM7TDMI &cpu, uint32_t instruction);
  static void loadStoreWBImm(ARM7TDMI &cpu, uint32_t instruction);
  static void loadStoreWBReg(ARM7TDMI &cpu, uint32_t instruction);
  static void blockDataTransfer(ARM7TDMI &cpu, uint32_t instruction);
  static void branch(ARM7TDMI &cpu, uint32_t instruction);
  static void coprocessorDataTransfer(ARM7TDMI &cpu, uint32_t instruction);
  static void coprocessorDataOPP(ARM7TDMI &cpu, uint32_t instruction);
  static void coprocessorRegTransfer(ARM7TDMI &cpu, uint32_t instruction);
  static void SWI(ARM7TDMI &cpu, uint32_t instruction);
};
#endif //! ARM_OPS_H
