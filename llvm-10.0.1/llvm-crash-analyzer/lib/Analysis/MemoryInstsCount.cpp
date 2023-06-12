#include "Analysis/MemoryInstsCount.h"

// static cl::opt<bool> PrintMemoryInstsCount(
//     "print-memory-insts-count",
//     cl::desc("Enables printing of memory insts"),
//     cl::init(false)
// );



#define DEBUG_TYPE "memory-insts-count"


bool MemoryInstsCount::runOnBlameModule(const BlameModule& BM)
{
    bool returnValue = false;
    for(const crash_analyzer::BlameFunction& BF : BM)
    {
        if(BF.MF)  returnValue = returnValue || this->runOnMachineFunction(*BF.MF);
    }
    return returnValue;
}

bool MemoryInstsCount::runOnMachineFunction(const MachineFunction& MF)
{
    bool returnValue = false;
    unsigned numOfMemIns = 0;
    auto TII = MF.getSubtarget().getInstrInfo();
    for(const MachineBasicBlock& MB : MF)
    {
        for(const MachineInstr& MI : MB)
        {
            const MCInstrDesc& MID = MI.getDesc();
            int numOfOperands = MI.getNumOperands();
            if(MI.isReturn() || MI.isCall())
            {
                numOfMemIns++;
                continue;
            }

                

            // if(MI.getNumMemOperands() > 0)
            // {
            //     numOfMemIns++;
            //     continue;
            // }
            
            if(TII->isPush(MI) || TII->isPop(MI))
            {
                numOfMemIns++;
                continue;
            }

            for(int i = 0; i < numOfOperands; i++)
            {
                if(MID.OpInfo[i].OperandType == MCOI::OperandType::OPERAND_MEMORY)
                {
                    numOfMemIns++;
                    break;
                }
            }
        }
    }

    llvm::outs() << MF.getName() << ": " << numOfMemIns << "\n";

    return returnValue;
}

bool MemoryInstsCount::runOnMIRFile(StringRef MIRFile, CoreFile& coreFile)
{

    LLVMContext Context;
    lldb::SBTarget& SBT = coreFile.getTarget();
    SMDiagnostic Err;
    std::unique_ptr<Module> M;
    std::unique_ptr<MIRParser> MIR;
    std::string TheTripleString = SBT.GetTriple();
    Triple TheTriple(Triple::normalize(TheTripleString));
    std::string TripleName;
    std::string Error;
    std::string CPUStr = "";
    std::string FeaturesStr = "";
    bool returnValue = false;

    if(MIRFile == "" || !MIRFile.endswith(".mir")) return returnValue;

    const Target* TheTarget = TargetRegistry::lookupTarget(TripleName, TheTriple, Error);
    if(!TheTarget){outs() << "No target" << "\n"; return returnValue;}

    std::unique_ptr<TargetMachine> TM(TheTarget->createTargetMachine(TheTripleString, CPUStr, FeaturesStr, TargetOptions(), None));
    if(!TM){/*error*/ return false;} 

    LLVMTargetMachine& LLVMTM = static_cast<LLVMTargetMachine &> (*TM.get());

    //Context not initialized
    auto setMIRFunctionAttributes = [&CPUStr, &FeaturesStr] (Function& F)
    {
        //leave it empty for now
    };

    MIR = createMIRParserFromFile(MIRFile, Err, Context, setMIRFunctionAttributes);

    M = MIR->parseIRModule();
    M->setTargetTriple(Triple::normalize(TheTripleString));
    M->setDataLayout(TM->createDataLayout());
    auto MMI =  std::make_unique<MachineModuleInfo>(&LLVMTM);
    if(!MMI) {/*error*/ return false;}
    
    
    MIR->parseMachineFunctions(*M, *MMI);

    for(const Function& F: M->getFunctionList())
    {
        const MachineFunction* MF = MMI->getMachineFunction(F);
        if(MF) returnValue = runOnMachineFunction(*MF) || returnValue;

    }

    return returnValue;
}