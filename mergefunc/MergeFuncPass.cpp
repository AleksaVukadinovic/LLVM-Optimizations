#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_map>
#include <unordered_set>

using namespace llvm;

namespace {
struct MergeFuncPass : public ModulePass {
  static char ID;
  MergeFuncPass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    std::unordered_map<size_t, std::vector<Function *>> Buckets;
    
    for (Function &F : M) {
        if (F.isDeclaration()) continue;
        size_t H = hashFunction(F);
        Buckets[H].push_back(&F);
    }

    bool Changed = false;
    std::unordered_set<Function*> FunctionsToErase;

    for (auto &P : Buckets) {
        auto &Funcs = P.second;
        for (int i = 0; i < Funcs.size(); i++) {
            Function *A = Funcs[i];
            if (!A || FunctionsToErase.count(A)) continue;

            for (int j = i + 1; j < Funcs.size(); j++) {
                Function *B = Funcs[j];
                if (!B || FunctionsToErase.count(B)) continue;

                if (areFunctionsEquivalent(*A, *B)) {
                    errs() << "Merging " << B->getName() << " into " << A->getName() << "\n";
                    
                    B->replaceAllUsesWith(A);
                    FunctionsToErase.insert(B);
                    Changed = true;
                }
            }
        }
    }

    for (Function *F : FunctionsToErase) {
        assert(F->use_empty());
        F->eraseFromParent();
    }

    return Changed;
}

  size_t hashFunction(Function &F) {
    size_t H = 0;
    for (auto &BB : F)
      for (auto &I : BB)
        H ^= std::hash<unsigned>()(I.getOpcode() + 31 * I.getNumOperands());
    return H;
  }

  bool areFunctionsEquivalent(Function &A, Function &B) {
      if (A.getAttributes() != B.getAttributes())
          return false;
      if (A.hasGC() != B.hasGC())
          return false;
      if (A.getSection() != B.getSection())
          return false;
      if (A.isVarArg() != B.isVarArg())
          return false;
      if (A.getCallingConv() != B.getCallingConv())
          return false;
      if (A.getReturnType() != B.getReturnType())
          return false;
      if (A.arg_size() != B.arg_size())
          return false;

      // argument types
      auto ArgItA = A.arg_begin();
      auto ArgItB = B.arg_begin();
      for (; ArgItA != A.arg_end(); ArgItA++, ArgItB++) {
          if (ArgItA->getType() != ArgItB->getType())
              return false;
      }

      // instruction comparison
      auto ItA = inst_begin(A), EndA = inst_end(A);
      auto ItB = inst_begin(B), EndB = inst_end(B);
      for (; ItA != EndA && ItB != EndB; ItA++, ItB++) {
        if (ItA->getOpcode() != ItB->getOpcode())
          return false;

        // if return, check return values
        if (auto *RA = dyn_cast<ReturnInst>(&*ItA)) {
          auto *RB = dyn_cast<ReturnInst>(&*ItB);
          if (!RB) return false;
          if (RA->getReturnValue() != RB->getReturnValue())
            return false;
        }
      }

      return ItA == EndA && ItB == EndB;
  }

};
}

char MergeFuncPass::ID = 0;
static RegisterPass<MergeFuncPass> X("simple-mergefunc", "Simple Function Merge");
