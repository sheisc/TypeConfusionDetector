//===- SVFGBuilder.cpp -- SVFG builder----------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * TCD.cpp
 *
 *    Type Confusion Detector.
 *    NOTICE:   We assume "-Xclang -disable-O0-optnone -O0 -g" is used.
 *              -O1,-O2 and -O3 are not supported.
 *              Since the compiler might optimize(delete) the IR pattern we rely on.
 *
 *  Created on: Oct 22, 2018
 *      Author: Jeffrey Zou
 */
#include "TCD/TCD.h"
#include "llvm/Support/Debug.h"
#include "Util/BasicTypes.h"
#include "llvm/IR/BasicBlock.h"

#include "Util/AnalysisUtil.h"
#include "Util/GEPTypeBridgeIterator.h"
#include "MemoryModel/MemModel.h"
#include "MemoryModel/PAGNode.h"
#include "DDA/ContextDDA.h"
#include "DDA/FlowDDA.h"
#include "DDA/DDAClient.h"
#include "DDA/FlowDDA.h"
#include "MemoryModel/LocationSet.h"
#include <utility>
using namespace llvm;
using namespace std;

/*
    //C++ functions
    {"_Znwm", ExtAPI::EFT_ALLOC},	// new
    {"_Znam", ExtAPI::EFT_ALLOC},	// new []
    {"_Znaj", ExtAPI::EFT_ALLOC},	// new
    {"_Znwj", ExtAPI::EFT_ALLOC},	// new []
 */
#define NEW_FUNC_NAME "_Znwm"
// _ZN10cObject123nwEm
// void * cObject123::operator new(size_t m)
#define CUSTOMED_OPERATOR_NEW_PREFIX "_ZN"
#define CUSTOMED_OPERATOR_NEW_POSTFIX "nwEm"
//
//_ZN10cObject123nwEmPv
//void * cObject123::operator new(unsigned long, void*)
#define CXX_VTABLE_NAME_PREFIX "_ZTV"

#define CXX_LIBRARY_DYNAMIC_CAST_FUNC    "__dynamic_cast"
#define OFFSET_ARG_POS      3

#undef DBOUT
#define DBOUT(TYPE, X) 	do{X;} while(0)

char CastStubInfoCollector::ID = 0;
// Register the pass
static RegisterPass<CastStubInfoCollector> collector ("cast-sbubs",
        "Collect statistics about cast stubs");


static cl::opt<bool> ConservativeTCD("conservative-tcd", cl::init(false),
                              cl::desc("Conservative type confusion detection, only reporting MUST-BE type error"));
static cl::opt<bool> DetectTC("detect-tc", cl::init(false),
                              cl::desc("Detect type confusion"));
static cl::opt<bool> DetectDynamic("detect-dynamic", cl::init(false),
                              cl::desc("check dynamic_cast"));
static cl::opt<bool> DetectReinterpret("detect-reinterpret", cl::init(false),
                              cl::desc("check reinterpret_cast"));


bool IsConservativeTCD(){
  return ConservativeTCD;
}

bool IsDetectTC(){
  return DetectTC;
}


CastStatistics::CastStatistics(){
  this->DynamicCastCnt = 0;
  this->ReinterpretCastCnt = 0;
  this->StaticCastCnt = 0;
  this->PlacementNewCnt = 0;
  this->NewCnt = 0;
  this->StaticMisuseCnt = 0;
  this->DynamicMisuseCnt = 0;
  this->ReinterpretMisuseCnt = 0;
}

void CastStatistics::Print(raw_ostream &out){
  //DBOUT_WITH_POS(TCD_DEBUG,out<< "\n");
  DBOUT(TCD_DEBUG,out << "The number of static_cast<> is " << StaticCastCnt << "\n");
  DBOUT(TCD_DEBUG,out << "The number of dynamic_cast<> is " << DynamicCastCnt << "\n");
  DBOUT(TCD_DEBUG,out << "The number of reinterpret_cast<> is " << ReinterpretCastCnt << "\n");
  DBOUT(TCD_DEBUG,out << "The number of placement new  is " << PlacementNewCnt << "\n");
  DBOUT(TCD_DEBUG,out << "The number of new  is " << NewCnt << "\n");
  DBOUT(TCD_DEBUG,out << "The number of misusing static_cast<> is " << StaticMisuseCnt << "\n");
  DBOUT(TCD_DEBUG,out << "The number of misusing dynamic_cast<> is " << DynamicMisuseCnt << "\n");
  DBOUT(TCD_DEBUG,out << "The number of misusing reinterpret_cast<> is " << ReinterpretMisuseCnt << "\n");
}
////////////////////////////////////////////////////////////////////////////////////////////////////

// Get the next instruction of current instruction.
static Instruction * findNextInstruction(Instruction *CurInst) {
  BasicBlock::iterator it(CurInst);
  ++it;
  if (it == CurInst->getParent()->end()){
    return nullptr;
  }
  return &*it;
}

// Get the previous instruction of current instruction.
static Instruction * findPreviousInstruction(Instruction *CurInst) {
  BasicBlock::reverse_iterator it(CurInst);
  ++it;
  if (it == CurInst->getParent()->rend()){
    return nullptr;
  }
  return &*it;
}
// Test whether it is a call instruction with the name @targetName
static CallInst * getTargetCall(llvm::Instruction * inst,StringRef targetName){
  if(CallInst * call = dyn_cast<CallInst>(inst)){
    if (Function * func = call->getCalledFunction()) {
      StringRef funcName = func->getName();
      if(funcName.equals(targetName)){
        return call;
      }
    }
  }
  return nullptr;
}
/*
  Get the 3rd argument in call i8* @__dynamic_cast(i8* %38, i8* @_ZTI1A, i8* @_ZTI1C, i64 32)
 */
static Size_t getDynamicCastOffset(llvm::Instruction * inst){
  Size_t offset = 0;
  if(CallInst * call = getTargetCall(inst,DYNAMIC_CAST_STUB)){
    while(inst = findPreviousInstruction(inst)){
      if(call = getTargetCall(inst,CXX_LIBRARY_DYNAMIC_CAST_FUNC)){
        ConstantInt *conInt = dyn_cast<ConstantInt>(call->getArgOperand(OFFSET_ARG_POS));
        if(conInt) {
          offset = conInt->getSExtValue();
          break;
        }
      }
    }
  }
  return offset;
}

llvm::PointerType * getPointerTypeViaMetadata(llvm::Instruction * inst, const char * metaDataName){
  MDNode *mdNode = inst->getMetadata(metaDataName);
  if(mdNode){
    if(llvm::ValueAsMetadata * valMeta = dyn_cast<llvm::ValueAsMetadata>(mdNode->getOperand(0))){
      return cast<PointerType>(valMeta->getValue()->getType());
    }
  }
  assert(0 && "Metadata is lost.");
  return nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
    @param  node
          The abstract object returned by pointer analysis
    @return
          Return the allocated-type of this object (struct type) or NULL.
 */
llvm::StructType * CastStubInfoCollector::GetAbstractObjectType(const PAGNode * node,llvm::Instruction * inst){
  Value * val = (Value *)node->getValue();
  Type * pointeeType = analysisUtil::getAbstractObjectType(val);
  //
  while(ArrayType * at = dyn_cast_or_null<ArrayType>(pointeeType)){
    pointeeType = at->getArrayElementType();
  }
  //
  if(StructType * structTy = dyn_cast_or_null<StructType>(pointeeType)){
    if(const GepObjPN * gepObj = dyn_cast<const GepObjPN>(node)){
      // We can't figure out the location set.
      const LocationSet &ls = gepObj->getLocationSet();
      if(ls.getBaseType() == nullptr){
        return nullptr;
      }
      //A GepObjPN corresponds to a getelementptr instruction.
      Size_t offset = ls.getByteOffset();
      // We have added gep instruction at Clang to adjust the offset.
//      if(getTargetCall(inst,DYNAMIC_CAST_STUB)){
//        offset -= getDynamicCastOffset(inst);
//      }
      //llvm::DataLayout * dataLayout = symTable->getDataLayout(pointeeType);
      //uint64_t sizeInBytes = dataLayout->getTypeAllocSize(pointeeType);
      Size_t sizeInBytes = symTable->getTypeSizeInBytes(structTy);
      if(offset >= 0){
        offset %= sizeInBytes;
      }
      StructType *targetTy = symTable->getSubStructByBytes(structTy,offset);
      if(targetTy == nullptr && ls.getMinusOffset() < 0){
        /*
          It is caused by down-cast error.
          To report it, we consider the minus offset to get its type.
         */
        offset = ls.getByteOffset() - ls.getMinusOffset();
        if(offset >= 0){
          offset %= sizeInBytes;
          targetTy = symTable->getSubStructByBytes(structTy,offset);
        }
      }
      structTy = targetTy;
    }
    return structTy;
  }else{
    return nullptr;
  }
}

void CastStubInfoCollector::AddDDAQuery(llvm::Value * s){
  if(pta){
    PAG* pag = pta->getPAG();
    //
    if(pag->hasValueNode(s)){
      if(ContextDDA * ctx = dyn_cast<ContextDDA>(pta)){
        ctx->GetDDAClient()->setQuery(pag->getValueNode(s));        
      }else if(FlowDDA * flow = dyn_cast<FlowDDA>(pta)){
        flow->GetDDAClient()->setQuery(pag->getValueNode(s));
      }
    }
  }

}

// Collect statistics about cast stub functions such as "__au_edu_unsw_reinterpret_cast_stub"
bool CastStubInfoCollector::runOnModule(Module &M) {
  //DBOUT_WITH_POS(TCD_DEBUG,llvm::outs() << "\n");
  for (auto &F : M){
    for (auto &BB : F) {
      // Visit a basic block
      for (BasicBlock::iterator II = BB.begin(), II_e = BB.end(); II != II_e;
           ++II) {
        // call void @__au_edu_unsw_dynamic_cast_stub(i64 %11, i64 %12)
        CallInst * call = dyn_cast<CallInst>(II);
        if (call && call->getCalledFunction()) {
          StringRef funcName = call->getCalledFunction()->getName();
          //
          if(funcName.equals(NEW_STUB)){
            CastStat.NewCnt++;
          }else if(funcName.equals(PLACEMENT_NEW_STUB)){
            CastStat.PlacementNewCnt++;            
          }else if(funcName.startswith(STUB_FUNC_PREFIX) && funcName.endswith(STUB_FUNC_POSTFIX)){
            Value * dstPointer = call->getArgOperand(1);
            if(funcName.equals(REINTERPRET_CAST_STUB)){
              CastStat.ReinterpretCastCnt++;              
              DetectMisuseOfCast(REINTERPRET_CAST_KIND,dstPointer,call);
              if(!DetectReinterpret){
                continue;
              }              
            }else if(funcName.equals(STATIC_CAST_STUB)){
              CastStat.StaticCastCnt++;
              DetectMisuseOfCast(STATIC_CAST_KIND,dstPointer,call);
            }else if(funcName.equals(DYNAMIC_CAST_STUB)){
              CastStat.DynamicCastCnt++;
              DetectMisuseOfCast(DYNAMIC_CAST_KIND,dstPointer,call);
              if(!DetectDynamic){
                continue;
              }
            }            
            AddDDAQuery(dstPointer);
            CastStat.AllCasts.insert(call);
          }
        }
      }
    }    
  }  
  // Return false to signal that the module was not modified by this pass.
  DBOUT(TCD_DEBUG, outs().flush(););
  return false;
}

void CastStubInfoCollector::SetPTA(PointerAnalysis * p){
  this->pta = p;
}

void CastStubInfoCollector::PrintStatistics(){
  DBOUT(TCD_DEBUG, outs() << "------------------------------ Statistics --------------------------------------------\n\n");
  CastStat.Print(llvm::outs());
  llvm::outs().flush();  
}
/*
 @dstType
      %struct.cStatistic = type <{ %struct.cObject, i32, [4 x i8] }>
 @actualType
      %struct.cStdDev = type { %struct.cStatistic.base, i64 }
      %struct.cStatistic.base = type <{ %struct.cObject, i32 }>
      %struct.cObject = type { i8* }
 */
static bool hasEqualName(llvm::StructType * dstType,llvm::StructType * actualType){
  if(dstType->hasName() && actualType->hasName()){
    std::string name = dstType->getName();
    name += ".base";
    llvm::StringRef nameWithBase(name);
    if(nameWithBase.equals(actualType->getName())){
      return true;
    }
  }
  return false;
}

// Test whether @dstType is contained in @actualType
bool CastStubInfoCollector::IsContained(llvm::StructType * dstType,llvm::StructType * actualType){
  if(actualType == dstType){
    return true;
  }
  if(hasEqualName(dstType,actualType)){
    return true;
  }
  for(Type::subtype_iterator it = actualType->subtype_begin(),
      e = actualType->subtype_end(); it != e; ++it){
    llvm::StructType * st = dyn_cast<llvm::StructType>(*it);
    if(st && IsContained(dstType,st)){
      return true;
    }
  }
  return false;
}

/*
  Test whether
  (1) @base is parent class of @derived
  (2) sizeof(@base) == sizeof(@derived)

  %struct.D = type { %struct.C }
 */
bool CastStubInfoCollector::IsPhantom(llvm::StructType * base,llvm::StructType * derived){
  return derived->elements().size() == 1 && (derived->elements())[0] == base;
  // FIXME:
//  llvm::StructType *eleTy = dyn_cast<llvm::StructType>((derived->elements())[0]);
//  Size_t baseSz = symTable->getTypeSizeInBytes(base);
//  Size_t derivedSz = symTable->getTypeSizeInBytes(base);
//  return (baseSz == derivedSz) && eleTy && ((eleTy == base) || hasEqualName(base,eleTy));
}

// Check whether @vt represents a virtual table
bool CastStubInfoCollector::IsCxxVTable(const llvm::Value * val){
  if(val && val->hasName() && val->getName().startswith(CXX_VTABLE_NAME_PREFIX)){
    return true;
  }
  return false;
}
/*
    Return true when it Must be bad
    FIXME:   we intentionally ignore some fake targets here.
 */
bool CastStubInfoCollector::IsBadType(llvm::StructType * actualType,llvm::StructType * dstType,const PAGNode * actual){
  if(actualType && dstType){
    if(!IsContained(dstType,actualType) && !IsPhantom(actualType,dstType)){
      if(IsCxxVTable(actual->getValue())){ // Filt fake targets of pointer analysis. Omnetpp in CPU2006
        return false;
      }else{
        return true;
      }
    }
  }
  return false;
}
// Return true when it Must be right
bool CastStubInfoCollector::IsRightType(llvm::StructType * actualType,llvm::StructType * dstType,const PAGNode * actual){
  if(actualType && dstType){
    if(IsContained(dstType,actualType) || IsPhantom(actualType,dstType)){
      return true;
    }
  }
  return false;
}

/*
  The implementation for IsBadCast() and IsRightCast()
  (1) The opposite of IsBadCast(): IsRightCast() or I don't know
  (2) The opposite of IsRightCast(): IsBadCast() or I don't know
*/
bool CastStubInfoCollector::DoCastTest(
                       llvm::Instruction * inst,
                       llvm::Value * src,
                       llvm::Value * dst,
                       const PAGNode * actual,
                       llvm::StructType * actualType,
                       IsXXXFuncType TestType){
  PointerType * ptrTy = getPointerTypeViaMetadata(inst,DST_TYPE_INFO_METADATA);
  // %struct.D
  assert(ptrTy && "ptrTy should not be nullptr.");
  llvm::StructType * dstType = dyn_cast<StructType>(ptrTy->getPointerElementType());
  if(actualType == NULL){
    return false;
  }else{
    return (this->*TestType)(actualType,dstType,actual);
  }
}

/*
    @param src
                 %5 = bitcast %struct.D* %4 to %struct.B*
    @param dst
                 %10 = bitcast i8* %9 to %struct.D*

    @param inst
                 call void @__au_edu_unsw_dynamic_cast_stub(i64 %11, i64 %12)

    %11 = ptrtoint %struct.B* %5 to i64, !dbg !953
    %12 = ptrtoint %struct.D* %10 to i64, !dbg !953
    call void @__au_edu_unsw_dynamic_cast_stub(i64 %11, i64 %12)

    @param actual
                 abstract object inferred from pointer analysis
    @return
      true:   It must be a bad cast
      false:  not sure
 */
bool CastStubInfoCollector::IsBadCast(llvm::Instruction * inst,llvm::Value * src, llvm::Value * dst, const PAGNode * actual){
  llvm::StructType * actualType = GetAbstractObjectType(actual,inst);
  if(actualType){
    do{
      // If one subtype is not bad cast, then we conservatively treat
      if(!DoCastTest(inst,src,dst,actual,actualType,&CastStubInfoCollector::IsBadType)){
        return false;
      }else{
        if(actualType->getNumContainedTypes() > 0){
          Type * ty = actualType->getContainedType(0);
          while(ArrayType * at = dyn_cast<ArrayType>(ty)){
            ty = at->getElementType();
          }
          actualType = dyn_cast<llvm::StructType>(ty);
        }else{
          break;
        }
      }
    }while(actualType);
    // If every subtype at field 0 is bad type, then it is a bad cast.
    return true;
  }else{
    return DoCastTest(inst,src,dst,actual,actualType,&CastStubInfoCollector::IsBadType);
  }
}
/*
    @return
      true:   It must be a right cast
      false:  not sure
 */
bool CastStubInfoCollector::IsRightCast(llvm::Instruction * inst,llvm::Value * src, llvm::Value * dst, const PAGNode* actual){

  //return DoCastTest(inst,src,dst,actual,&CastStubInfoCollector::IsRightType);
  llvm::StructType * actualType = GetAbstractObjectType(actual,inst);
  if(actualType){
    while(actualType){
      if(DoCastTest(inst,src,dst,actual,actualType,&CastStubInfoCollector::IsRightType)){
        return true;
      }else{
        if(actualType->getNumContainedTypes() > 0){
          Type * ty = actualType->getContainedType(0);
          while(ArrayType * at = dyn_cast<ArrayType>(ty)){
            ty = at->getElementType();
          }
          actualType = dyn_cast<llvm::StructType>(ty);
        }else{
          break;
        }
      }
    }
    // If every subtype at field 0 is not good type, then it is not a right cast.
    return false;
  }else{
    return DoCastTest(inst,src,dst,actual,actualType,&CastStubInfoCollector::IsRightType);
  }
}

/*
  Find possible type confusion positions.

  The result will be saved in CastStat .
*/
void CastStubInfoCollector::FindPossibleTypeConfusions(){
  // DBOUT_WITH_POS(TCD_DEBUG, outs() << "\n\n");
  // for every <src, {inst1,inst2, ...}> in map
  PAG* pag = pta->getPAG();
  for(CastStatistics::InstructionSet::iterator it = CastStat.AllCasts.begin(),
      e = CastStat.AllCasts.end(); it != e; ++it){
    const CallInst * call = cast<const CallInst>(*it);
    Value * src = call->getArgOperand(0);
    Value * dst = call->getArgOperand(1);
    const PAGNode* node = pag->getPAGNode(pag->getValueNode(dst));
    if(node && pag->isValidTopLevelPtr(node)){
      PointsTo& pts = pta->getPts(node->getId());
      CastStat.ValPointscntMap[dst] = pts.count();
      // for every actual abstract object
      int total_cnt = 0, bad_cnt = 0, right_cnt = 0;
      TypeConfusionEntry entry(call,src,dst);
      for (PointsTo::iterator pit = pts.begin(), pe = pts.end(); pit != pe; ++pit){
        const PAGNode* n = pta->getPAG()->getPAGNode(*pit);
        total_cnt++;
        if(n && n->hasValue()){
          if(IsBadCast((Instruction *) call,src, dst,n)){
            bad_cnt++;
            entry.AddPointee(n);
          }
          if(IsRightCast((Instruction *) call,src, dst,n)){
            right_cnt++;
            entry.AddPointee(n);
          }
        }
      }
      if(total_cnt > 0){
        if(bad_cnt == total_cnt){
          CastStat.DefinitelyUnsafeCasts.insert(call);
        }else if(right_cnt == total_cnt){
          CastStat.DefinitelySafeCasts.insert(call);
        }else{
          CastStat.UncertainCasts.insert(call);
        }
      }
      if(bad_cnt > 0){
        if(ConservativeTCD){
          if(total_cnt == bad_cnt){
            CastStat.TypeConfusionInfo.push_back(entry);
          }
        }else{
          CastStat.TypeConfusionInfo.push_back(entry);
        }
      }
    }
  }
}
/*
    Print histogram of Points-to information for debugging.
    For example,
      (0	11)         There are 11 pointers which point to nothing
      (1	15)         There are 15 pointers which only point to an abstract object
      (464	137)      There are 137 pointers which point to 464 abstract objects.
 */
void CastStubInfoCollector::PrintHistogram(){
  // Print Histogram
  DBOUT(TCD_DEBUG, outs() << "\n\n-------------------------------Histogram----------------------------------\n");
  DBOUT(TCD_DEBUG, outs() << "sizeof(points-to(ptr))\tcount\n");
  for(CastStatistics::ValPointscntMapType::iterator it = CastStat.ValPointscntMap.begin(),
      e = CastStat.ValPointscntMap.end(); it != e; ++it) {
    if(CastStat.Histogram.find(it->second) != CastStat.Histogram.end()){
      CastStat.Histogram[it->second]++;
    }else{
      CastStat.Histogram[it->second] = 1;
    }
  }
  for(CastStatistics::HistogramType::iterator it = CastStat.Histogram.begin(),
      e = CastStat.Histogram.end(); it != e; ++it){
    DBOUT(TCD_DEBUG, outs() << "(" << it->first << "\t\t\t" << it->second << ")\n");
  }
  int nodeCnt = 0;
  // print details of Points-to information
  DBOUT(TCD_DEBUG, outs() << "\n\n-------------------------------Points-to----------------------------------\n");
  for(CastStatistics::ValPointscntMapType::iterator it = CastStat.ValPointscntMap.begin(),
      e = CastStat.ValPointscntMap.end(); it != e; ++it){

    const Value * target = it->first;

    const PAGNode* node = pta->getPAG()->getPAGNode(symTable->getValSym(target));
    PointsTo& pts = pta->getPts(node->getId());
    nodeCnt++;
    DBOUT(TCD_DEBUG, outs() << "[" << nodeCnt << "] ");
    DBOUT(TCD_DEBUG, outs() << *node << "\n\t" << analysisUtil::getSourceLoc(target) << "\n");
    if (pts.empty()) {
      DBOUT(TCD_DEBUG, outs() << ": ===================> {empty}\n\n");
    } else {
      int pointeeCnt = 0;
      DBOUT(TCD_DEBUG, outs() << ": ===================> \n{\n");
      for (PointsTo::iterator it = pts.begin(), eit = pts.end();
           it != eit; ++it){
        pointeeCnt++;
        outs() << "(" << pointeeCnt << ")\n";
        const PAGNode* n = pta->getPAG()->getPAGNode(*it);
        if(n->hasValue()){
          DBOUT(TCD_DEBUG, outs() << "\t" << *n << "\n\t" << analysisUtil::getSourceLoc(n->getValue())<< "\n\n");
        }else{
          DBOUT(TCD_DEBUG, outs() << "\t******* DummyValNode or DummyObjNode ******" << "\n\n");
        }
      }
      DBOUT(TCD_DEBUG, outs() << "}\n\n");
    }
    DBOUT(TCD_DEBUG, outs() << "--------------------------------------------------------------------------\n\n");

  }
}


void CastStubInfoCollector::TypeConfusionReport(){
  // Type confusion report
  //int k = 0;
  CastStat.TypeErrorCnt = 0;
  for(CastStatistics::TypeConfusionInfoType::iterator it = CastStat.TypeConfusionInfo.begin(),
      e = CastStat.TypeConfusionInfo.end(); it != e; ++it){
    TypeConfusionEntry & entry = *it;
    llvm::CallInst * call = cast<llvm::CallInst>((llvm::Instruction *)entry.inst);    
    StringRef funcName = call->getCalledFunction()->getName();    
    PointerType * srcType = getPointerTypeViaMetadata(call,SRC_TYPE_INFO_METADATA);
    PointerType * dstType = getPointerTypeViaMetadata(call,DST_TYPE_INFO_METADATA);
    CastStat.TypeErrorCnt++;
    PAG * pag = pta->getPAG();

    DBOUT(TCD_DEBUG, outs() << "================================  ");
    if(CastStat.DefinitelyUnsafeCasts.find(call) != CastStat.DefinitelyUnsafeCasts.end()){
      DBOUT(TCD_DEBUG, outs() << "(MUST BE) ");
    }else{
      DBOUT(TCD_DEBUG, outs() << "(MAY BE) ");
    }
    DBOUT(TCD_DEBUG, outs() << "Type Error [" << CastStat.TypeErrorCnt
          << "] ================================\n");

    if(funcName.equals(REINTERPRET_CAST_STUB)){
      DBOUT(TCD_DEBUG, outs() << "Bad reinterpret_cast: \n");
    }else if(funcName.equals(STATIC_CAST_STUB)){
      DBOUT(TCD_DEBUG, outs() << "Bad static_cast: \n");
    }else{
      DBOUT(TCD_DEBUG, outs() << "Bad dynamic_cast: \n");
    }
    DBOUT(TCD_DEBUG, outs() << "\t" << *srcType << "   ====>   ");
    DBOUT(TCD_DEBUG, outs() << *dstType << "\n");

    DBOUT(TCD_DEBUG, outs() << "Where: \n\t");
    DBOUT(TCD_DEBUG, outs() << analysisUtil::getSourceLoc(entry.inst) << "\n");

    DBOUT(TCD_DEBUG, outs() << "Instruction: \n\t" << *(entry.inst) << "\n");
    DBOUT(TCD_DEBUG, outs() << "Src: \n\t" << *(srcType->getPointerElementType()) << "\n");
    pag->printNode(pag->getValueNode(entry.src));
    DBOUT(TCD_DEBUG, outs() << "Dst: \n\t" << *(dstType->getPointerElementType()) << "\n");
    pag->printNode(pag->getValueNode(entry.dst));
    DBOUT(TCD_DEBUG, outs() << "Actual types of " << entry.actual_pointees.size() << " pointee(s): \n");
    int i = 0;
    for(std::set<const PAGNode *>::iterator sit = entry.actual_pointees.begin(),
        se = entry.actual_pointees.end(); sit != se; ++sit){
      //
      i++;
      //assert(GetAbstractObjectType((llvm::Value *)*sit) != NULL && "It must be a struct.");
      StructType * st = GetAbstractObjectType(*sit,(Instruction *)entry.inst);
      DBOUT(TCD_DEBUG, outs() << "(" << i << ")\n");
      while(st){
        DBOUT(TCD_DEBUG, outs() << "\t" <<  *st << "\n");
        if(st->getNumContainedTypes() > 0){
          Type * ty = st->getContainedType(0);
          while(ArrayType * at = dyn_cast<ArrayType>(ty)){
            ty = at->getElementType();
          }
          st = dyn_cast<StructType>(ty);
        }else{
          break;
        }
      }      
      pag->printNode((*sit)->getId());
    }

    DBOUT(TCD_DEBUG, outs() << "\n\n");
    outs().flush();
  }
}

void CastStubInfoCollector::PrintTargetCasts(std::string title, std::set<const llvm::Instruction *> &casts){
  int cnt = 0;
  DBOUT(TCD_DEBUG,outs() << "--------------------- The number of " << title << " is "
                         << casts.size()
                         << " ------------------\n");

  for(auto &inst: casts){
    cnt++;
    DBOUT(TCD_DEBUG, outs() << title << " (" << cnt << "):\n\t");
    DBOUT(TCD_DEBUG, outs() << "Instruction: \n\t" << *inst << "\n\t");
    DBOUT(TCD_DEBUG, outs() << "Where: \n\t");
    DBOUT(TCD_DEBUG, outs() << analysisUtil::getSourceLoc(inst) << "\n\t");
    const CallInst * call = cast<const CallInst>(inst);
    NodeID nodeId = pta->getPAG()->getValueNode(call->getArgOperand(1));
    DBOUT(TCD_DEBUG, outs() << "Dst NodeId: " << nodeId << "\n\n");


  }
  DBOUT(TCD_DEBUG, outs() << "\n\n");
}

static bool usedInIfGuard(const llvm::Value *dst){
  // FIXME: bug
//  for(Value::const_user_iterator it = dst->user_begin(), ie = dst->user_end(); it != ie; ++it){
//    //%13 = icmp eq %struct.C* %9, null, !dbg !82
//    if(const llvm::ICmpInst *icmp = dyn_cast<llvm::ICmpInst>(*it)){
//      if(Constant *conVal = dyn_cast<Constant>(icmp->getOperand(1))){
//        if(conVal->isNullValue()){
//          return true;
//        }
//      }
//    }
//    // Phi(dst, ...)
//    else if(const llvm::PHINode *phi = dyn_cast<llvm::PHINode>(*it)){
//      if(usedInIfGuard(phi)){
//        return true;
//      }
//    }
//  }
  return false;
}

/*
  Check whether static_cast, dynamic_cast, and reinterpret_cast are misused.
 */
void CastStubInfoCollector::DetectMisuseOfCast(CastKind kind, const llvm::Value *dst, const Instruction * inst){
#if 0
  static int misuseCnt = 0;
  if(0 == misuseCnt){
    DBOUT(TCD_DEBUG, outs() << "------------------------------- Misuse of C++ Casting -------------------------------------------\n\n");
    misuseCnt = 1;
  }
  if(const BitCastOperator *bitcast = dyn_cast<BitCastOperator>(dst)){
    dst = bitcast->getOperand(0);
  }
  bool hasIfGuard = usedInIfGuard(dst);

  if((kind == STATIC_CAST_KIND) && hasIfGuard){
    DBOUT(TCD_DEBUG, outs() << "[" << misuseCnt << "] " << analysisUtil::getSourceLoc(inst) << "\n");
    DBOUT(TCD_DEBUG, outs() << "\t\t Do you mean dynamic_cast<> ?\n\n");
    this->CastStat.StaticMisuseCnt++;
    misuseCnt++;
  }
  else if((kind == REINTERPRET_CAST_KIND) && hasIfGuard){
    DBOUT(TCD_DEBUG, outs() << "[" << misuseCnt << "] " << analysisUtil::getSourceLoc(inst) << "\n");
    DBOUT(TCD_DEBUG, outs() << "\t\t Do you mean dynamic_cast<> ?\n\n");
    this->CastStat.ReinterpretMisuseCnt++;
    misuseCnt++;
  }
//  For dynamic_cast, it is essentially a Nullpointer Problem.
//  else if((kind == DYNAMIC_CAST_KIND) && !hasIfGuard && used){
//    DBOUT(TCD_DEBUG, outs() << "[" << misuseCnt << "] " << analysisUtil::getSourceLoc(inst) << "\n");
//    DBOUT(TCD_DEBUG, outs() << "\t\t Please check the result of dynamic_cast<> before using it.\n\n");
//    this->CastStat.DynamicMisuseCnt++;
//    misuseCnt++;
//  }
#endif
}

/*
    Detect down-cast type confusion.
 */
void CastStubInfoCollector::DetectTypeConfustion(){
  if(pta == NULL){
    return;
  }
  //
  FindPossibleTypeConfusions();
  PrintHistogram();
  TypeConfusionReport();
  //
  PrintTargetCasts("DefinitelySafeCasts",CastStat.DefinitelySafeCasts);
  PrintTargetCasts("DefinitelyUnsafeCasts",CastStat.DefinitelyUnsafeCasts);
  PrintTargetCasts("UncertainCasts",CastStat.UncertainCasts);

  DBOUT(TCD_DEBUG,outs() << "The number of definitely SAFE casts " <<  " is " << CastStat.DefinitelySafeCasts.size() << "\n"
                         << "The number of definitely UNSAFE casts " <<  " is " << CastStat.DefinitelyUnsafeCasts.size() << "\n"
                         << "The number of UNCERTAIN casts " <<  " is " << CastStat.UncertainCasts.size() << "\n\n\n");
}

