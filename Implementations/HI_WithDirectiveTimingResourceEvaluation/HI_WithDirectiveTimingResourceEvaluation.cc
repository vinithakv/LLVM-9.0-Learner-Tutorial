#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Pass.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "HI_print.h"
#include "HI_WithDirectiveTimingResourceEvaluation.h"
#include "polly/PolyhedralInfo.h"

#include <stdio.h>
#include <string>
#include <ios>
#include <stdlib.h>
#include <sstream>
using namespace llvm;


bool HI_WithDirectiveTimingResourceEvaluation::runOnModule(Module &M) // The runOnFunction declaration will overide the virtual one in ModulePass, which will be executed for each Function.
{    
    *Evaluating_log << " ======================= the module begin =======================\n";
    *Evaluating_log << M;
    *Evaluating_log << " ======================= the module end =======================\n";

    TraceMemoryDeclarationinModule(M);

    AnalyzeFunctions(M);

    analyzeTopFunction(M);

    return false;
}

bool HI_WithDirectiveTimingResourceEvaluation::CheckDependencyFesilility(Function &F)
{
    for (auto &B : F)
        for (auto &I : B)
            if (CallInst *CI = dyn_cast<CallInst>(&I))
            {
                if (FunctionLatency.find(CI->getCalledFunction()) == FunctionLatency.end())
                {
                    if (CI->getCalledFunction()->getName().find("llvm.")!=std::string::npos)
                    {
                        timingBase tmp(0,0,1,clock_period);
                        FunctionLatency[CI->getCalledFunction()] = tmp;                    
                    }
                    else
                        return false;
                }
            }
    return true;
}

char HI_WithDirectiveTimingResourceEvaluation::ID = 0;  // the ID for pass should be initialized but the value does not matter, since LLVM uses the address of this variable as label instead of its value.

// introduce the dependence of Pass
void HI_WithDirectiveTimingResourceEvaluation::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequiredTransitive<ScalarEvolutionWrapperPass>();
    
    // AU.addRequired<ScalarEvolutionWrapperPass>();
    // AU.addRequired<LoopInfoWrapperPass>();
    // AU.addPreserved<LoopInfoWrapperPass>();
    AU.addRequired<LoopAccessLegacyAnalysis>();
    AU.addRequired<DominatorTreeWrapperPass>();
    // AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addRequired<OptimizationRemarkEmitterWrapperPass>();
    // AU.addRequiredTransitive<polly::DependenceInfoWrapperPass>();
    // AU.addRequired<LoopInfoWrapperPass>();
    // AU.addRequiredTransitive<polly::ScopInfoWrapperPass>();
    // AU.addRequired<polly::PolyhedralInfo>();
    // AU.addPreserved<GlobalsAAWrapperPass>();
    
}




void HI_WithDirectiveTimingResourceEvaluation::AnalyzeFunctions(Module &M)
{
    bool all_processed = 0;
    while (!all_processed)
    {
        all_processed = 1;
        for (auto &F : M)
        {   
            *Evaluating_log << "CHECKING FUNCTION "<< F.getName() <<"\n";
            Evaluating_log->flush();
            if (FunctionLatency.find(&F) != FunctionLatency.end())
            {
                continue;
            }
            else
            {                
                if (F.getName().find("llvm.")!=std::string::npos)
                {
                    timingBase tmp(0,0,1,clock_period);
                    FunctionLatency[&F] = tmp;       
                    continue;             
                }
                all_processed = 0;
                if (CheckDependencyFesilility(F))
                {
                    LAA = &getAnalysis<LoopAccessLegacyAnalysis>(F);
                    LI = &getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
                    SE = &getAnalysis<ScalarEvolutionWrapperPass>(F).getSE();  
                    ArrayAccessCheckForFunction(&F);
                    getLoopBlockMap(&F);
                    analyzeFunction(&F);
                }
            }
            
        }
    }
}

void HI_WithDirectiveTimingResourceEvaluation::analyzeTopFunction(Module &M)
{
    int state_total_num = getTotalStateNum(M);
    int LUT_needed_by_FSM = LUT_for_FSM(state_total_num);
    int FF_needed_by_FSM = state_total_num;
    for (auto &F : M)
    {
        std::string mangled_name = F.getName();
        std::string demangled_name;
        demangled_name = demangeFunctionName(mangled_name);
        mangled_name = "find function " + mangled_name + "and its demangled name is : " + demangled_name;
        print_info(mangled_name.c_str());
        if (demangled_name == top_function_name)
        {
            *Evaluating_log << "Top Function: "<< F.getName() <<" is found";
            topFunctionFound = 1;
            top_function_latency = analyzeFunction(&F).latency;
            
            std::string printOut("");
            FunctionResource[&F] = FunctionResource[&F] + BRAM_MUX_Evaluate();
            // print out the information of top function in terminal
            printOut = "Done latency evaluation of top function: [" + demangled_name + "] and its latency is " + std::to_string(top_function_latency) 
                                                                                    + " the state num is: " + std::to_string(state_total_num) 
                                                                                    + " and its resource cost is [DSP=" + std::to_string(FunctionResource[&F].DSP) 
                                                                                    + ", FF=" + std::to_string(FunctionResource[&F].FF+FF_needed_by_FSM) 
                                                                                    + ", LUT=" + std::to_string(FunctionResource[&F].LUT +  LUT_needed_by_FSM) 
                                                                                    + ", BRAM=" + std::to_string(FunctionResource[&F].BRAM ) 
                                                                                    + "]";
            *Evaluating_log << printOut << "\n";
            print_info(printOut);
        }
    }
}

void HI_WithDirectiveTimingResourceEvaluation::TraceMemoryDeclarationinModule(Module &M)
{
    for (auto &F : M)
    {
        
        if (F.getName().find("llvm.")!=std::string::npos) // bypass the "llvm.xxx" functions..
            continue;
        std::string mangled_name = F.getName();
        std::string demangled_name;
        demangled_name = demangeFunctionName(mangled_name);
        findMemoryDeclarationin(&F, demangled_name == top_function_name); 

        TraceMemoryAccessinFunction(F);

    }
}

int HI_WithDirectiveTimingResourceEvaluation::getTotalStateNum(Module &M)
{
    int state_total = 0;
    for (auto &F : M)
    {
        if (F.getName().find("llvm.")!=std::string::npos) // bypass the "llvm.xxx" functions..
            continue;
        BasicBlock *Func_Entry = &(F.getEntryBlock()); //get the entry of the function
        timingBase origin_path_in_F(0,0,1,clock_period);
        tmp_BlockCriticalPath_inFunc.clear(); // record the block level critical path in the loop
        tmp_LoopCriticalPath_inFunc.clear(); // record the critical path to the end of sub-loops in the loop
        Func_BlockVisited.clear();
        state_total += getFunctionStageNum(origin_path_in_F, &F, Func_Entry) ; 
    }
    return state_total + 2;  // TODO: check +2 is for function or module (reset/idle)
}

int HI_WithDirectiveTimingResourceEvaluation::LUT_for_FSM(int stateNum)
{
    double x = stateNum;
    double y = -0.44444444444444475*x*x+10.555555555555559*x-14.11111111111112;
    return round(y);
}