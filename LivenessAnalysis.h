//========================================================================
// FILE:
//    LivenessAnalysis.h
//
// DESCRIPTION:
//    Declares the LivenessAnalysis Passes
//      * new pass manager interface
//      * legacy pass manager interface
//      * printer pass for the new pass manager
//
// License: MIT
//========================================================================
#ifndef LLVM_LIVENESSANALYSIS_H
#define LLVM_LIVENESSANALYSIS_H

#include "llvm/ADT/MapVector.h"
#include "llvm/IR/AbstractCallSite.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

//------------------------------------------------------------------------------
// New PM interface
//------------------------------------------------------------------------------
namespace llvm
{

  using ValueSet = llvm::SmallPtrSet<const llvm::Value *, 8>;
  using BBLiveOutSet = llvm::MapVector<const llvm::BasicBlock *, ValueSet>;
  using InstLiveOutSet = llvm::MapVector<const llvm::Instruction *, ValueSet>;

  struct ResultLivenessAnalysis
  {
    BBLiveOutSet ResultBBLiveOut;
    InstLiveOutSet ResultInstLiveOut;
  };

  class LivenessAnalysis : public AnalysisInfoMixin<LivenessAnalysis>
  {
  public:
    using Result = ResultLivenessAnalysis;

    Result run(Function &F, FunctionAnalysisManager &AM);
    Result runOnFunction(Function &F);

  private:
    // A special type used by analysis passes to provide an address that
    // identifies that particular analysis pass type.
    static llvm::AnalysisKey Key;
    friend struct llvm::AnalysisInfoMixin<LivenessAnalysis>;
  };

  //------------------------------------------------------------------------------
  // New PM interface for the printer pass
  //------------------------------------------------------------------------------
  class LivenessAnalysisPrinter
      : public llvm::PassInfoMixin<LivenessAnalysisPrinter>
  {
  public:
    explicit LivenessAnalysisPrinter(llvm::raw_ostream &OutS) : OS(OutS) {}
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);

  private:
    llvm::raw_ostream &OS;
  };

}; // End namespace llvm

#endif // LLVM_LIVENESSANALYSIS_H