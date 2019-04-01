#ifndef _HI_NoDirectiveTimingResourceEvaluation
#define _HI_NoDirectiveTimingResourceEvaluation
// related headers should be included.
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Pass.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "HI_print.h"
#include <stdio.h>
#include <string>
#include <ios>
#include <stdlib.h>
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IRReader/IRReader.h"
#include <bits/stl_map.h>
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/PassAnalysisSupport.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/LoopAccessAnalysis.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/LoopVersioning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/ADT/SmallVector.h"
#include "polly/PolyhedralInfo.h"
#include "polly/DependenceInfo.h"
#include "polly/LinkAllPasses.h"
#include "polly/Options.h"
#include "polly/ScopInfo.h"
#include "HI_InstructionFiles.h"
#include <set>
#include  <iostream>
#include  <fstream>
#include <ios>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <cctype>
#include <algorithm>
#include <sstream>
#include "ClockInfo.h"
using namespace llvm;


// Pass for simple evluation of the latency of the top function, without considering HLS directives
class HI_NoDirectiveTimingResourceEvaluation : public ModulePass {
public:

    // Pass for simple evluation of the latency of the top function, without considering HLS directives
    HI_NoDirectiveTimingResourceEvaluation(const char* config_file_name, const char* evaluating_log_name, const char* top_function) : ModulePass(ID) 
    {
        BlockEvaluated.clear();
        LoopEvaluated.clear();
        FunctionEvaluated.clear();
        Loop_id.clear();
        Loop_Counter = 0;
        config_file = new std::ifstream(config_file_name);
        Evaluating_log = new raw_fd_ostream(evaluating_log_name, ErrInfo, sys::fs::F_None);
        top_function_name = std::string(top_function);
        Parse_Config();
        Load_Instruction_Info();
    } // define a pass, which can be inherited from ModulePass, LoopPass, FunctionPass and etc.
    ~HI_NoDirectiveTimingResourceEvaluation()
    {
        for (auto ele : Loop2Blocks) 
        {
            delete ele.second;
        }
        for (auto ele : Block2Loops) 
        {
            delete ele.second;
        }
        Evaluating_log->flush(); delete Evaluating_log;
    }

    virtual bool doInitialization(Module &M)
    {
        
        print_status("Initilizing HI_NoDirectiveTimingResourceEvaluation pass.");
        Loop_id.clear();
        LoopLatency.clear();
        BlockLatency.clear();
        FunctionLatency.clear();
        BlockEvaluated.clear();
        Func_BlockEvaluated.clear();
        LoopEvaluated.clear();
        FunctionEvaluated.clear();
        InstructionEvaluated.clear();
        BlockVisited.clear();
        Func_BlockVisited.clear();

        Function2OuterLoops.clear();
        Block2EvaluatedLoop.clear();
        BlockCriticalPath_inLoop.clear();
        tmp_BlockCriticalPath_inFunc.clear();
        tmp_LoopCriticalPath_inFunc.clear();
        tmp_BlockCriticalPath_inLoop.clear();
        tmp_SubLoop_CriticalPath.clear();
        InstructionCriticalPath_inBlock.clear();

        return false;
        
    }

    class timingBase;

    // set the dependence of Passes
    void getAnalysisUsage(AnalysisUsage &AU) const;

    // parse the file to get configuration
    void Parse_Config();

    // load the timing and resource information for the instructions
    void Load_Instruction_Info();

    virtual bool runOnModule(Module &M); 

    // check whether the block is in some loops
    bool isInLoop(BasicBlock *BB); 

    // evaluatate the latency of a outer loop, which could be a nested one
    timingBase getOuterLoopLatency(Loop *outerL); 

    // get the most outer loop which contains the block, treat the loop as a node for the evaluation of latency
    Loop* getOuterLoopOfBlock(BasicBlock* B);

    // find the inner unevaluated loop for processing
    Loop* getInnerUnevaluatedLoop(Loop* outerL);

    // evaluate a loop in which all the children loops have been evauluated
    timingBase getLoopLatency_InnerChecked(Loop *L); 
    
    static char ID;

    int Loop_Counter;

    std::map<Loop*, int> Loop_id;

    // the latency of each loop
    std::map<Loop*, timingBase> LoopLatency;

    // the latency of each block
    std::map<BasicBlock*, timingBase> BlockLatency;

    // the latency of each function
    std::map<Function*, timingBase> FunctionLatency;

    // record whether the component is evaluated
    std::set<BasicBlock*> BlockEvaluated;
    std::set<BasicBlock*> Func_BlockEvaluated;
    std::set<Loop*> LoopEvaluated;
    std::set<Function*> FunctionEvaluated;
    std::set<Instruction*> InstructionEvaluated;
    std::set<BasicBlock*> BlockVisited;
    std::set<BasicBlock*> Func_BlockVisited;

    // record the information of the processing
    raw_ostream *Evaluating_log;
    std::error_code ErrInfo;
    std::ifstream *config_file;

    // the pass requires a specified top_function name
    std::string top_function_name;

    // record the relations between loops and blocks
    std::map<Loop*, std::vector<BasicBlock*>*> Loop2Blocks;
    std::map<BasicBlock*, std::vector<Loop*>*> Block2Loops;

    // record the list of outer loops for functions
    std::map<Function*, std::vector<Loop*> > Function2OuterLoops;

    // record which evaluated loop the block is belong to, so the pass can directly trace to the loop for the latency
    std::map<BasicBlock*, Loop*> Block2EvaluatedLoop;

    // record the critical path from the loop header to the end of the specific block
    std::map<Loop*, std::map<BasicBlock*, timingBase> > BlockCriticalPath_inLoop;

    // record the critical path to the end of block in the function
    std::map<BasicBlock*, timingBase> tmp_BlockCriticalPath_inFunc;

    // record the critical path to the end of loops in the function
    std::map<Loop*, timingBase> tmp_LoopCriticalPath_inFunc;

    // record the critical path to the end of sub-loops in the loop
    std::map<BasicBlock*, timingBase> tmp_BlockCriticalPath_inLoop;

    // record the critical path from the outter loop header to the end of the specific sub-loop
    std::map<Loop*, timingBase> tmp_SubLoop_CriticalPath;

    // record the critical path from the block entry to the end of the specific instruction
    std::map<BasicBlock*, std::map<Instruction*, timingBase> > InstructionCriticalPath_inBlock;

    // get the function latency
    timingBase getFunctionLatency(Function* F);

    // get the function critical path by traversing the blocks based on DFS
    void getFunctionLatency_traverseFromEntryToExiting(timingBase tmp_critical_path, Function *F, BasicBlock* curBlock);

    // get the loop latency by traversing from the header to the exiting blocks
    void LoopLatencyEvaluation_traversFromHeaderToExitingBlocks(timingBase tmp_critical_path,  Loop* L, BasicBlock *curBlock);

    // mark the block in loop with latency by traversing from the header to the exiting blocks
    void MarkBlock_traversFromHeaderToExitingBlocks(timingBase total_latency, Loop* L, BasicBlock *curBlock);

    // evaluate the block latency by traversing the instructions
    timingBase BlockLatencyEvaluation(BasicBlock *B);

    // check whether the instruction is in the block
    bool BlockContain(BasicBlock *B, Instruction *I);

    // get the latency of a specific instruction
    timingBase getInstructionLatency(Instruction *I);

    // check whether all the sub-function are evaluated
    bool CheckDependencyFesilility(Function &F);

    // get the relationship between loops and blocks
    void getLoopBlockMap(Function* F);

    // some LLVM analysises could be involved
    ScalarEvolution *SE;
    LoopInfo *LI;
    LoopAccessLegacyAnalysis *LAA;

    bool topFunctionFound = 0;
    
    float clock_period = 10.0;

    int top_function_latency;

    std::string clock_period_str = "10.0";

    std::string HLS_lib_path = "";

    class inst_timing_resource_info
    {
        public:
        // resource 
        int FF;
        int DSP;
        int LUT;

        // timing 
        int Lat;
        float delay;
        int II;

        // core
        std::string core_name;

        inst_timing_resource_info &operator=(inst_timing_resource_info input)
        {
            FF=input.FF;
            DSP=input.DSP;
            LUT=input.LUT;
            Lat=input.Lat;
            delay=input.delay;
            II=input.II;
            core_name=input.core_name;
        }

        inst_timing_resource_info(const inst_timing_resource_info &input)
        {
            FF=input.FF;
            DSP=input.DSP; 
            LUT=input.LUT;
            Lat=input.Lat;
            delay=input.delay;
            II=input.II;
            core_name=input.core_name;
        }

        inst_timing_resource_info()
        {
            FF=0;
            DSP=0; 
            LUT=0;
            Lat=0;
            delay=0;
            II=0;
            core_name="";
        }

        void print()
        {
            llvm::errs() << " DSP=" << DSP << " FF=" << FF << " LUT=" << LUT << " Lat=" << Lat << " delay=" << delay << " II=" << II << "\n";  
        }

    };

    class timingBase
    {
        public:
            timingBase( int l,double t, int i, double p)
            {
                latency = l;
                timing = t;
                clock_period = p;
                II = i;
            }
            int latency;
            int II;
            double timing;    
            double clock_period;

            timingBase &operator=(timingBase input)
            {
                latency=input.latency;
                II=input.II;
                timing=input.timing;
                clock_period=input.clock_period;
            }

            timingBase(const timingBase& input)
            {
                latency=input.latency;
                II=input.II;
                timing=input.timing;
                clock_period=input.clock_period;
            }
            timingBase()
            {
                latency=0;
                II=0;
                timing=0;
                clock_period=0;
            }
    };

    friend timingBase operator+(timingBase lhs, timingBase rhs)
    {
        assert(lhs.clock_period == rhs.clock_period);
        if (rhs.latency<0)
        {
            lhs.timing = rhs.timing;
            lhs.latency++;
            return lhs;
        }
        lhs.latency = lhs.latency + rhs.latency;
        lhs.timing = lhs.timing + rhs.timing;
        if (lhs.timing > lhs.clock_period*7/8)
        {
            lhs.timing = rhs.timing;
            lhs.latency++;
        }
        return lhs;
    }

    friend bool operator>(timingBase lhs, timingBase rhs)
    {
        assert(lhs.clock_period == rhs.clock_period);
        return (((lhs.latency>rhs.latency))||(lhs.latency == rhs.latency && lhs.timing > rhs.timing));
    }

    friend timingBase operator*(timingBase lhs, int rhs)
    {
        if (lhs.timing>0.0001)
        {
            lhs.latency++;
            lhs.timing=0;
        }
        lhs.latency*=rhs;
        return lhs;
    }

    friend timingBase operator*(int rhs,timingBase lhs)
    {
        if (lhs.timing>0.0001)
        {
            lhs.latency++;
            lhs.timing=0;
        }
        lhs.latency*=rhs;
        return lhs;
    }

    friend raw_ostream& operator<< (raw_ostream& stream, const timingBase& tb)
    {
        stream << " [latency=" << tb.latency << ", timing="<<tb.timing<<"] ";
        return stream;
    }

    typedef std::map<int,std::map<int, std::map<std::string,inst_timing_resource_info>>> Info_type_list;

    std::map<std::string,Info_type_list> BiOp_Info_name2list_map;

    inst_timing_resource_info get_inst_info(std::string opcode, int operand_bitwid , int res_bitwidth, std::string period);

    timingBase get_inst_info_result(std::string opcode, int operand_bitwid , int res_bitwidth, std::string period);

    // int get_N_DSP(std::string opcode, int operand_bitwid , int res_bitwidth, std::string period);
    // int get_N_FF(std::string opcode, int operand_bitwid , int res_bitwidth, std::string period);
    // int get_N_LUT(std::string opcode, int operand_bitwid , int res_bitwidth, std::string period);
    // int get_N_Lat(std::string opcode, int operand_bitwid , int res_bitwidth, std::string period);
    // double get_N_Delay(std::string opcode, int operand_bitwid , int res_bitwidth, std::string period);
    bool checkInfoAvailability(std::string opcode, int operand_bitwid , int res_bitwidth, std::string period);
    bool checkFreqProblem(std::string opcode, int operand_bitwid , int res_bitwidth, std::string period);
    inst_timing_resource_info checkInfo_HigherFreq(std::string opcode, int operand_bitwid , int res_bitwidth, std::string period);
};

#endif