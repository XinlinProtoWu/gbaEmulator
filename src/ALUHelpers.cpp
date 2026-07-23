#include "ALUHelpers.h"
#include "ARM7TDMI.h"
#include "memoryBus.h"
#include <cstdint>

namespace ALUHelper {
shiftResult shiftOperand(uint32_t value, uint8_t shiftType, uint8_t amount,
                         uint8_t oldCarry, bool byRegister) {
  shiftResult out{};
  out.value = value;
  out.carry = oldCarry;
  out.carryUpdated = true;

  switch (shiftType) {
  case 0x0:
    // LSL
    if (!amount) {
      out.carryUpdated = false;
    } else if (amount < 32) {
      out.carry = (value >> (32 - amount)) & 0x01;
      out.value = value << amount;
    } else {
      out.carry = 0;
      out.value = 0;
    }
    break;
  case 0x1:
    // LSR
    if (!amount && !byRegister) {
      amount = 32;
    }
    if (amount < 32) {
      out.carry = (value >> (amount - 1)) & 0x01;
      out.value = value >> amount;
    } else {
      out.carry = (value >> 31) & 0x01;
      out.value = 0;
    }
    break;
  case 0x2:
    // ASR
    if (!amount && !byRegister) {
      amount = 32;
    }
    if (amount < 32) {
      out.carry = (value >> (amount - 1)) & 1;
      out.value = static_cast<int32_t>(value) >> amount;
    } else {
      out.carry = value >> 31;
      out.value = (value & 0x80000000) ? 0xFFFFFFFF : 0x00000000;
    }
    break;
  case 0x3:
    // ROR / RRX
    if (!amount && !byRegister) {
      // RRX
      out.value = (oldCarry << 31) | (value >> 1);
      out.carry = value & 0x01;
    } else {
      amount &= 0x1F;
      out.value = (value >> amount) | (value << (32 - amount));
      out.carry = out.value >> 31;
    }
    break;
  }
  return out;
}

uint32_t rotatedRead(ARM7TDMI &cpu, uint32_t transferAddr) {
  // Load word (with misaligned rotated read support)
  uint32_t wordAlignedAddr = transferAddr & ~0x03;
  // How many bits are shifted
  uint32_t shift = (transferAddr & 0x03) * 8;
  uint32_t val = cpu.memoryBus.read32(wordAlignedAddr);

  if (shift != 0) {
    val = (val >> shift) | (val << (32 - shift));
  }
  return val;
}

} // namespace ALUHelper
