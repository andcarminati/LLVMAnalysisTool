//=============================================================================
// FILE:
//    LivenessAnalysis.cpp
//
// DESCRIPTION:
//    This pass implements Dominance Analysis
//
// USAGE:
//    1. Legacy PM
//      opt -load libLivenessAnalysis.dylib -legacy-dominators -disable-output `\`
//        <input-llvm-file>
//    2. New PM
//      opt -load-pass-plugin=libLivenessAnalysis.dylib -passes="dominance" `\`
//        -disable-output <input-llvm-file>
//
//
// License: MIT
//=============================================================================
#include "LivenessAnalysis.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;




//-----------------------------------------------------------------------------
// DominatorsAnalysis implementation
//-----------------------------------------------------------------------------

// This method implements what the pass does
ResultLivenessAnalysis LivenessAnalysis::run(Function &F, FunctionAnalysisManager &MAM) {
  return runOnFunction(F);
}

ResultLivenessAnalysis LivenessAnalysis::runOnFunction(Function &F){
    ResultLivenessAnalysis res;
    BBLiveOutSet UEVar, VarKill;
    BBLiveOutSet &LiveOut = res.ResultBBLiveOut;
    InstLiveOutSet &InstVSet = res.ResultInstLiveOut;

      for (auto &BB : F){
        auto &BBUEVar = UEVar.insert(std::make_pair(&BB, llvm::SmallPtrSet<const llvm::Value*, 8>())).first->second;
        auto &BBVarKill = VarKill.insert(std::make_pair(&BB, llvm::SmallPtrSet<const llvm::Value*, 8>())).first->second;
        LiveOut.insert(std::make_pair(&BB, llvm::SmallPtrSet<const llvm::Value*, 8>()));
        for(const auto &Inst : BB){
            if(!isa<PHINode>(Inst)){
                for (const Use &U : Inst.operands()) {
                    Value *v = U.get();

                    if(!BBVarKill.contains(v) && !isa<BasicBlock>(*v) && !isa<GlobalVariable>(*v) && !isa<Constant>(*v)){
                        BBUEVar.insert(v);
                    }
                }
            } 
            if(!Inst.user_empty()){
                BBVarKill.insert(&Inst);
            }
        } 
    }  
    bool changed =  true;
    while(changed){
        changed = false;
        for (const auto &BB : F){
            // recompute liveout
            llvm::SmallPtrSet<const llvm::Value*, 8> NewLiveOut;
            const auto& BBLiveOut = LiveOut[&BB];
            for(auto IT = succ_begin(&BB); IT != succ_end(&BB); IT++){
                const auto SuccBB = *IT;
                const auto& SuccLiveOut = LiveOut[SuccBB];
                const auto& SuccUEVar = UEVar[SuccBB];
                const auto& SuccVarKill = VarKill[SuccBB];
                llvm::SmallPtrSet<const llvm::Value*, 8> TmpLiveOut;
                for(auto V : SuccLiveOut ){
                    TmpLiveOut.insert(V);
                }
                for(auto V : SuccVarKill ){
                    TmpLiveOut.erase(V);
                }
                for(auto V : SuccUEVar ){
                    TmpLiveOut.insert(V);
                }
                for(auto V : TmpLiveOut){
                    NewLiveOut.insert(V);
                }
            }
            if(NewLiveOut != BBLiveOut){
                changed = true;
                LiveOut[&BB] = NewLiveOut;
            }
        }    
    }
    // handle phi nodes of BBs that are successors of themselves 
    for (auto &BB : F){
        for(const auto &Inst : BB){
            if(isa<PHINode>(Inst)){
                for (const Use &U : Inst.operands()) {
                    Value *v = U.get();
                    auto *I = dyn_cast<Instruction>(v);
                    if (I && I->getParent() == &BB) {
                        LiveOut[&BB].insert(v);
                    } 
                }    
            }
        }
    }
    for (const auto &BB : F){
        auto BBLiveOut = LiveOut[&BB];
        for(auto RIT = BB.rbegin(); RIT != BB.rend(); RIT++){
            const auto& Inst = *RIT;
            InstVSet[&Inst] = BBLiveOut;
            for (const Use &U : Inst.operands()) {
                Value *v = U.get();
                if(!isa<BasicBlock>(*v) && !isa<GlobalVariable>(*v) && !isa<Constant>(*v)){
                    BBLiveOut.insert(v);
                }
            }
            BBLiveOut.erase(&Inst);
        }
    }    
    return res;
}


// Dominator printer implementation
PreservedAnalyses
LivenessAnalysisPrinter::run(Function &F,
                              FunctionAnalysisManager &FAM) {

    auto Liveness = FAM.getResult<LivenessAnalysis>(F);
    for (auto &BB : F){
        OS << "=====Basic block: " << BB.getNameOrAsOperand() << "=====\n";
        for(auto& I : BB){
            I.print(errs());
            errs() << "\n{";
            for(const auto v : Liveness.ResultInstLiveOut[&I]){
                errs() << v->getNameOrAsOperand() << " ";
            }
            errs() << "}\n";
        }    
    }     
    return PreservedAnalyses::all();
}




//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
AnalysisKey LivenessAnalysis::Key;

llvm::PassPluginLibraryInfo getLivenessAnalysisPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "liveness", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "print<liveness>") {
                    FPM.addPass(LivenessAnalysisPrinter(llvm::errs()));
                    return true;
                  }
                  return false;
                });
             
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &MAM) {
                  MAM.registerPass([&] { return LivenessAnalysis(); });
                });
          }};    
          
          
         // };
}



// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize LivenessAnalysis when added to the pass pipeline on the
// command line, i.e. via '-passes=liveness'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getLivenessAnalysisPluginInfo();
}