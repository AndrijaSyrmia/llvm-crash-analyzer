#ifndef _MEMORY_INSTS_COUNT_
#define _MEMORY_INSTS_COUNT_

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Support/CommandLine.h"

#include "Decompiler/Decompiler.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/CodeGen/MIRParser/MIRParser.h"
#include "lldb/API/SBTarget.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/Target/TargetMachine.h"


using namespace llvm;
using namespace crash_analyzer;


class MemoryInstsCount {

    public:

    static void MemInsCountInlineAsmDiagHandler(const SMDiagnostic& SMD, void* Context, unsigned LocCookie);

    bool runOnBlameModule(const BlameModule& BM);
    bool runOnMIRFile(StringRef MIRFile);
    bool runOnMachineFunction(const MachineFunction& MF);
    bool runOnMIRFile(StringRef MIRFile, CoreFile& coreFile);

};

#endif