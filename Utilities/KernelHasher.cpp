#include <map>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/CommandLine.h"
#include "json.hpp"
using namespace llvm;
using json = nlohmann::json;
using namespace std;

cl::opt<std::string> InputFilename("i", cl::desc("Specify input bitcode"), cl::value_desc("bitcode filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"));
cl::opt<std::string> KernelFilename("k", cl::desc("Specify kernel json"), cl::value_desc("kernel filename"), cl::Required);

static int UID = 0;

static int valueId = 0;

string getName()
{
    string name = "v_" + to_string(valueId++);
    return name;
}

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    LLVMContext context;
    SMDiagnostic smerror;
    std::unique_ptr<Module> mptr = parseIRFile(InputFilename, smerror, context);
    Module *M = mptr.get();

    map<int, BasicBlock *> blockMap;

    for (Module::iterator F = M->begin(), E = M->end(); F != E; ++F)
    {
        for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
        {
            BasicBlock *b = cast<BasicBlock>(BB);
            blockMap[UID++] = b;
        }
    }

    ifstream inputJson(KernelFilename);
    nlohmann::json j;
    inputJson >> j;
    inputJson.close();

    map<string, uint64_t> outputMap;
    hash<string> hasher;
    for (auto &[key, value] : j.items())
    {
        vector<int> blocks = value;
        std::sort(blocks.begin(), blocks.end());
        vector<string> blockStrings;
        for (int block : blocks)
        {
            BasicBlock *toConvert = blockMap[block];
            valueId = 0;
            string blockStr = "";
            vector<Value *> namedVals;
            for (BasicBlock::iterator BI = toConvert->begin(), BE = toConvert->end(); BI != BE; ++BI)
            {
                Instruction *inst = cast<Instruction>(BI);
                int ops = inst->getNumOperands();
                for (int i = 0; i < ops; i++)
                {
                    Value *op = inst->getOperand(i);
                    op->setName(getName());
                    namedVals.push_back(op);
                }
                if (std::find(namedVals.begin(), namedVals.end(), inst) == namedVals.end())
                {
                    inst->setName(getName());
                    namedVals.push_back(inst);
                }
                std::string str;
                llvm::raw_string_ostream rso(str);
                inst->print(rso);
                blockStr += str + "\n";
            }
            for (Value *v : namedVals)
            {
                v->setName("");
            }
            namedVals.clear();
            blockStr += "\n";
            blockStrings.push_back(blockStr);
        }

        std::sort(blockStrings.begin(), blockStrings.begin());

        string toHash = "";
        for (auto str : blockStrings)
        {
            toHash += str + "\n";
        }

        uint64_t hashed = hasher(toHash);
        outputMap[key] = hashed;
    }

    json j_map(outputMap);
    if (OutputFilename != "")
    {
        std::ofstream file;
        file.open(OutputFilename);
        file << j_map;
        file.close();
    }
    else
    {
        cout << j_map << "\n";
    }

    return 0;
}