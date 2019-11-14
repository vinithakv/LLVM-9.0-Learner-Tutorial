#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Pass.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "HI_print.h"
#include "HI_MulOrderOpt.h"
#include "llvm/Transforms/Utils/Local.h"
#include <stdio.h>
#include <string>
#include <ios>
#include <stdlib.h>

using namespace llvm;
 
bool HI_MulOrderOpt::runOnFunction(llvm::Function &F) // The runOnModule declaration will overide the virtual one in ModulePass, which will be executed for each Module.
{
    print_status("Running HI_MulOrderOpt pass."); 
    std::set<long long> power2;
    generatedI.clear();
    op2Cnt.clear();
    
    for (int i=0, pow=1;i<64;i++,pow*=2)
        power2.insert(pow);

    if (F.getName().find("llvm.") != std::string::npos)
        return false;
        

    //AAAAABB
    //AAAAA * BB
    //AA * AA * A   B *  B   
    //  A * A    
    bool changed = 0;
    for (BasicBlock &B : F) 
    {
        bool action = 1;
        while (action)
        {
            action = 0;
            for (BasicBlock::reverse_iterator it=B.rbegin(), ie=B.rend(); it!=ie; it++) 
            {
                Instruction* I=&*it;

                if (I->getOpcode() != Instruction::Mul)
                    continue;
                if (generatedI.find(I) != generatedI.end()) // this Instruction in generated by us, bypass it
                    continue;
                MulOperator *curMulI =  dyn_cast<MulOperator>(I);
                MulOperator *LMul = nullptr, *RMul = nullptr;
                Value *VarForMul;
                
                LMul = dyn_cast<MulOperator>(I->getOperand(0));
                RMul = dyn_cast<MulOperator>(I->getOperand(1));
                if (!LMul && !RMul) 
                    continue;

                while (heap_opCnt.size()>0)
                    heap_opCnt.pop();
                op2Cnt.clear();
                recursiveGetMulOpAndCounter(I);

                int tot_cnt = 0;
                for (auto total_val_cnt_pair : op2Cnt[curMulI])
                {
                    heap_opCnt.push(std::pair<int, Value*>(total_val_cnt_pair.second,total_val_cnt_pair.first));
                    tot_cnt += total_val_cnt_pair.second;
                }

                
                changed = 1;
                action = 1;  

                *MulOrderOptLog << "\n\nbefore replacement:\n\n" << B;
                IRBuilder<> Builder(I);

                int replcaceCnt = 0;

                Value *newMul = recursiveMul(heap_opCnt, tot_cnt, Builder);
                curMulI->replaceAllUsesWith(newMul);
                RecursivelyDeleteTriviallyDeadInstructions(curMulI);

                *MulOrderOptLog << "\n\nafter replacement#" << replcaceCnt <<":\n\n" << B;
                // while (1)
                // {
                //     replcaceCnt++;
                //     int mulcnt = (heap_opCnt.top()).first;
                //     if (mulcnt>1)
                //     {
                //         *MulOrderOptLog << "\n\n========================\n  handling " << *heap_opCnt.top().second << "^(" << mulcnt << ")\n";
                //         MulOrderOptLog->flush();
                //         Value *newMul = 
                //         heap_opCnt.pop();
                //         heap_opCnt.push(std::pair<int, Value*>(1, newMul));
                //         *MulOrderOptLog << "\n\nafter replacement#" << replcaceCnt <<":\n\n" << B;
                //     }
                //     else
                //     {
                //         if (heap_opCnt.size()==1)
                //         {
                //             *MulOrderOptLog << "\n\n========================\n  do the re-multiplication at " << *heap_opCnt.top().second << "\n";
                //             MulOrderOptLog->flush();
                //             curMulI->replaceAllUsesWith(heap_opCnt.top().second);
                //             RecursivelyDeleteTriviallyDeadInstructions(curMulI);
                //             *MulOrderOptLog << "\n\nafter replacement#" << replcaceCnt <<":\n\n" << B;
                //             break;
                //         }
                //         else
                //         {
                //             Value *val0 = heap_opCnt.top().second;
                //             heap_opCnt.pop();
                //             Value *val1 = heap_opCnt.top().second;
                //             heap_opCnt.pop();                   
                //             Value *newMul = Builder.CreateMul(val0, val1);
                //             generatedI.insert(newMul);
                //             *MulOrderOptLog << "\n\n========================\n  do the re-multiplication between " << *val0 << " and " << *val1 << " by " << *newMul << "\n";
                //             heap_opCnt.push(std::pair<int, Value*>(1, newMul));
                //             *MulOrderOptLog << "\n\nafter replacement#" << replcaceCnt <<":\n\n" << B;
                //         }               
                //     }
                // }

                break;

                if (action)
                    break;
                    
            }
        }

    }
    MulOrderOptLog->flush(); 
    return changed;
}



char HI_MulOrderOpt::ID = 0;  // the ID for pass should be initialized but the value does not matter, since LLVM uses the address of this variable as label instead of its value.

void HI_MulOrderOpt::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.setPreservesCFG();
}

void HI_MulOrderOpt::recursiveGetMulOpAndCounter(Value *MulI)
{
    if (auto real_MulI = dyn_cast<MulOperator>(MulI))
    {
        if (op2Cnt.find(real_MulI) != op2Cnt.end())
            return;
        std::map<Value*, int> tmp_val_cnt_map;
        for (int i=0; i<real_MulI->getNumOperands(); i++)
        {
            MulOperator* opMul = dyn_cast<MulOperator>(real_MulI->getOperand(i));
            if (opMul)
            {
                recursiveGetMulOpAndCounter(opMul);                
                for (auto val_cnt_pair : op2Cnt[opMul])
                {
                    if (tmp_val_cnt_map.find(val_cnt_pair.first) == tmp_val_cnt_map.end())
                    {
                        tmp_val_cnt_map[val_cnt_pair.first] = val_cnt_pair.second;
                    }
                    else
                    {
                        tmp_val_cnt_map[val_cnt_pair.first] += val_cnt_pair.second;
                    }                    
                }
                
            }
            else
            {
                if (tmp_val_cnt_map.find(real_MulI->getOperand(i)) == tmp_val_cnt_map.end())
                {
                    tmp_val_cnt_map[real_MulI->getOperand(i)] = 1;
                }
                else
                {
                    tmp_val_cnt_map[real_MulI->getOperand(i)] += 1;
                } 
            }
        }

        op2Cnt[real_MulI] = tmp_val_cnt_map;
        *MulOrderOptLog << "\n=========================\nop info for MulI: " << *MulI << "\n";
        for (auto val_cnt_pair : tmp_val_cnt_map )
        {
            *MulOrderOptLog << "     -->" << *val_cnt_pair.first << "  --> " << val_cnt_pair.second << "\n";
        }

    }

    MulOrderOptLog->flush();

}

Value* HI_MulOrderOpt::recursiveMul(std::priority_queue<std::pair<int, Value*>, std::vector<std::pair<int, Value*>>, HI_MulOrderOpt::cmp_mulorder> cur_heap, int tot_cnt, IRBuilder<> &Builder)
{
    std::priority_queue<std::pair<int, Value*>, std::vector<std::pair<int, Value*>>, cmp_mulorder> cur_heapL, cur_heapR;
    int cntL = (tot_cnt+1) / 2;
    int cntR = tot_cnt - cntL;

    int const_cntL = cntL;
    int const_cntR = cntR;

    if (tot_cnt==1)
        return cur_heap.top().second;

    while (cur_heap.size())
    {
        std::pair<int, Value*> curMuls = cur_heap.top();

        
        cur_heap.pop();

        *MulOrderOptLog << " spliting " << curMuls.first << " x [" << *curMuls.second << "]\n";

        if (curMuls.first <= cntL)
        {
            cur_heapL.push(curMuls);
            cntL -= curMuls.first;
            *MulOrderOptLog << "       assign " << curMuls.first << " in left heap (newcap=" << cntL << ")\n";
        }
        else
        {
            if (cntL)
            {
                cur_heapL.push(std::pair<int, Value*>(cntL, curMuls.second));
                *MulOrderOptLog << "       assign " << cntL << " in left heap (newcap=" << 0 << ")\n";
                curMuls.first -= cntL;
                cntL = 0;
            }
            
            cur_heapR.push(std::pair<int, Value*>(curMuls.first, curMuls.second));
            cntR -= (curMuls.first);
            *MulOrderOptLog << "       assign " << curMuls.first<< " in right heap (newcap=" << cntR << ")\n";
        }
        assert(cntL >= 0);
        assert(cntR >= 0);
    }

    *MulOrderOptLog << "\n\n\n\n\n";
    Value *subMul0 = recursiveMul(cur_heapL, const_cntL, Builder);
    Value *subMul1 = recursiveMul(cur_heapR, const_cntR, Builder);
    Value *newMul = Builder.CreateMul(subMul0, subMul1);
    generatedI.insert(newMul);
    return newMul;
}