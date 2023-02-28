// This file is part of SymCC.
//
// SymCC is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// SymCC is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// SymCC. If not, see <https://www.gnu.org/licenses/>.

#include "Pass.h"

#if LLVM_VERSION_MAJOR >= 13
// Here we might have a problem that if we are using MPM we need go to FPM level of granularity,
// Though if we are in Vectorizer Pipeline we need go to MPM level of granularity at least for SymbolizePass::init(...)
llvm::PassPluginLibraryInfo getSymbolizePassPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "symbolize", LLVM_VERSION_STRING, [](llvm::PassBuilder &PB){
        llvm::errs() << "[INFO] Registering our SymbolizePass...\n";
        PB.registerVectorizerStartEPCallback([](llvm::FunctionPassManager &FPM,
                                                llvm::PassBuilder::OptimizationLevel level){
                                                    FPM.addPass(SymbolizePass());
                                                });
        // @Info(alekum): there is no exact mapping on legacy pass manager's EP_EnableOnOptLevel0
        // we are use this and should experiment with pipeline(registerOptimizerEarlyEPCallback,
        // registerScalarOptimizerLateEPCallback....)
        PB.registerOptimizerLastEPCallback([](llvm::ModulePassManager &MPM,
                                              llvm::PassBuilder::OptimizationLevel level){
                                                MPM.addPass(SymbolizePass());
                                              });
    }};
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() { return getSymbolizePassPluginInfo(); }

#else

void addSymbolizePass(const llvm::PassManagerBuilder & /* unused */,
                      llvm::legacy::PassManagerBase &PM) {
  // We do this only for pure concolic execution.
  PM.add(llvm::createLowerSwitchPass());
  PM.add(new SymbolizePass());
}

// Make the pass known to opt.
static llvm::RegisterPass<SymbolizePass> X("symbolize", "Symbolization Pass");
// Tell frontends to run the pass automatically.
static struct llvm::RegisterStandardPasses
    Y(llvm::PassManagerBuilder::EP_VectorizerStart, addSymbolizePass);
static struct llvm::RegisterStandardPasses
    Z(llvm::PassManagerBuilder::EP_EnabledOnOptLevel0, addSymbolizePass);

#endif // LLVM_VERSION_MAJOR