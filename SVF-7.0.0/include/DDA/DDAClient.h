/*
 * @file: DDAClient.h
 * @author: yesen
 * @date: 4 Feb 2015
 *
 * LICENSE
 *
 */


#ifndef DDACLIENT_H_
#define DDACLIENT_H_


#include "MemoryModel/PAG.h"
#include "MemoryModel/PAGBuilder.h"
#include "MemoryModel/PointerAnalysis.h"
#include "MSSA/SVFGNode.h"
#include "Util/BasicTypes.h"
#include "Util/CPPUtil.h"
#include <llvm/IR/DataLayout.h>


/**
 * General DDAClient which queries all top level pointers by default.
 */
class DDAClient {
public:
    DDAClient(SVFModule mod) : pag(NULL), module(mod), curPtr(0), solveAll(true) {}

    virtual ~DDAClient() {}

    virtual inline void initialise(SVFModule module) {}

    /// Collect candidate pointers for query.
    virtual inline NodeBS& collectCandidateQueries(PAG* p) {
        setPAG(p);
        if (solveAll)
            candidateQueries = pag->getAllValidPtrs();
        else {
            for (NodeBS::iterator it = userInput.begin(), eit = userInput.end(); it != eit; ++it)
                addCandidate(*it);
        }
        return candidateQueries;
    }
    /// Get candidate queries
    inline const NodeBS& getCandidateQueries() const {
        return candidateQueries;
    }

    /// Call back used by DDAVFSolver.
    virtual inline void handleStatement(const SVFGNode* stmt, NodeID var) {}
    /// Set PAG graph.
    inline void setPAG(PAG* g) {
        pag = g;
    }
    /// Set the pointer being queried.
    void setCurrentQueryPtr(NodeID ptr) {
        curPtr = ptr;
    }
    /// Set pointer to be queried by DDA analysis.
    void setQuery(NodeID ptr) {
        userInput.set(ptr);
        solveAll = false;
    }
    /// Get LLVM module
    inline SVFModule getModule() const {
        return module;
    }
    virtual void answerQueries(PointerAnalysis* pta);

    virtual inline void performStat(PointerAnalysis* pta) {}

    virtual inline void collectWPANum(SVFModule mod) {}
protected:
    void addCandidate(NodeID id) {
#if 1
        if (pag->isValidTopLevelPtr(pag->getPAGNode(id)))
            candidateQueries.set(id);
#endif
#if 0   // modified 2018.10.25
        if (pag->isValidPointer(id)){
            candidateQueries.set(id);
        }
#endif

    }

    PAG*   pag;					///< PAG graph used by current DDA analysis
    SVFModule module;		///< LLVM module
    NodeID curPtr;				///< current pointer being queried
    NodeBS candidateQueries;	///< store all candidate pointers to be queried

private:
    NodeBS userInput;           ///< User input queries
    bool solveAll;				///< TRUE if all top level pointers are being queried
};


/**
 * DDA client with function pointers as query candidates.
 */
class FunptrDDAClient : public DDAClient {
private:
    typedef std::map<NodeID,llvm::CallSite> VTablePtrToCallSiteMap;
    VTablePtrToCallSiteMap vtableToCallSiteMap;
public:
    FunptrDDAClient(SVFModule module) : DDAClient(module) {}
    ~FunptrDDAClient() {}

    /// Only collect function pointers as query candidates.
    virtual inline NodeBS& collectCandidateQueries(PAG* p) {
        setPAG(p);
        for(PAG::CallSiteToFunPtrMap::const_iterator it = pag->getIndirectCallsites().begin(),
                eit = pag->getIndirectCallsites().end(); it!=eit; ++it) {
            if (cppUtil::isVirtualCallSite(it->first)) {
                const llvm::Value *vtblPtr = cppUtil::getVCallVtblPtr(it->first);
                assert(pag->hasValueNode(vtblPtr) && "not a vtable pointer?");
                NodeID vtblId = pag->getValueNode(vtblPtr);
                addCandidate(vtblId);
                vtableToCallSiteMap[vtblId] = it->first;
            } else {
                addCandidate(it->second);
            }
        }
        return candidateQueries;
    }
    virtual void performStat(PointerAnalysis* pta);
};


#endif /* DDACLIENT_H_ */
