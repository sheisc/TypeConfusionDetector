//===- LocationSet.h -- Location set of abstract object-----------------------//
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
 * @file: LocationSet.h
 * @author: yesen
 * @date: 26 Sep 2014
 *
 * LICENSE
 *
 */


#ifndef LOCATIONSET_H_
#define LOCATIONSET_H_


#include "Util/BasicTypes.h"

#include <llvm/IR/Type.h>
#if 1
#include <llvm/IR/Operator.h>
#endif
/*!
 * Field information of an aggregate object
 */
#if 0
class FieldInfo {
public:
    typedef std::vector<NodePair > ElemNumStridePairVec;

private:
    u32_t offset;
    const llvm::Type* elemTy;
#if 1
    const llvm::Type* unFlattenTy;
#endif
    ElemNumStridePairVec elemNumStridePair;
public:
#if 0
    FieldInfo(u32_t of, const llvm::Type* ty, ElemNumStridePairVec pa):
        offset(of), elemTy(ty), elemNumStridePair(pa) {
    }
#endif
#if 1 // added 2018.11.08
    FieldInfo(u32_t of, const llvm::Type* ty, ElemNumStridePairVec pa, const llvm::Type * unTy):
        offset(of), elemTy(ty), elemNumStridePair(pa),unFlattenTy(unTy){
    }
#endif
    inline u32_t getFlattenOffset() const {
        return offset;
    }
    inline const llvm::Type* getFlattenElemTy() const {
        return elemTy;
    }
#if 1  // added 2018.11.08
    inline const llvm::Type* getUnFlattenType() const {
        return unFlattenTy;
    }
#endif
    inline const ElemNumStridePairVec& getElemNumStridePairVect() const {
        return elemNumStridePair;
    }
    inline ElemNumStridePairVec::const_iterator elemStridePairBegin() const {
        return elemNumStridePair.begin();
    }
    inline ElemNumStridePairVec::const_iterator elemStridePairEnd() const {
        return elemNumStridePair.end();
    }
};
#endif
#if 1  // modified 2018.11.13
class FieldInfo {
public:
    typedef std::vector<NodePair > ElemNumStridePairVec;

private:
    u32_t fldIdx;
    u32_t byteOffset;
    const llvm::Type* elemTy;
    const llvm::Type *largestSubTy;
    const llvm::Type *baseTy;
    ElemNumStridePairVec elemNumStridePair;
public:
    FieldInfo(u32_t idx, u32_t byteOff, const llvm::Type* ty, ElemNumStridePairVec pa,const llvm::Type* largest,const llvm::Type* bt) :
        fldIdx(idx), byteOffset(byteOff), elemTy(ty), elemNumStridePair(pa), largestSubTy(largest), baseTy(bt) {
    }
    inline u32_t getFlattenFldIdx() const {
        return fldIdx;
    }
    inline u32_t getFlattenByteOffset() const {
        return byteOffset;
    }
    inline const llvm::Type* getFlattenElemTy() const {
        return elemTy;
    }
    inline const llvm::Type* getLargestSubTy() const {
        return largestSubTy;
    }
    inline const llvm::Type* getBaseTy() const {
        return baseTy;
    }
    inline const ElemNumStridePairVec& getElemNumStridePairVect() const {
        return elemNumStridePair;
    }
    inline ElemNumStridePairVec::const_iterator elemStridePairBegin() const {
        return elemNumStridePair.begin();
    }
    inline ElemNumStridePairVec::const_iterator elemStridePairEnd() const {
        return elemNumStridePair.end();
    }
};
#endif

/*
 * Location set represents a set of locations in a memory block with following offsets:
 *     { offset + \sum_{i=0}^N (stride_i * j_i) | 0 \leq j_i < M_i }
 * where N is the size of number-stride pair vector, M_i (stride_i) is i-th number (stride)
 * in the number-stride pair vector.
 */
#if 0
class LocationSet {
    friend class SymbolTableInfo;
    friend class LocSymTableInfo;
public:
    enum LSRelation {
        NonOverlap, Overlap, Subset, Superset, Same
    };

    typedef FieldInfo::ElemNumStridePairVec ElemNumStridePairVec;
#if 0
    /// Constructor
    LocationSet(Size_t o = 0) : offset(o)
    {
        this->gepOp = nullptr;
    }
#endif
#if 1  // modified 2018.11.08
    /// Constructor
    LocationSet(Size_t o,Size_t bo, llvm::Type * bt) : offset(o),byteOffset(bo), baseType(bt)
    {

    }
#endif

    /// Copy Constructor
    LocationSet(const LocationSet& ls) : offset(ls.offset)
    {
        const ElemNumStridePairVec& vec = ls.getNumStridePair();
        ElemNumStridePairVec::const_iterator it = vec.begin();
        ElemNumStridePairVec::const_iterator eit = vec.end();
        for (; it != eit; ++it)
            addElemNumStridePair(*it);
#if 1
        this->baseType = ls.baseType;
        this->byteOffset = ls.byteOffset;
#endif
    }
#if 1
    /// Initialization from FieldInfo
    LocationSet(const FieldInfo& fi) : offset(fi.getFlattenOffset())
    {
        const ElemNumStridePairVec& vec = fi.getElemNumStridePairVect();
        ElemNumStridePairVec::const_iterator it = vec.begin();
        ElemNumStridePairVec::const_iterator eit = vec.end();
        for (; it != eit; ++it)
            addElemNumStridePair(*it);
#if 1   // added 2018.11.08
        this->baseType = nullptr;
        this->byteOffset = 0;
#endif
    }
#endif
    ~LocationSet() {}


    /// Overload operators
    //@{
    inline LocationSet operator+ (const LocationSet& rhs) const {
        LocationSet ls(rhs);
        ls.offset += getOffset();
        ElemNumStridePairVec::const_iterator it = getNumStridePair().begin();
        ElemNumStridePairVec::const_iterator eit = getNumStridePair().end();
        for (; it != eit; ++it)
            ls.addElemNumStridePair(*it);
#if 1
        ls.baseType = this->baseType;        
        ls.byteOffset = this->byteOffset + rhs.byteOffset;        
#endif
        return ls;
    }
    inline const LocationSet& operator= (const LocationSet& rhs) {
        offset = rhs.offset;
        numStridePair = rhs.getNumStridePair();
#if 1
        this->baseType = rhs.baseType;
        this->byteOffset = rhs.byteOffset;
#endif
        return *this;
    }
#if 0
    inline bool operator< (const LocationSet& rhs) const {

        if (offset != rhs.offset)
            return (offset < rhs.offset);
        else {
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
#endif
#if 1 // modified 2018.11.12
    bool operator< (const LocationSet& rhs) const;
#endif
    //@}

    /// Get methods
    //@{
    inline Size_t getOffset() const {
        return offset;
    }
    inline const ElemNumStridePairVec& getNumStridePair() const {
        return numStridePair;
    }

    //@}

    void addElemNumStridePair(const NodePair& pair);

    /// Return TRUE if this is a constant location set.
    inline bool isConstantOffset() const {
        return (numStridePair.size() == 0);
    }

    /// Return TRUE if we share any location in common with RHS
    inline bool intersects(const LocationSet& RHS) const {
        return computeAllLocations().intersects(RHS.computeAllLocations());
    }

    /// Check relations of two location sets
    static inline LSRelation checkRelation(const LocationSet& LHS, const LocationSet& RHS) {
        PointsTo lhsLocations = LHS.computeAllLocations();
        PointsTo rhsLocations = RHS.computeAllLocations();
        if (lhsLocations.intersects(rhsLocations)) {
            if (lhsLocations == rhsLocations)
                return Same;
            else if (lhsLocations.contains(rhsLocations))
                return Superset;
            else if (rhsLocations.contains(lhsLocations))
                return Subset;
            else
                return Overlap;
        }
        else {
            return NonOverlap;
        }
    }

    /// Dump location set
    void dump() const {
        llvm::outs() << "LocationSet\tOffset: " << getOffset()
                     << ",\tNum-Stride: {";
        const ElemNumStridePairVec& vec = getNumStridePair();
        ElemNumStridePairVec::const_iterator it = vec.begin();
        ElemNumStridePairVec::const_iterator eit = vec.end();
        for (; it != eit; ++it) {
            llvm::outs() << " (" << it->first << "," << it->second << ")";
        }
        llvm::outs() << " }\n";
    }

    Size_t getByteOffset() const{
        return byteOffset;
    }

    void setByteOffset(Size_t bo){
        byteOffset = bo;
    }

    llvm::Type * getBaseType() const{
        return baseType;
    }


private:
    /// Return TRUE if successfully increased any index by 1
    bool increaseIfNotReachUpperBound(std::vector<NodeID>& indices,	const ElemNumStridePairVec& pairVec) const;

    /// Compute all possible locations according to offset and number-stride pairs.
    PointsTo computeAllLocations() const;

    /// Return greatest common divisor
    inline unsigned gcd (unsigned n1, unsigned n2) const {
        return (n2 == 0) ? n1 : gcd (n2, n1 % n2);
    }

    Size_t offset;	///< offset relative to base
    ElemNumStridePairVec numStridePair;	///< element number and stride pair
#if 1
    llvm::Type * baseType;
    Size_t byteOffset;
#endif
};
#endif

#if 1  // added 2018.11.13
class LocationSet {
    friend class SymbolTableInfo;
    friend class LocSymTableInfo;
public:
    enum LSRelation {
        NonOverlap, Overlap, Subset, Superset, Same
    };

    typedef FieldInfo::ElemNumStridePairVec ElemNumStridePairVec;

    /// Constructor
    LocationSet(Size_t o,Size_t bo, llvm::Type * bt, Size_t mo) : fldIdx(o), byteOffset(bo),baseType(bt),minusOffset(mo)
    {
    }

    /// Copy Constructor
    LocationSet(const LocationSet& ls) : fldIdx(ls.fldIdx), byteOffset(ls.byteOffset),baseType(ls.baseType),minusOffset(ls.minusOffset)
    {
        const ElemNumStridePairVec& vec = ls.getNumStridePair();
        ElemNumStridePairVec::const_iterator it = vec.begin();
        ElemNumStridePairVec::const_iterator eit = vec.end();
        for (; it != eit; ++it)
            addElemNumStridePair(*it);
    }

    /// Initialization from FieldInfo
    LocationSet(const FieldInfo& fi) : fldIdx(fi.getFlattenFldIdx())
    {
        const ElemNumStridePairVec& vec = fi.getElemNumStridePairVect();
        ElemNumStridePairVec::const_iterator it = vec.begin();
        ElemNumStridePairVec::const_iterator eit = vec.end();
        for (; it != eit; ++it)
            addElemNumStridePair(*it);
#if 1  // FIXME
        byteOffset = fi.getFlattenByteOffset();
        baseType = (llvm::Type *)fi.getBaseTy();
        minusOffset = 0;
#endif
    }

    ~LocationSet() {}


    /// Overload operators
    //@{
    LocationSet operator+ (const LocationSet& rhs) const;

    inline const LocationSet& operator= (const LocationSet& rhs) {
        fldIdx = rhs.fldIdx;
        byteOffset = rhs.byteOffset;
#if 1
        baseType = rhs.baseType;
        minusOffset = rhs.minusOffset;
#endif
        numStridePair = rhs.getNumStridePair();
        return *this;
    }
    bool operator< (const LocationSet& rhs) const;
    //@}

    /// Get methods
    //@{
    inline Size_t getFldIdx() const {
        return fldIdx;
    }
    inline void setFldIdx(Size_t idx) {
        fldIdx = idx;
    }
    inline Size_t getByteOffset() const {
        return byteOffset;
    }
    inline void setByteOffset(Size_t os) {
        byteOffset = os;
    }
    inline Size_t getMinusOffset() const {
        return minusOffset;
    }
    inline void setMinusOffset(Size_t mo) {
        minusOffset = mo;
    }
    inline const ElemNumStridePairVec& getNumStridePair() const {
        return numStridePair;
    }
    inline llvm::Type * getBaseType() const{
        return baseType;
    }
    inline void setBaseType(llvm::Type * ty){
        baseType = ty;
    }
    //@}

    void addElemNumStridePair(const NodePair& pair);

    /// Return TRUE if this is a constant location set.
    inline bool isConstantOffset() const {
        return (numStridePair.size() == 0);
    }

    /// Return TRUE if we share any location in common with RHS
    inline bool intersects(const LocationSet& RHS) const {
        return computeAllLocations().intersects(RHS.computeAllLocations());
    }

    /// Check relations of two location sets
    static inline LSRelation checkRelation(const LocationSet& LHS, const LocationSet& RHS) {
        PointsTo lhsLocations = LHS.computeAllLocations();
        PointsTo rhsLocations = RHS.computeAllLocations();
        if (lhsLocations.intersects(rhsLocations)) {
            if (lhsLocations == rhsLocations)
                return Same;
            else if (lhsLocations.contains(rhsLocations))
                return Superset;
            else if (rhsLocations.contains(lhsLocations))
                return Subset;
            else
                return Overlap;
        }
        else {
            return NonOverlap;
        }
    }

    /// Dump location set
    void dump() const {
        llvm::outs() << "LocationSet\tField_Index: " << getFldIdx();
        llvm::outs() << "\tOffset: " << getByteOffset()
                     << ",\tNum-Stride: {";
        const ElemNumStridePairVec& vec = getNumStridePair();
        ElemNumStridePairVec::const_iterator it = vec.begin();
        ElemNumStridePairVec::const_iterator eit = vec.end();
        for (; it != eit; ++it) {
            llvm::outs() << " (" << it->first << "," << it->second << ")";
        }
        llvm::outs() << " }\n";
    }
private:
    /// Return TRUE if successfully increased any index by 1
    bool increaseIfNotReachUpperBound(std::vector<NodeID>& indices,	const ElemNumStridePairVec& pairVec) const;

    /// Compute all possible locations according to offset and number-stride pairs.
    PointsTo computeAllLocations() const;

    /// Return greatest common divisor
    inline unsigned gcd (unsigned n1, unsigned n2) const {
        return (n2 == 0) ? n1 : gcd (n2, n1 % n2);
    }

    Size_t fldIdx;	///< offset relative to base
    Size_t byteOffset;	///< offset relative to base
    Size_t minusOffset;
    ElemNumStridePairVec numStridePair;	///< element number and stride pair
    llvm::Type * baseType;
};
#endif

#endif /* LOCATIONSET_H_ */
