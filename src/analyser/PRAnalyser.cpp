#include "PRAnalyser.h"
#include "../ast/ASTHelper.cpp"
#include "../symbolutil/SymbolTable.h"
#include "analyserUtil.hpp"

bool checkIdEqual(Identifier *id1, Identifier *id2)
{
    return (strcmp(id1->getIdentifier(), id2->getIdentifier()) == 0);
}

bool checkIdNameEqual(Identifier *id1, char *c)
{
    return (strcmp(id1->getIdentifier(), c) == 0);
}

bool checkPropEqual(PropAccess *prop1, PropAccess *prop2)
{
    return (strcmp(prop1->getIdentifier2()->getIdentifier(), prop2->getIdentifier2()->getIdentifier()) == 0);
}

BoolProp PRAnalyser::analyseFor(forallStmt *stmt)
{
    statement *body = stmt->getBody();
    switch (body->getTypeofNode())
    {
        case NODE_ASSIGN:
        {
            assignment *assignStmt = (assignment *)body;
            usedVariables usedVars = getVarsExpr(assignStmt->getExpr());
            list <PropAccess *> usedProps = usedVars.getPropAcess();
            if (usedProps.size() > 0)
                return BoolProp(true, usedProps.front());
            break;
        }
        case NODE_BLOCKSTMT:
        {
            blockStatement *blockStmt = (blockStatement *)body;
            for (statement *stmt: blockStmt->returnStatements())
                if (stmt->getTypeofNode() == NODE_ASSIGN)
                {
                    assignment *assnStmt = (assignment *)stmt;
                    usedVariables usedVars = getVarsExpr(assnStmt->getExpr());
                    list <PropAccess *> usedProps = usedVars.getPropAcess();
                    if (usedProps.size() > 0)
                        return BoolProp(true, usedProps.front());
                }
        }
    }
    return BoolProp(false, NULL);
}

void PRAnalyser::analyseForAll(forallStmt *stmt)
{
    Identifier *itr = stmt->getIterator();
    Identifier *sourceGraph = stmt->getSourceGraph();

    statement *body = stmt->getBody();

    if (body->getTypeofNode() == NODE_BLOCKSTMT)
    {
        bool read_flag = false, write_flag = false;
        PropAccess *read_prop = NULL;
        blockStatement *blockStmt = (blockStatement *)body;
        int count = 1;
        for (statement *stmt: blockStmt->returnStatements())
        {
            switch (stmt->getTypeofNode())
            {
                case NODE_FORALLSTMT:
                {
                    forallStmt *currStmt = (forallStmt *)stmt;
                    proc_callExpr *procCall = currStmt->getExtractElementFunc();
                    if (currStmt->isSourceProcCall() && (procCall->getArgList().size() == 1) 
                        && checkIdEqual(sourceGraph, currStmt->getSourceGraph())
                        && (checkIdNameEqual(procCall->getMethodId(), "neighbours")
                            || checkIdNameEqual(procCall->getMethodId(), "nodes_to")))
                    {
                        BoolProp bp = analyseFor(currStmt);
                        if (bp.getBool())
                        {
                            read_flag = true;
                            read_prop = bp.getProp();
                        }
                    }
                    break;
                }
                case NODE_ASSIGN:
                {
                    assignment *assignStmt = (assignment *)stmt;
                    if (assignStmt->lhs_isProp())
                    {
                        PropAccess *lhs_prop = assignStmt->getPropId();
                        if (checkPropEqual(lhs_prop, read_prop))
                        {
                            // TODO: Add a barrier
                            // cout << "barrier needed" << endl;
                            barrierStmt *barrier = barrierStmt::nodeForBarrier();
                            blockStmt->insertToBlock(barrier, count);
                        }
                    }
                }
            }
            ++count;
        }
    }
}

void PRAnalyser::analyseBlock(statement *stmt)
{
    blockStatement *blockStmt = (blockStatement *)stmt;
    for (statement *stmt: blockStmt->returnStatements())
    {
        switch (stmt->getTypeofNode())
        {
            case NODE_BLOCKSTMT:
            {
                analyseBlock(stmt);
                break;
            }
            case NODE_FORALLSTMT:
            {
                forallStmt *currStmt = (forallStmt *)stmt;
                if (currStmt->isForall() && checkIdNameEqual(currStmt->getExtractElementFunc()->getMethodId(), "nodes"))
                    analyseForAll((forallStmt *)currStmt);
                else
                    analyseBlock(currStmt->getBody());
                break;
            }
            case NODE_DOWHILESTMT:
            {
                dowhileStmt *dowhilestmt = (dowhileStmt *)stmt;
                analyseBlock(dowhilestmt->getBody());
                break;
            }
            case NODE_WHILESTMT:
            {
                whileStmt *whilestmt = (whileStmt *)stmt;
                analyseBlock(whilestmt->getBody());
                break;
            }
            case NODE_FIXEDPTSTMT:
            {
                fixedPointStmt *fpStmt = (fixedPointStmt *)stmt;
                analyseBlock(fpStmt->getBody());
            }
        }
    }
}

void PRAnalyser::analyseFunc(ASTNode *proc)
{
    Function *func = (Function *)proc;
    analyseBlock(func->getBlockStatement());
    return;
}

void PRAnalyser::analyse()
{
    list<Function *> funcList = frontEndContext.getFuncList();
    for (Function *func : funcList)
        analyseFunc(func);
}