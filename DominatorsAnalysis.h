//========================================================================
// FILE:
//    DominatorsAnalysis.h
//
// DESCRIPTION:
//    Declares the DominatorsAnalysis Passes
//      * new pass manager interface
//      * legacy pass manager interface
//      * printer pass for the new pass manager
//
// License: MIT
//========================================================================
#ifndef LLVM_DOMINATORSANALYSIS_H
#define LLVM_DOMINATORSANALYSIS_H

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
namespace llvm{

using BBPtrSet = llvm::SmallPtrSet<const llvm::BasicBlock*, 8>;
using Dominators = llvm::MapVector<const llvm::BasicBlock *, BBPtrSet>;


struct ResultDominators {
public:  
  Dominators dom;    

  bool invalidate(Function &F, const PreservedAnalyses &PA,
                    FunctionAnalysisManager::Invalidator &Inv);

  const BBPtrSet& getBBDominators(const llvm::BasicBlock* BB) const;
  const Dominators::const_iterator dominatorsBegin() const;
  const Dominators::const_iterator dominatorsEnd() const;
  
};

class DominatorsAnalysis : public AnalysisInfoMixin<DominatorsAnalysis> {
public:
  using Result = ResultDominators;

  Result run(Function &F, FunctionAnalysisManager &AM);
  Result runOnFunction(Function &F);
private:
  // A special type used by analysis passes to provide an address that
  // identifies that particular analysis pass type.
  static llvm::AnalysisKey Key;
  friend struct llvm::AnalysisInfoMixin<DominatorsAnalysis>;
};

//------------------------------------------------------------------------------
// Legacy PM interface
//------------------------------------------------------------------------------

class LegacyDominatorsAnalysis : public FunctionPass {
public:  
  static char ID;
  LegacyDominatorsAnalysis() : FunctionPass(ID) {}
  // Main entry point - the name conveys what unit of IR this is to be run on.
  bool runOnFunction(Function &F) override;
  void print(llvm::raw_ostream &OutS, llvm::Module const *M) const override;
private:  
  ResultDominators dom;
  DominatorsAnalysis Impl;
};


//------------------------------------------------------------------------------
// New PM interface for the printer pass
//------------------------------------------------------------------------------
class DominatorsAnalysisPrinter
    : public llvm::PassInfoMixin<DominatorsAnalysisPrinter> {
public:
  explicit DominatorsAnalysisPrinter(llvm::raw_ostream &OutS) : OS(OutS) {}
  PreservedAnalyses run(Function &M, FunctionAnalysisManager &MAM);

private:
  llvm::raw_ostream &OS;
};



}; // End namespace llvm


#endif // LLVM_DOMINATORSANALYSIS_H