#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"

using namespace llvm;

namespace {
struct OurPass : public LoopPass {
  static char ID;
  OurPass() : LoopPass(ID) {}

  bool runOnLoop(Loop *L, LPPassManager &LPM) override {
    DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    AliasAnalysis &AA = getAnalysis<AAResultsWrapperPass>().getAAResults();

    return runLICM(L, DT, AA);
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<AAResultsWrapperPass>();
    AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addPreserved<AAResultsWrapperPass>();
  }

private:
  bool runLICM(Loop *L, DominatorTree &DT, AliasAnalysis &AA) {
    bool Changed = false;

    BasicBlock *Preheader = L->getLoopPreheader();
    if (!Preheader)
      return false;

    SmallVector<Instruction *, 16> ToHoist;

    for (BasicBlock *BB : L->getBlocks()) {
      for (Instruction &I : *BB) {
        if (isLoopInvariant(I, L, AA) && isSafeToHoist(I, L, DT))
          ToHoist.push_back(&I);
      }
    }

    for (Instruction *I : ToHoist) {
      I->moveBefore(Preheader->getTerminator());
      Changed = true;
    }

    return Changed;
  }

  bool isLoopInvariant(Instruction &I, Loop *L, AliasAnalysis &AA) {
    if (I.mayHaveSideEffects() || I.mayReadOrWriteMemory())
      return false;
    if (isa<PHINode>(&I))
      return false;

    for (Value *Op : I.operands())
      if (Instruction *OpI = dyn_cast<Instruction>(Op))
        if (L->contains(OpI))
          return false;

    return true;
  }

  bool isSafeToHoist(Instruction &I, Loop *L, DominatorTree &DT) {
    return DT.dominates(I.getParent(), L->getHeader());
  }
};
} // namespace

char OurPass::ID = 0;
static RegisterPass<OurPass>
    X("our-pass", "Our Loop-Invariant Code Motion Pass", false, false);