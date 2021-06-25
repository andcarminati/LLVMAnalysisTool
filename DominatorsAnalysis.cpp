//=============================================================================
// FILE:
//    DominatorsAnalysis.cpp
//
// DESCRIPTION:
//    This pass implements Dominance Analysis
//
// USAGE:
//    1. Legacy PM
//      opt -load libDominatorsAnalysis.dylib -legacy-dominators -disable-output `\`
//        <input-llvm-file>
//    2. New PM
//      opt -load-pass-plugin=libDominatorsAnalysis.dylib -passes="dominance" `\`
//        -disable-output <input-llvm-file>
//
//
// License: MIT
//=============================================================================
#include "DominatorsAnalysis.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;


// Pretty-prints the result of this analysis
static void printDominatorsResult(llvm::raw_ostream &OutS,
                         const ResultDominators &Dominators);

//-----------------------------------------------------------------------------
// DominatorsAnalysis implementation
//-----------------------------------------------------------------------------

// This method implements what the pass does
ResultDominators DominatorsAnalysis::run(Function &F, FunctionAnalysisManager &MAM) {
  return runOnFunction(F);
}

ResultDominators DominatorsAnalysis::runOnFunction(Function &F){
    ResultDominators res;
    Dominators &dom = res.dom;

    for(auto &BB : F){
      // Insert dom(entry) = entry
      if(BB.isEntryBlock()){
        auto set = dom.insert(std::make_pair(&BB, llvm::SmallPtrSet<const llvm::BasicBlock*, 8>())).first;
        set->second.insert(&BB);

      } else {
        // dom(i != entry) = N (all BBs)
        auto set = dom.insert(std::make_pair(&BB, llvm::SmallPtrSet<const llvm::BasicBlock*, 8>())).first;
        for(auto &EveryBB: F){
          set->second.insert(&EveryBB);
        }
      }
    }

    bool changed = true;
    while(changed){
      changed = false;
      auto IT = F.begin();
      IT++; // ignore first;
  
      // For each BB different from entry
      for( ; IT != F.end(); IT++){
        llvm::SmallPtrSet<const llvm::BasicBlock*, 8> NewSet;
        llvm::MapVector<const llvm::BasicBlock*, unsigned> AuxMap; // track BB counts for intersection purposes
        auto const &BB = *IT;
        auto ActualSet = dom[&BB];
        const_pred_iterator PI = pred_begin(&BB), E = pred_end(&BB);
        unsigned NPred = pred_size(&BB);

        // For each predecessor of BB
        for(; PI != E; PI++){
          // get the dominator set of each BB
          auto PredSet = dom[*PI];

          //Add each BB in dominator set, on NewSet
          for(auto TempBB : PredSet){
            NewSet.insert(TempBB);
            auto ITMap = AuxMap.find(TempBB);
            if(AuxMap.end() == ITMap){
              ITMap = AuxMap.insert(std::make_pair(TempBB, 0)).first;
            }
            // Track the occurrence of BB in all sets
            ITMap->second++;
          }
        }
        
        // clear elements that are not in the intersection
        // ie. does not appear in all sets (count < NPred)
        for(auto ITMap : AuxMap){
          if(ITMap.second < NPred){
            NewSet.erase(ITMap.first);
          }
        }  

        // BB must be in this set
        NewSet.insert(&BB); 
        // If the se changes, use the new set and go again
        if(ActualSet != NewSet){
          dom[&BB] = NewSet;
          changed = true;
        }
      }
    }

    return res;
}

bool ResultDominators::invalidate(
    Function &F, const PreservedAnalyses &PA,
    FunctionAnalysisManager::Invalidator &Inv) {
      
  auto PAC = PA.getChecker<DominatorsAnalysis>();
  return !(PAC.preserved() || PAC.preservedSet<AllAnalysesOn<Function>>());
}

const BBPtrSet& ResultDominators::getBBDominators(const llvm::BasicBlock* BB) const{
    auto &domSet = dom.find(BB)->second;
 
    return domSet;
}

  const Dominators::const_iterator ResultDominators::dominatorsBegin() const{
    return begin(dom);
  }

  const Dominators::const_iterator ResultDominators::dominatorsEnd() const {
    return end(dom);
  }


// Legacy PM implementation
bool LegacyDominatorsAnalysis::runOnFunction(Function &F) {
    dom = Impl.runOnFunction(F);
    return false;
}

void LegacyDominatorsAnalysis::print(raw_ostream &OutS, Module const *) const {
  printDominatorsResult(OutS, dom);
}


// Dominator printer implementation
PreservedAnalyses
DominatorsAnalysisPrinter::run(Function &M,
                              FunctionAnalysisManager &MAM) {

  auto Dominators = MAM.getResult<DominatorsAnalysis>(M);
  printDominatorsResult(OS, Dominators);
  return PreservedAnalyses::all();
}


//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
AnalysisKey DominatorsAnalysis::Key;

llvm::PassPluginLibraryInfo getDominatorsAnalysisPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "dom", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "print<dom>") {
                    FPM.addPass(DominatorsAnalysisPrinter(llvm::errs()));
                    return true;
                  }
                  return false;
                });
              
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &MAM) {
                  MAM.registerPass([&] { return DominatorsAnalysis(); });
                });
          }};    
          
          
         // };
}



// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize DominatorsAnalysis when added to the pass pipeline on the
// command line, i.e. via '-passes=dom'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getDominatorsAnalysisPluginInfo();
}

//-----------------------------------------------------------------------------
// Legacy PM Registration
//-----------------------------------------------------------------------------
// The address of this variable is used to uniquely identify the pass. The
// actual value doesn't matter.
char LegacyDominatorsAnalysis::ID = 0;

// This is the core interface for pass plugins. It guarantees that 'opt' will
// recognize LegacyDominatorsAnalysis when added to the pass pipeline on the command
// line, i.e.  via '--legacy-dominators'
static RegisterPass<LegacyDominatorsAnalysis>
    X("legacy-dominators", "Dominator Analysis",
      true, // This pass doesn't modify the CFG => true
      false // This pass is not a pure analysis pass => false
    );

//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------
static void printDominatorsResult(llvm::raw_ostream &OutS,
                         const ResultDominators &dominators){ 
 
    auto IT = dominators.dominatorsBegin();
    auto ITE = dominators.dominatorsEnd();    

    for(; IT != ITE; IT++){
      auto BB = IT->first;
      auto DomSet = IT->second;
      OutS << "(DominatorsAnalysis) Basic Block "<< BB->getNameOrAsOperand() << "{ ";
      for(auto DomBB : DomSet){
        OutS << DomBB->getNameOrAsOperand() << " ";
      }
      OutS << "}\n";
    }
                         
}