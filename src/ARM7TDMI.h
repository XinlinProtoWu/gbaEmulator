#ifndef ARM_7_TDMI_H
#define ARM_7_TDMI_H
#include <array>
#include <cstdint>

enum class ProcessorMode : uint8_t {
  User = 0x10,
  FIQ = 0x11,
  IRQ = 0x12,
  Supervisor = 0x13,
  Abort = 0x17,
  Undefined = 0x1B,
  System = 0x1F
};

struct ARM7TDMI {
  // 31 physical registers (R0-R14, plus mode-specific banked registers)
  std::array<uint32_t, 31> regs;
  uint32_t pc;   // Program Counter (R15)
  uint32_t cpsr; // Current Program Status Register

  // Helper to map logical R0-R15 to physical regs based on mode
  uint32_t &getRegister(int logicalReg);

  // Status flag helpers
  bool getFlag(int bit);
  void setFlag(int bit, bool value);
};
#endif // !PROCESSOR
