//===- ConcreteReverseExec.cpp - Concrete Reverse Execution ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Analysis/ConcreteReverseExec.h"

#include <iomanip>
#include <set>
#include <sstream>

#define DEBUG_TYPE "conrecete-rev-exec"

static cl::opt<bool> DisableCRE("disable-cre",
                                cl::desc("Disable Concrete Reverse Execution."),
                                cl::init(false));

void ConcreteReverseExec::dump() {
  LLVM_DEBUG(llvm::dbgs() << "\n****Concrete Register Values For Function: "
                          << mf->getName() << "\n";
             for (const auto &R
                  : currentRegisterValues) {
               if (R.Value != "")
                 llvm::dbgs() << R.Name << ": " << R.Value << "\n";
               else
                 llvm::dbgs() << R.Name << ": "
                              << "<not available>\n";
             });
}

void ConcreteReverseExec::dump2() {
  llvm::dbgs() << "\n****Concrete Register Values For Function: "
               << mf->getName() << "\n";
  for (const auto &R : currentRegisterValues) {
    if (R.Value != "")
      llvm::dbgs() << R.Name << ": " << R.Value << "\n";
    else
      llvm::dbgs() << R.Name << ": "
                   << "<not available>\n";
  }
}

bool ConcreteReverseExec::getIsCREEnabled() const {
  if (DisableCRE)
    return false;
  return CREEnabled;
}

// TODO: Optimize this.
void ConcreteReverseExec::updateCurrRegVal(std::string Reg, std::string Val) {
  for (auto &R : currentRegisterValues) {
    if (R.Name == Reg) {
      if (Val == "") {
        R.Value = "";
        return;
      }

      // Register value is unknown.
      if (R.Value == "") {
        if (CATI->getRegSize(Reg) == 64) {
          const unsigned RegValInBits = (Val.size() - 2) / 2 * 8;
          if (RegValInBits <= 64)
            R.Value = Val;
          else {
            // drop 0x
            Val.erase(Val.begin());
            Val.erase(Val.begin());
            // get last 8 bytes.
            R.Value = "0x" + Val.substr(/*8 bytes*/ Val.size() - 16);
          }
        } else if (CATI->getRegSize(Reg) == 32) {
          const unsigned RegValInBits = (Val.size() - 2) / 2 * 8;
          if (RegValInBits <= 32)
            R.Value = Val;
          else {
            // drop 0x
            Val.erase(Val.begin());
            Val.erase(Val.begin());
            // get last 4 bytes.
            R.Value = "0x" + Val.substr(/*4 bytes*/ Val.size() - 8);
          }
        } else if (CATI->getRegSize(Reg) == 16) {
          const unsigned RegValInBits = (Val.size() - 2) / 2 * 8;
          if (RegValInBits <= 16)
            R.Value = Val;
          else {
            // drop 0x
            Val.erase(Val.begin());
            Val.erase(Val.begin());
            // get last 2 bytes.
            R.Value = "0x" + Val.substr(/*2 bytes*/ Val.size() - 4);
          }
        }
        return;
      }

      // There is already a value that needs to be updated.
      if (R.Value.size() == Val.size())
        R.Value = Val;
      else if (R.Value.size() > Val.size()) {
        // drop 0x part.
        Val.erase(Val.begin());
        Val.erase(Val.begin());
        unsigned diff = R.Value.size() - Val.size();
        R.Value.replace(diff, Val.size(), Val);
      } else {
        // Val.size > R.Value.size
        // get the last N chars only:
        //  eax = 0x00000009
        //  ax = 0x0009
        Val.erase(Val.begin());
        Val.erase(Val.begin());
        unsigned diff = Val.size() - R.Value.size() + 2;
        R.Value = "0x" + Val.substr(diff);
      }
      return;
    }
  }
}

std::string ConcreteReverseExec::getCurretValueInReg(const std::string &Reg) {
  for (auto &R : currentRegisterValues) {
    if (R.Name == Reg)
      return R.Value;
  }
  return std::string("");
}

template <typename T> std::string intToHex(T num, unsigned regValSize) {
  std::stringstream stream;
  stream << "0x" << std::setfill('0') << std::setw(regValSize) << std::hex
         << num;
  return stream.str();
}

void ConcreteReverseExec::writeUIntRegVal(std::string RegName, uint64_t Val,
                                          unsigned regValSize) {
  // We should update all reg aliases as well.
  // TODO: Improve this.
  auto regInfoId = CATI->getID(RegName);
  if (!regInfoId) {
    updateCurrRegVal(RegName, "");
    return;
  }
  auto RegsTuple = CATI->getRegMap(*regInfoId);
  // Create hex value with 16 chars.
  std::string newValue = intToHex(Val, regValSize);
  // update reg aliases as well.
  // e.g. if $eax is modified, update both $rax and $ax as well.
  updateCurrRegVal(std::get<0>(RegsTuple), newValue);
  updateCurrRegVal(std::get<1>(RegsTuple), newValue);
  updateCurrRegVal(std::get<2>(RegsTuple), newValue);
  updateCurrRegVal(std::get<3>(RegsTuple), newValue);
}

void ConcreteReverseExec::invalidateRegVal(std::string RegName) {
  // We should update all reg aliases as well.
  auto regInfoId = CATI->getID(RegName);
  if (!regInfoId) {
    updateCurrRegVal(RegName, "");
    return;
  }
  auto RegsTuple = CATI->getRegMap(*regInfoId);
  updateCurrRegVal(std::get<0>(RegsTuple), "");
  updateCurrRegVal(std::get<1>(RegsTuple), "");
  updateCurrRegVal(std::get<2>(RegsTuple), "");
  updateCurrRegVal(std::get<3>(RegsTuple), "");
}

void ConcreteReverseExec::updatePC(const MachineInstr &MI) {
  // If the option is enabled, we skip the CRE of the MIs.
  if (!getIsCREEnabled())
    return;
  // Initial PC value for the frame, points to the crash-start instruction.
  // We start updating PC for instructions preceding to the crash-start.
  if (MI.getFlag(MachineInstr::CrashStart))
    return;

  if (!CATI->getPC())
    return;
  std::string RegName = *CATI->getPC();

  if (!CATI->getInstAddr(&MI)) {
    invalidateRegVal(RegName);
    return;
  }
  uint64_t Val = 0;
  // Get MIs PC value saved during decompilation.
  Val = *CATI->getInstAddr(&MI);

  // Write current value of the register in the map.
  writeUIntRegVal(RegName, Val);
  dump();
}

std::string ConcreteReverseExec::getEqRegValue(MachineInstr *MI, Register &Reg,
                                               const TargetRegisterInfo &TRI) {
  std::string RetVal = "";

  auto &MRI = MI->getMF()->getRegInfo();

  if (REAnalysis) {
    auto EqRegisters = REAnalysis->getEqRegsAfterMI(MI, {Reg});
    for (auto &RegOffset : EqRegisters) {
      if (RegOffset.RegNum == Reg.id())
        continue;
      if (RegOffset.IsDeref) {
        std::string EqRegName = TRI.getRegAsmName(RegOffset.RegNum).lower();
        auto BaseStr = getCurretValueInReg(EqRegName);
        if (BaseStr != "") {
          uint64_t BaseAddr = 0;
          std::stringstream SS;
          SS << std::hex << BaseStr;
          SS >> BaseAddr;

          BaseAddr += RegOffset.Offset;
          lldb::SBError error;
          // TO DO: Check if this is right
          uint32_t bitSize = TRI.getRegSizeInBits(Reg, MRI);
          uint32_t byteSize = bitSize / 8 + (bitSize % 8 ? 1 : 0);
          auto ValOpt =
              MemWrapper.ReadUnsignedFromMemory(BaseAddr, byteSize, error);
          if (ValOpt.hasValue()) {
            SS.clear();
            SS << std::hex << *ValOpt;
            SS >> RetVal;
            break;
          }
        }
      } else {
        std::string EqRegName = TRI.getRegAsmName(RegOffset.RegNum).lower();
        RetVal = getCurretValueInReg(EqRegName);
        if (RetVal != "")
          break;
      }
    }
  }

  return RetVal;
}

void ConcreteReverseExec::execute(const MachineInstr &MI) {
  // If the option is enabled, we skip the CRE of the MIs.
  if (!getIsCREEnabled())
    return;

  // If this instruction modifies any of the registers,
  // update the register values for the function. First definition of the reg
  // is the one that is in the 'regInfo:' (going backward is the first, but it
  // is the latest def actually by going forward).
  auto TRI = MI.getParent()->getParent()->getSubtarget().getRegisterInfo();
  auto TII = MI.getParent()->getParent()->getSubtarget().getInstrInfo();

  auto &MRI = MI.getMF()->getRegInfo();

  // This will be used to avoid implicit operands that can be in the instruction
  // multiple times.
  std::multiset<Register> RegisterWorkList;

  if (TII->isStore(MI) || TII->isPush(MI)) {
    auto OptDestSrc = TII->getDestAndSrc(MI);
    if (OptDestSrc.hasValue()) {
      DestSourcePair &DestSrc = *OptDestSrc;

      if (DestSrc.Destination) {
        auto Reg = DestSrc.Destination->getReg();
        std::string RegName = TRI->getRegAsmName(Reg).lower();

        auto AddrStr = getCurretValueInReg(RegName);
        if (AddrStr == "") {
          AddrStr = getEqRegValue(const_cast<MachineInstr *>(&MI), {Reg}, *TRI);
        }

        // TO DO: Add support for PC relative addressing,
        // needs to add the size of machine instruction to total address size
        if (AddrStr != "" && !(CATI && CATI->isPCRegister(RegName))) {
          uint64_t Addr = 0;
          std::stringstream SS;
          SS << std::hex << AddrStr;
          SS >> Addr;
          if (DestSrc.DestOffset.hasValue()) {
            if (TII->isStore(MI)) {

              Addr += static_cast<uint64_t>(*DestSrc.DestOffset);
              LLVM_DEBUG(llvm::dbgs()
                             << "Store instruction: " << MI << ", Destination: "
                             << "(" << RegName << ")"
                             << "+" << *DestSrc.DestOffset << "\n";);

            } else if (TII->isPush(MI)) {
              // Stack is already aligned on its address
              LLVM_DEBUG(llvm::dbgs()
                             << "Push instruction: " << MI << ", Destination: "
                             << "(" << RegName << ")"
                             << "+" << *DestSrc.DestOffset << "\n";);
            }
            lldb::SBError error;
            // invalidate 8 bytes if size of instruction is not known
            uint32_t byteSize = 8;

            Optional<uint32_t> BitSize = TII->getBitSizeOfMemoryDestination(MI);
            if (BitSize.hasValue()) {
              // TO DO: Check if this is right
              byteSize = (*BitSize) / 8 + (*BitSize % 8 ? 1 : 0);
            }

            Optional<uint64_t> MemValOptional =
                MemWrapper.ReadUnsignedFromMemory(Addr, byteSize, error);
            llvm::dbgs() << error.GetCString() << "\n";
            if (MemValOptional.hasValue() && DestSrc.Source &&
                !DestSrc.Source2) {
              if (!DestSrc.Src2Offset.hasValue() && DestSrc.Source->isReg()) {

                uint64_t MemVal = *MemValOptional;
                std::string SrcRegName =
                    TRI->getRegAsmName(DestSrc.Source->getReg()).lower();
                auto srcRegVal = getCurretValueInReg(SrcRegName);
                writeUIntRegVal(SrcRegName, MemVal, byteSize * 2);
              }
            }
            if (TII->isPush(MI)) {
              writeUIntRegVal(RegName, Addr - (*DestSrc.DestOffset),
                              AddrStr.size() - 2);
            }
            MemWrapper.InvalidateAddress(Addr, byteSize);
            dump();

            // continue;
          }
        }
      }
    }
  }

  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg())
      continue;
    Register Reg = MO.getReg();
    RegisterWorkList.insert(Reg);
    std::string RegName = TRI->getRegAsmName(Reg).lower();

    if (RegisterWorkList.count(Reg) == 1 && MI.modifiesRegister(Reg, TRI)) {
      LLVM_DEBUG(llvm::dbgs() << MI << " modifies " << RegName << "\n";);
      // Here we update the register values.

      // TODO: Handle all posible opcodes here.
      // For all unsupported MIs, we just invalidates the value in reg
      // by setting it to "".

      // If the value of the register isn't available, we have nothing to
      // update.
      // FIXME: Is this right?
      auto regVal = getCurretValueInReg(RegName);
      if (regVal == "") {
        // FIXME: No use of register equivalence here and it even shouldn't be
        // right, is this right? regVal =
        // getEqRegValue(const_cast<MachineInstr*>(&MI), Reg, *TRI); if(regVal
        // == "") continue;
        continue;
      }

      // Skip push/pop intructions here.
      if (TII->isPush(MI) || TII->isPop(MI))
        continue;

      uint64_t Val = 0;
      std::stringstream SS;
      SS << std::hex << regVal;
      SS >> Val;

      // In c_test_cases/test3.c there is a case
      //  $eax = ADD32ri8 $eax(tied-def 0), 1
      // so handle it.
      if (auto RegImm = TII->isAddImmediate(MI, Reg)) {
        // We do the oposite operation, since we are
        // intereting the instruction going backward.
        Val -= RegImm->Imm;
        // Write current value of the register in the map.
        writeUIntRegVal(RegName, Val, regVal.size() - 2);
        dump();
        continue;
      }

      if (TII->isLoad(MI)) {
        auto OptDestSrc = TII->getDestAndSrc(MI);
        if (OptDestSrc.hasValue()) {
          LLVM_DEBUG(llvm::dbgs() << "Load instruction: " << MI;);
          DestSourcePair &DestSrc = *OptDestSrc;

          if (DestSrc.Source && DestSrc.Source->isReg() &&
              DestSrc.SrcOffset.hasValue()) {
            Register SrcReg = DestSrc.Source->getReg();
            std::string SrcRegStr = TRI->getRegAsmName(SrcReg).lower();
            // TO DO: Add support for PC relative addressing,
            // needs to add the size of machine instruction to total address
            // size
            if (CATI && CATI->isPCRegister(SrcRegStr)) {
              invalidateRegVal(RegName);
              dump();
              continue;
            }
            auto srcRegVal = getCurretValueInReg(SrcRegStr);
            if (srcRegVal == "") {
              srcRegVal =
                  getEqRegValue(const_cast<MachineInstr *>(&MI), SrcReg, *TRI);
              if (srcRegVal == "")
                continue;
            }

            // Cannot know value of memory if loading reg from (reg)offset,
            // unless RegisterEquivalence has some equivalent registers
            if (DestSrc.Source->getReg() == Reg) {
              // TO DO: Machine Instr at the beginning of basic block
              if (MI.getIterator() != MI.getParent()->begin()) {
                // TO DO: Check if this is right in all situations
                srcRegVal = getEqRegValue(
                    const_cast<MachineInstr *>(&*std::prev(MI.getIterator())),
                    Reg, *TRI);
                if (srcRegVal == "") {
                  invalidateRegVal(RegName);
                  dump();
                  continue;
                }
              } else {
                invalidateRegVal(RegName);
                dump();
                continue;
              }
            }

            uint64_t Addr;
            std::istringstream(srcRegVal) >> std::hex >> Addr;
            Addr += static_cast<uint64_t>(*DestSrc.SrcOffset);

            // TO DO: Check if this is right
            uint32_t bitSize =
                TRI->getRegSizeInBits(DestSrc.Destination->getReg(), MRI);
            uint32_t byteSize = bitSize / 8 + (bitSize % 8 ? 1 : 0);

            lldb::SBError error;
            MemWrapper.WriteMemory(Addr, &Val, byteSize, error);
            invalidateRegVal(RegName);
            dump();
            continue;
          }
        }
      }

      // FIXME: This isn't right, since current instruction shouldn't
      // be using the new value.
      /*else if (MI.isMoveImmediate()) {
       if (!MI.getOperand(1).isImm()) {
         updateCurrRegVal(RegName, "");
         return;
       }
       Val = MI.getOperand(1).getImm();
       std::stringstream SS;
       SS << std::hex << regVal;
       SS >> Val;
       // Write current value of the register in the map.
       writeUIntRegVal(RegName, Val, regVal.size() - 2);

       dump();
       return;
     }*/

      // The MI is not supported, so consider it as not available.
      LLVM_DEBUG(llvm::dbgs() << "Concrete Rev Exec not supported for \n";
                 MI.dump(););
      // Invalidate register value, since it is not available.
      invalidateRegVal(RegName);
      dump();
    }
  }
}
