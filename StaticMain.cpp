//========================================================================
// FILE:
//    StaticMain.cpp
//
// DESCRIPTION:
//    A command-line tool that do dominators and liveout analysis in the input LLVM file. Internally it uses the
//    DominatorsAnalysis and LivenessAnalysis passes.
//
// USAGE:
//    # First, generate an LLVM file:
//      clang -emit-llvm <input-file> -o <output-llvm-file>
//    # Now you can run this tool as follows:
//      <BUILD/DIR>/bin/static <output-llvm-file>
//
// License: MIT
//========================================================================
#include "DominatorsAnalysis.h"
#include "LivenessAnalysis.h"

#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

enum MyAnalysis{
  DOMINATORS,
  LIVENESS
};

//===----------------------------------------------------------------------===//
// Command line options
//===----------------------------------------------------------------------===//
static cl::OptionCategory AnalysisCategory{"analysis options"};

static cl::opt<std::string> InputModule{cl::Positional,
                                        cl::desc{"<Module to analyze>"},
                                        cl::value_desc{"bitcode filename"},
                                        cl::init(""),
                                        cl::Required,
                                        cl::cat{AnalysisCategory}};

static cl::opt<MyAnalysis> AnalysisType{
      "analysis", cl::desc("Choose an analysis."),
      //cl::init(JITKind::Orc),
      cl::Required,
      cl::values(clEnumValN(MyAnalysis::DOMINATORS, "dom", "Dominators"),
                 clEnumValN(MyAnalysis::LIVENESS, "liveout", "Liveness analysis")),
      cl::cat{AnalysisCategory}};


//===----------------------------------------------------------------------===//
// static - implementation
//===----------------------------------------------------------------------===//
static void doAnalysis(Module &M, MyAnalysis MA) {
  // Create a function pass manager and add the specified pas to it.
  FunctionPassManager FPM;
  if(MA == MyAnalysis::DOMINATORS){
      DominatorsAnalysisPrinter DAP(llvm::errs());
      FPM.addPass(DAP);
  } else {
      LivenessAnalysisPrinter LAP(llvm::errs());
      FPM.addPass(LAP);
  }


  // Create an analysis manager and register the analysis pass with it.
  FunctionAnalysisManager FAM;
  //FAM.registerPass([&] { return DominatorsAnalysis(); });
  FAM.registerPass([&] { return LivenessAnalysis(); });
  FAM.registerPass([&] { return DominatorsAnalysis(); });

  // Register all available module analysis passes defined in PassRegisty.def.
  // We only really need PassInstrumentationAnalysis (which is pulled by
  // default by PassBuilder), but to keep this concise, let PassBuilder do all
  // the _heavy-lifting_.
  PassBuilder PB;
  PB.registerFunctionAnalyses(FAM);

  // Finally, run the passes registered with MPM
  for(auto &F : M){
     llvm::errs() << "=====Function: " << F.getName() << "=====\n";
     FPM.run(F, FAM);
  }
 
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//
int main(int Argc, char **Argv) {
  // Hide all options apart from the ones specific to this tool
  cl::HideUnrelatedOptions(AnalysisCategory);

  cl::ParseCommandLineOptions(Argc, Argv,
                              "Discover the dominators set for each"
                              "basic block using fixed point approach or"
                              "do liveout analysis for each instruction \n");

  // Makes sure llvm_shutdown() is called (which cleans up LLVM objects)
  //  http://llvm.org/docs/ProgrammersManual.html#ending-execution-with-llvm-shutdown
  llvm_shutdown_obj SDO;

  // Parse the IR file passed on the command line.
  SMDiagnostic Err;
  LLVMContext Ctx;
  std::unique_ptr<Module> M = parseIRFile(InputModule.getValue(), Err, Ctx);
  MyAnalysis analysis = AnalysisType.getValue();

  if (!M) {
    errs() << "Error reading bitcode file: " << InputModule << "\n";
    Err.print(Argv[0], errs());
    return -1;
  }

  // Run the analysis and print the results
  doAnalysis(*M, analysis);

  return 0;
}
