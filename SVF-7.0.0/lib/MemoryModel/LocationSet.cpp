//===- LocationSet.cpp -- Location set for modeling abstract memory object----//
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
 * @file: LocationSet.cpp
 * @author: yesen
 * @date: 26 Sep 2014
 *
 * LICENSE
 *
 */


#include "MemoryModel/LocationSet.h"
#include "MemoryModel/MemModel.h"
#include <llvm/Support/CommandLine.h> // for tool output file
#if 1
#include <Util/AnalysisUtil.h>
#endif
using namespace llvm;

static cl::opt<bool> singleStride("stride-only", cl::init(false),
                                  cl::desc("Only use single stride in LocMemoryModel"));

/*!
 * Add element num and stride pair
 */
void LocationSet::addElemNumStridePair(const NodePair& pair) {
    /// The pair will not be added if any number of a stride is zero,
    /// because they will not have effect on the locations represented by this LocationSet.
    if (pair.first == 0 || pair.second == 0)
        return;

    if (singleStride) {
        if (numStridePair.empty())
            numStridePair.push_back(std::make_pair(SymbolTableInfo::getMaxFieldLimit(),pair.second));
        else {
            /// Find the GCD stride
            NodeID existStride = (*numStridePair.begin()).second;
            NodeID newStride = gcd(pair.second, existStride);
            if (newStride != existStride) {
                numStridePair.pop_back();
                numStridePair.push_back(std::make_pair(SymbolTableInfo::getMaxFieldLimit(),newStride));
            }
        }
    }
    else {
        numStridePair.push_back(pair);
    }
}


/*!
 * Return TRUE if it successfully increases any index by 1
 */
bool LocationSet::increaseIfNotReachUpperBound(std::vector<NodeID>& indices,
        const ElemNumStridePairVec& pairVec) const {
    assert(indices.size() == pairVec.size() && "vector size not match");

    /// Check if all indices reach upper bound
    bool reachUpperBound = true;
    for (u32_t i = 0; i < indices.size(); i++) {
        assert(pairVec[i].first > 0 && "number must be greater than 0");
        if (indices[i] < (pairVec[i].first - 1))
            reachUpperBound = false;
    }

    /// Increase index if not reach upper bound
    bool increased = false;
    if (reachUpperBound == false) {
        u32_t i = 0;
        while (increased == false) {
            if (indices[i] < (pairVec[i].first - 1)) {
                indices[i] += 1;
                increased = true;
            }
            else {
                indices[i] = 0;
                i++;
            }
        }
    }

    return increased;
}
/*
  struct Info{
    int number[8];
    double income;
  };
  struct Data{
    Info a;
    double total;
    Info b[4];
    Info d;
  };
  Data dt;
  ----------------------------------- When LocMemModel in MemModel.cpp is enabled ---------------------------------------------
  {Type: %struct.Data = type { %struct.Info, double, [4 x %struct.Info], %struct.Info }}
    Field_idx = 0 [offset: 0, field type: i32, field size: 4, field stride pair: [ 1, 0 ] [ 8, 4 ] [ 1, 0 ] [ 1, 0 ]
    Field_idx = 1 [offset: 32, field type: double, field size: 8, field stride pair: [ 1, 0 ] [ 1, 0 ] [ 1, 0 ]
    Field_idx = 2 [offset: 40, field type: double, field size: 8, field stride pair: [ 1, 0 ] [ 1, 0 ]
    Field_idx = 3 [offset: 48, field type: i32, field size: 4, field stride pair: [ 1, 0 ] [ 8, 4 ] [ 1, 0 ] [ 4, 40 ] [ 1, 0 ]
    Field_idx = 4 [offset: 80, field type: double, field size: 8, field stride pair: [ 1, 0 ] [ 1, 0 ] [ 4, 40 ] [ 1, 0 ]
    Field_idx = 5 [offset: 208, field type: i32, field size: 4, field stride pair: [ 1, 0 ] [ 8, 4 ] [ 1, 0 ] [ 1, 0 ]
    Field_idx = 6 [offset: 240, field type: double, field size: 8, field stride pair: [ 1, 0 ] [ 1, 0 ] [ 1, 0 ]

  For example:

    The possible locations of dt.b[i].number[j] can be calculated by calling computeAllLocations().
    LocationSet:
      offset = 48;
      ElemNumStridePairVec = {[8, sizeof(int)], [4, sizeof(Info)]
                           = {[8,4],[4,40]}
    The possible locations are :
    48 + i * sizeof(Info) + j * sizeof(int), where 0 <= i < 4, 0 <= j < 8,
    48 + i * 40 + j * 4

    We can use the following for() to get the result.
    for(0 <= i < 4){
      for(0 <= j < 8){
      }
    }
    But since the number of pairs in ElemNumStridePairVec is unknown,
    we don't know how many for() are needed when writing computeAllLocations().
    Suppose we have k pairs, that is, (N1,S1), (N2,S2), ..., (Nk,Sk), where Ni > 0, Si > 0
    there will be (N1 * N2 * N3 * ...* Nk) possible locations.
    We can treat it as an integer and then increment it step by step(starting from 0).
 */

/*!
 * Compute all possible locations according to offset and number-stride pairs.
 */
PointsTo LocationSet::computeAllLocations() const {

    PointsTo result;
    result.set(getFldIdx());

    if (isConstantOffset() == false) {
        const ElemNumStridePairVec& lhsVec = getNumStridePair();
        std::vector<NodeID> indices;
        u32_t size = lhsVec.size();
        while (size) {
            indices.push_back(0);
            size--;
        }

        do {
            u32_t i = 0;
            NodeID ofst = getFldIdx();
            while (i < lhsVec.size()) {
                ofst += (lhsVec[i].second * indices[i]);
                i++;
            }

            result.set(ofst);

        } while (increaseIfNotReachUpperBound(indices, lhsVec));
    }

    return result;
}

#if 1  // added
bool LocationSet::operator< (const LocationSet& rhs) const {
    //return fldIdx < rhs.fldIdx;
    if(fldIdx != rhs.fldIdx){
        return fldIdx < rhs.fldIdx;
    }
    else{
        const ElemNumStridePairVec& pairVec = getNumStridePair();
        const ElemNumStridePairVec& rhsPairVec = rhs.getNumStridePair();
        if (pairVec.size() != rhsPairVec.size())
            return (pairVec.size() < rhsPairVec.size());
        else {
            ElemNumStridePairVec::const_iterator it = pairVec.begin();
            ElemNumStridePairVec::const_iterator rhsIt = rhsPairVec.begin();
            for (; it != pairVec.end() && rhsIt != rhsPairVec.end(); ++it, ++rhsIt) {
                if ((*it).first != (*rhsIt).first)
                    return ((*it).first < (*rhsIt).first);
                else if ((*it).second != (*rhsIt).second)
                    return ((*it).second < (*rhsIt).second);
            }

            return false;
        }
    }
}

LocationSet LocationSet::operator+ (const LocationSet& rhs) const {
    LocationSet ls(rhs);
    ls.fldIdx += getFldIdx();
    ls.baseType = this->baseType;
    ls.byteOffset += byteOffset;    
#if 1    
    ls.minusOffset += this->minusOffset;
    if(StructType * st = dyn_cast_or_null<StructType>(baseType)){
        Size_t sz = SymbolTableInfo::Symbolnfo()->getTypeSizeInBytes(st);
        const std::vector<FieldInfo> &fieldInfo  = SymbolTableInfo::Symbolnfo()->getFlattenFieldInfoVec(st);
        Size_t bo = fieldInfo[fldIdx].getFlattenByteOffset();
        bo += rhs.byteOffset;
        // If there is minus byteoffset, we need to adjust the field index.
        if(bo >= 0 && (rhs.byteOffset < 0 || byteOffset < 0)){
            bo %= sz;
            Size_t idx = analysisUtil::getFldIdxViaByteOffset(st,bo);            
            if(idx >= 0){
                ls.fldIdx = idx;
            }
        }
    }
#endif
    ElemNumStridePairVec::const_iterator it = getNumStridePair().begin();
    ElemNumStridePairVec::const_iterator eit = getNumStridePair().end();
    for (; it != eit; ++it)
        ls.addElemNumStridePair(*it);

    return ls;
}
#endif
