#ifndef TCD_H
#define TCD_H

#include "llvm/IR/Value.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/PassSupport.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Operator.h"

#include "MemoryModel/PointerAnalysis.h"
#include "MemoryModel/PAG.h"

#include <map>
#include <vector>
#include <set>

#define TCD_DEBUG   "tcd"

#define STUB_FUNC_PREFIX   "__au_edu_unsw_"
#define STUB_FUNC_POSTFIX   "_cast_stub"
#define REINTERPRET_CAST_STUB   "__au_edu_unsw_reinterpret_cast_stub"
#define STATIC_CAST_STUB   "__au_edu_unsw_static_cast_stub"
#define DYNAMIC_CAST_STUB   "__au_edu_unsw_dynamic_cast_stub"
#define PLACEMENT_NEW_STUB  "__au_edu_unsw_placement_new_stub"
#define NEW_STUB            "__au_edu_unsw_new_stub"

#define SRC_TYPE_INFO_METADATA   "SrcTypeInfo"
#define DST_TYPE_INFO_METADATA   "DstTypeInfo"

class TypeConfusionEntry{
public:
  const llvm::Instruction * inst;
  const llvm::Value * src;
  const llvm::Value * dst;

  std::set<const PAGNode *> actual_pointees;
  TypeConfusionEntry(const llvm::Instruction * i, const llvm::Value * s, const llvm::Value * d){
    inst = i;
    src = s;
    dst = d;
  }
  void AddPointee(const PAGNode * pointee){
    actual_pointees.insert(pointee);
  }
  unsigned long GetNumOfPointees(){
    return actual_pointees.size();
  }
};

// Statistics information about C++ casts.
// FIXME: memory leakage :)
class CastStatistics{
public:
  int StaticCastCnt;
  int DynamicCastCnt;
  int ReinterpretCastCnt;
  int PlacementNewCnt;
  int NewCnt;
  int TypeErrorCnt;
  int StaticMisuseCnt;
  int DynamicMisuseCnt;
  int ReinterpretMisuseCnt;


  typedef std::map<unsigned, unsigned> HistogramType;
  typedef std::map<const llvm::Value *, unsigned> ValPointscntMapType;
  typedef std::set<const llvm::Instruction *> InstructionSet;

  typedef std::vector<TypeConfusionEntry> TypeConfusionInfoType;

  HistogramType Histogram;
  ValPointscntMapType ValPointscntMap;

  //
  TypeConfusionInfoType TypeConfusionInfo;

  InstructionSet DefinitelySafeCasts;
  InstructionSet DefinitelyUnsafeCasts;
  InstructionSet UncertainCasts;
  InstructionSet AllCasts;

  void Print(llvm::raw_ostream &out);
  CastStatistics();
};

// Collect information of cast stub function.
class CastStubInfoCollector : public llvm::ModulePass {

public:
  CastStubInfoCollector(PointerAnalysis * p = 0) : llvm::ModulePass(ID),pta(p) {
    symTable = SymbolTableInfo::Symbolnfo();
  }
  virtual bool runOnModule(llvm::Module &M) ;
  static char ID;
  enum CastKind{
    STATIC_CAST_KIND,
    DYNAMIC_CAST_KIND,
    REINTERPRET_CAST_KIND
  };
  CastStatistics CastStat;
  void DetectTypeConfustion();
  void DetectMisuseOfCast(CastKind kind, const llvm::Value *dst, const llvm::Instruction * inst);
  bool IsContained(llvm::StructType * dstType,llvm::StructType * actualType);
  bool IsPhantom(llvm::StructType * base,llvm::StructType * derived);
  void PrintStatistics();

  void SetPTA(PointerAnalysis * p);

private:
  PointerAnalysis * pta;
  SymbolTableInfo* symTable;

  //
  typedef bool (CastStubInfoCollector::* IsXXXFuncType)(
      llvm::StructType * actualType,llvm::StructType * dstType,const PAGNode * actual);

  void PrintHistogram();
  void FindPossibleTypeConfusions();
  void TypeConfusionReport();
  void PrintTargetCasts(std::string title, std::set<const llvm::Instruction *> &casts);
  llvm::StructType * GetAbstractObjectType(const PAGNode * node, llvm::Instruction * inst);
  bool IsBadType(llvm::StructType * actualType,llvm::StructType * dstType,const PAGNode * actual);
  bool IsRightType(llvm::StructType * actualType,llvm::StructType * dstType,const PAGNode * actual);
  bool DoCastTest(       llvm::Instruction * inst,
                         llvm::Value * src,
                         llvm::Value * dst,
                         const PAGNode  * actual,
                         llvm::StructType * actualType,
                         IsXXXFuncType TestType);
  bool IsBadCast(llvm::Instruction * inst, llvm::Value * src, llvm::Value * dst, const PAGNode * actual);
  bool IsRightCast(llvm::Instruction * inst, llvm::Value * src, llvm::Value * dst, const PAGNode * actual);
  bool IsCxxVTable(const llvm::Value * val);  
  void AddDDAQuery(llvm::Value * s);  
};


bool IsConservativeTCD();
bool isNewExpressionStub(llvm::StringRef name);
bool IsDetectTC();
llvm::PointerType * getPointerTypeViaMetadata(llvm::Instruction * inst, const char * metaDataName);


#endif
