#ifndef ALU_HELP_H
#define ALU_HELP_H

#include <cstdint>

class ARM7TDMI;

namespace ALUHelper {
// Helper for shift type
struct shiftResult {
  uint32_t value;
  uint8_t carry;
  bool carryUpdated;
};

shiftResult shiftOperand(uint32_t value, uint8_t shiftType, uint8_t amount,
                         uint8_t oldCarry, bool byRegister);

uint32_t rotatedRead(ARM7TDMI &cpu, uint32_t transferAddr);

} // namespace ALUHelper
#endif // !ALU_HELP_H
