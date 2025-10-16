#include "llvm/IR/Instruction.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/LoopPeel.h"
#include "llvm/Transforms/Utils/LoopSimplify.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/SizeOpts.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopUnrollAnalyzer.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IRBuilder.h"
#include <algorithm>

using namespace llvm;

namespace {
struct LoopUnrollingPass : public LoopPass {
  std::vector<BasicBlock *> LoopBasicBlocks;
  std::unordered_map<Value *, Value *> VariablesMap;
  Value *LoopCounter, *LoopBound;
  bool isLoopBoundConst;
  int BoundValue;

  static char ID;
  LoopUnrollingPass() : LoopPass(ID) {}

  void MapVariables(Loop *L)
  {
    Function *F = L->getHeader()->getParent();
    for (BasicBlock &BB : *F) {
      for (Instruction &I : BB) {
        if (isa<LoadInst>(&I)) {
          VariablesMap[&I] = I.getOperand(0);
        }
      }
    }
  }

  void findLoopCounterAndBound(Loop *L)
  {
    for (Instruction &I : *L->getHeader()) {
      if (isa<ICmpInst>(&I)) {
        LoopCounter = VariablesMap[I.getOperand(0)];
        if (ConstantInt *ConstInt = dyn_cast<ConstantInt>(I.getOperand(1))) {
          isLoopBoundConst = true;
          BoundValue = ConstInt->getSExtValue();
        }
      }
    }
  }

  void duplicateLoopBody(std::vector<BasicBlock *> LoopBodyBasicBlocks, int numOfTimes, BasicBlock *InsertBefore)
  {
    std::unordered_map<Value *, Value *> Mapping;
    std::unordered_map<Value *, Value *> LoadMapping;
    std::unordered_map<BasicBlock *, BasicBlock *> BlocksMapping;
    IRBuilder<> Builder(InsertBefore->getContext());

    Instruction *Copy;
    BasicBlock *LastFromPreviousCopy = LoopBodyBasicBlocks.back();
    std::vector<BasicBlock *> LoopBodyBasicBlockCopy;

    for (int i = 1; i <= numOfTimes; i++) {
      LoopBodyBasicBlockCopy.clear();

      for (size_t j = 0; j < LoopBodyBasicBlocks.size(); j++) {
          BasicBlock *NewBasicBlock = BasicBlock::Create(InsertBefore->getContext(), "", InsertBefore->getParent(), InsertBefore);

          LoopBodyBasicBlockCopy.push_back(NewBasicBlock);
          BlocksMapping[LoopBodyBasicBlocks[j]] = NewBasicBlock;
      }

      for (size_t j = 0; j < LoopBodyBasicBlocks.size(); j++) {
        Builder.SetInsertPoint(LoopBodyBasicBlockCopy[j]);

        for (Instruction &I : *LoopBodyBasicBlocks[j]) {
          Copy = I.clone();
          Builder.Insert(Copy);

          if (isa<LoadInst>(Copy) && Copy->getOperand(0) == LoopCounter) {
            Instruction *Add = (Instruction *) BinaryOperator::CreateAdd(Copy,
                               ConstantInt::get(Type::getInt32Ty(Copy->getContext()), i));
            Add->insertAfter(Copy);
            LoadMapping[Copy] = Add;
          }

          Mapping[&I] = Copy;
          for (size_t k = 0; k < Copy->getNumOperands(); k++) {
            if (Mapping.find(Copy->getOperand(k)) != Mapping.end()) {
              Copy->setOperand(k, Mapping[Copy->getOperand(k)]);
            }
            if (LoadMapping.find(Copy->getOperand(k)) != LoadMapping.end()) {
              Copy->setOperand(k, LoadMapping[Copy->getOperand(k)]);
            }
          }
        }
      }

      for (size_t j = 0; j < LoopBodyBasicBlocks.size(); j++) {
        for (size_t k = 0; k < LoopBodyBasicBlockCopy[j]->getTerminator()->getNumSuccessors(); k++) {
          if (BlocksMapping.find(LoopBodyBasicBlocks[j]->getTerminator()->getSuccessor(k)) != BlocksMapping.end()) {
            LoopBodyBasicBlockCopy[j]->getTerminator()->setSuccessor(k,
            BlocksMapping[LoopBodyBasicBlocks[j]->getTerminator()->getSuccessor(k)]);
          }
        }
      }

      LastFromPreviousCopy->getTerminator()->setSuccessor(0, LoopBodyBasicBlockCopy.front());
      LastFromPreviousCopy = LoopBodyBasicBlockCopy.back();
    }

    LastFromPreviousCopy->getTerminator()->setSuccessor(0, InsertBefore);
  }

  BasicBlock *copyLoop(Loop *L)
  {
    BasicBlock *Exit = L->getExitBlock();

    std::unordered_map<Value *, Value *> Mapping;
    std::unordered_map<BasicBlock *, BasicBlock *> BlocksMapping;
    IRBuilder<> Builder(Exit->getContext());
    Instruction *Copy;
    std::vector<BasicBlock *> LoopBasicBlockCopy;

    for (size_t j = 0; j < LoopBasicBlocks.size(); j++) {
      BasicBlock *NewBasicBlock = BasicBlock::Create(Exit->getContext(), "", Exit->getParent(), Exit);
      LoopBasicBlockCopy.push_back(NewBasicBlock);
      BlocksMapping[LoopBasicBlocks[j]] = NewBasicBlock;
    }

    for (size_t j = 0; j < LoopBasicBlocks.size(); j++) {
      Builder.SetInsertPoint(LoopBasicBlockCopy[j]);

      for (Instruction &I : *LoopBasicBlocks[j]) {
        Copy = I.clone();
        Builder.Insert(Copy);
        Mapping[&I] = Copy;

        for (size_t k = 0; k < Copy->getNumOperands(); k++) {
          if (Mapping.find(Copy->getOperand(k)) != Mapping.end()) {
            Copy->setOperand(k, Mapping[Copy->getOperand(k)]);
          }
        }
      }
    }

    for (size_t j = 0; j < LoopBasicBlocks.size(); j++) {
      for (size_t k = 0; k < LoopBasicBlocks[j]->getTerminator()->getNumSuccessors(); k++) {
        if (BlocksMapping.find(LoopBasicBlocks[j]->getTerminator()->getSuccessor(k)) != BlocksMapping.end()) {
          LoopBasicBlockCopy[j]->getTerminator()->setSuccessor(k,
          BlocksMapping[LoopBasicBlocks[j]->getTerminator()->getSuccessor(k)]);
        }
      }
    }

    return LoopBasicBlockCopy.front();
  }

  void fullUnrolling(Loop *L)
  {
    std::vector<BasicBlock *> LoopBodyBasicBlocks(LoopBasicBlocks.size() - 2);
    std::copy(LoopBasicBlocks.begin() + 1, LoopBasicBlocks.end() - 1, LoopBodyBasicBlocks.begin());

    L->getLoopPreheader()->getTerminator()->setSuccessor(0, LoopBodyBasicBlocks.front());
    LoopBasicBlocks[LoopBasicBlocks.size() - 2]->getTerminator()->setSuccessor(0, L->getExitBlock());
    LoopBasicBlocks.front()->eraseFromParent();
    LoopBasicBlocks.back()->eraseFromParent();

    duplicateLoopBody(LoopBodyBasicBlocks, BoundValue - 1, L->getExitBlock());
  }

  void partialUnrolling(Loop *L)
  {
    int Factor = 3;

    BasicBlock *JumpTo = copyLoop(L);

    std::vector<BasicBlock *> LoopBodyBasicBlocks(LoopBasicBlocks.size() - 2);
    std::copy(LoopBasicBlocks.begin() + 1, LoopBasicBlocks.end() - 1, LoopBodyBasicBlocks.begin());

    duplicateLoopBody(LoopBodyBasicBlocks, Factor - 1, LoopBasicBlocks.back());
    LoopBasicBlocks.front()->getTerminator()->setSuccessor(1, JumpTo);

    for (Instruction &I : *LoopBasicBlocks.front()) {
      if (isa<LoadInst>(&I) && I.getOperand(0) == LoopCounter) {
        Instruction *Add = (Instruction *) BinaryOperator::CreateAdd(&I,
        ConstantInt::get(Type::getInt32Ty(I.getContext()), Factor - 1));
        Add->insertAfter(&I);
        I.replaceAllUsesWith(Add);
      }
    }

    for (Instruction &I : *LoopBasicBlocks.back()) {
      if (isa<AddOperator>(&I) && I.getOperand(0) == LoopCounter) {
        I.setOperand(1, ConstantInt::get(Type::getInt32Ty(I.getContext()), Factor));
      }
    }
  }

  void unrollLoop(Loop *L)
  {
    if (isLoopBoundConst) {
      fullUnrolling(L);
    }
    else {
      partialUnrolling(L);
    }
  }

  bool runOnLoop(Loop *L, LPPassManager &LPM) override {
    LoopBasicBlocks = L->getBlocksVector();
    MapVariables(L);
    findLoopCounterAndBound(L);
    unrollLoop(L);

    return true;
  }
};
}

char LoopUnrollingPass::ID = 0;
static RegisterPass<LoopUnrollingPass> X("loop-unrolling", "",
                                       false,
                                       false);
