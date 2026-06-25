#ifndef ARM_7_TDMI_H
#define ARM_7_TDMI_H
#include <array>
#include <cstdint>

// Forward declaration of Memory class, ARMOps class, and THUMBOps class
class MemoryBus;
class ARMOps;
class THUMBOps;
// ARM7TDMI Operating Modes (Stored in the lower 5 bits of CPSR)
enum class CpuMode : uint8_t {
  User = 0x10,
  FIQ = 0x11,
  IRQ = 0x12,
  Supervisor = 0x13,
  Abort = 0x17,
  Undefined = 0x1B,
  System = 0x1F
};

class ARM7TDMI {
  friend class ARMOps;
  friend class THUMBOps;

public:
  ARM7TDMI(MemoryBus &bus);
  ~ARM7TDMI() = default;

  // System Interface
  void reset();
  void step();

  // Debugger and State Access
  uint32_t getLogicalRegister(int index) const;
  void setLogicalRegister(int index, uint32_t value);
  uint32_t getCPSR() const;

private:
  MemoryBus &memoryBus;
  // Processor State
  // 31 physical registers: 16 user + 15 banked registers
  // R13 is SP, R15 is PC
  std::array<uint32_t, 31> physicalRegisters;

  // Status registers
  uint32_t cpsr;
  uint8_t currentMode;
  // 5 Bankes SPSRs (FIQ, IRQ, Supervisor, Abort, Undefined)
  std::array<uint32_t, 5> spsr;

  // 3 stage instruction pipeline
  // Instruction A is actively modifying registers or memory
  // pipeline[0]: Instruction B is being translated by the CPU for next steps
  // pipeline[1]: Instruction C is being physically read from the GBA memory bus
  std::array<uint32_t, 2> pipeline;

  // Translate the "virtual" register index to its actual physical index
  uint8_t getPhysicalRegisterIndex(int logicalIndex) const;

  // Fetch a reference to the current mode's SPSR
  // Throw an exception or return a dummy reference if in user/system mode
  uint32_t &getCurrentSPSR();
  uint8_t getSPSRIndex();
  void flushPipeline();
  void fillPipeline();

  void executeARM(uint32_t instruction);
  void executeTHUMB(uint32_t instruction);
};
#endif // !ARM_7_TDMI_H
