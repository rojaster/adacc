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

#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include "llvm/Transforms/Utils.h"
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include "Runtime.h"
#include "Symbolizer.h"

//#include <time.h>
#include <ctime>

#include <random>

#ifndef NDEBUG
#define DEBUG(X)                                                               \
  do {                                                                         \
    X;                                                                         \
  } while (false)
#else
#define DEBUG(X) ((void)0)
#endif

bool SymbolizePassImpl::init(llvm::Module &M) {
  if (!getenv("SYMCC_SILENT")) {
    llvm::errs() << "[INFO] Symbolizer module init\n" 
          << "[INFO] Going through the symboliser \n"
          << "[INFO] Analysing filename " << M.getSourceFileName() << "\n";
  }
  // Redirect calls to external functions to the corresponding wrappers and
  // rename internal functions.
  for (auto &function : M.functions()) {
    auto name = function.getName();
    //errs() << "Getting function: " << name << "\n";
    if (isInterceptedFunction(function))
      function.setName(name + "_symbolized");
  }

  // Insert a constructor that initializes the runtime and any globals.
  llvm::Function *ctor;
  std::tie(ctor, std::ignore) = llvm::createSanitizerCtorAndInitFunctions(
      M, "__sym_ctor", "_sym_initialize", {}, {});
      //M, kSymCtorName, "_sym_initialize", {}, {});
  llvm::appendToGlobalCtors(M, ctor, 0);

  // Add a dtor function for cleaning up paths.
  llvm::IRBuilder<> IRB(M.getContext());
  llvm::Type *void_type = IRB.getVoidTy();
  llvm::FunctionType *FT = llvm::FunctionType::get(void_type, void_type, false);
  llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "__dtor_runtime", M);

  llvm::appendToGlobalDtors(M, M.getFunction("__dtor_runtime"), 0);

  return true;
}

bool SymbolizePassImpl::run(llvm::Function &F) {
  auto functionName = F.getName();
  //if (functionName == kSymCtorName)
  if (functionName == "__sym_ctor")
    return false;

  if (functionName.find("sanitizer_cov_trace") != std::string::npos) {
     return false;
  }

  llvm::errs() << "[INFO] Symbolizing function ";
  llvm::errs().write_escaped(functionName) << '\n';

  llvm::SmallVector<llvm::Instruction *, 0> allInstructions;
  allInstructions.reserve(F.getInstructionCount());
  for (auto &I : instructions(F)) allInstructions.push_back(&I);

  Symbolizer symbolizer(*F.getParent());
  symbolizer.symbolizeFunctionArguments(F);
  
  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<std::mt19937::result_type> dist6(1,1000000000);
  
  symbolizer.my_random_number = dist6(rng);
  
  for (auto &basicBlock : F)
    symbolizer.insertBasicBlockNotification(basicBlock);

  for (auto *instPtr : allInstructions)
    symbolizer.visit(instPtr);

  symbolizer.finalizePHINodes();
  symbolizer.shortCircuitExpressionUses();

  // This inserts all of the code for pure concolic execution.
  auto *pureConcolic= getenv("SYMCC_PC");
  if (pureConcolic != nullptr) {
    if (!getenv("SYMCC_SILENT"))
      llvm::errs() << "[INFO] We are instrumenting for pure concolic execution\n";
    for (auto &basicBlock : F) {
        symbolizer.insertCovs(basicBlock);
    }
  } 

  assert(!llvm::verifyFunction(F, &llvm::errs()) &&
         "[ERROR] SymbolizePass produced invalid bitcode");

  return true;
}

#if LLVM_VERSION_MAJOR >=13
llvm::PreservedAnalyses SymbolizePass::run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM){
  llvm::errs() << "[INFO] Analysing filename " << M.getSourceFileName() << "\n";
  SPI.init(M);
  return llvm::PreservedAnalyses::none();
}
llvm::PreservedAnalyses SymbolizePass::run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM){
  llvm::errs() << "[INFO] Analyzing function: "<< F.getName() 
                << " with  number of arguments: " << F.arg_size() << "\n";
  SPI.run(F);
  return llvm::PreservedAnalyses::none();
}
#else
char SymbolizePass::ID = 0;

bool SymbolizePass::doInitialization(llvm::Module &M) {
  return SPI.init(M);
}

bool SymbolizePass::runOnFunction(llvm::Function &F) {
  return SPI.run(F);
}
#endif // LLVM_VERSION_MAJOR