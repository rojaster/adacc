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

#ifndef PASS_H
#define PASS_H

#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/Pass.h"

/// @Cleanup(alekum) maybe we should hide this in a namespace...
class SymbolizePassImpl final {
public:
  /// Mapping from global variables to their corresponding symbolic expressions.
  //using GSMap = llvm::ValueMap<llvm::GlobalVariable*, llvm::GlobalVariable*>;

  SymbolizePassImpl() = default;
  virtual ~SymbolizePassImpl() = default;
  SymbolizePassImpl(const SymbolizePassImpl&) = default;
  SymbolizePassImpl& operator=(const SymbolizePassImpl&) = default;
  SymbolizePassImpl(SymbolizePassImpl&&) = default;
  SymbolizePassImpl& operator=(SymbolizePassImpl&&) = default;

  bool init(llvm::Module &M);
  bool run(llvm::Function &F);

private:
  //GSMap globalExpressions;
  //static constexpr char kSymCtorName[] = "__sym_ctor";
};


/// @Information(alekum): Assume we start support NewPM 
/// Should we keep LegacyPM support?
#if LLVM_VERSION_MAJOR >= 13

class SymbolizePass : public llvm::PassInfoMixin<SymbolizePass> {
public: 
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);
  llvm::PreservedAnalyses run(llvm::Function &M, llvm::FunctionAnalysisManager &FAM);
  static bool isRequired() { return true; }
private:
  SymbolizePassImpl SPI;
};

#else

class SymbolizePass : public llvm::FunctionPass {
public:
  static char ID;

  SymbolizePass() : FunctionPass(ID) {}

  bool doInitialization(llvm::Module &M) override;
  bool runOnFunction(llvm::Function &F) override;
private:
  SymbolizePassImpl SPI;
};

#endif // LLVM_VERSION_MAJOR


#endif
