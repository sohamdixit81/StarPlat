#include <string.h>

#include <cassert>

#include "../../ast/ASTHelper.cpp"
#include "dsl_cpp_generator.h"

namespace spomp {

void dsl_cpp_generator::addIncludeToFile(const char* includeName, dslCodePad& file, bool isCppLib) {  //cout<<"ENTERED TO THIS ADD INCLUDE FILE"<<"\n";
  if (!isCppLib)
    file.push('"');
  else
    file.push('<');

  file.pushString(includeName);
  if (!isCppLib)
    file.push('"');
  else
    file.push('>');
  file.NewLine();
}

void dsl_cpp_generator::generation_begin() {
  char temp[1024];
  header.pushString("#ifndef GENCPP_");
  header.pushUpper(fileName);
  header.pushstr_newL("_H");
  header.pushString("#define GENCPP_");
  header.pushUpper(fileName);
  header.pushstr_newL("_H");
  header.pushString("#include");
  addIncludeToFile("stdio.h", header, true);
  header.pushString("#include");
  addIncludeToFile("stdlib.h", header, true);
  header.pushString("#include");
  addIncludeToFile("limits.h", header, true);
  header.pushString("#include");
  addIncludeToFile("atomic", header, true);
  header.pushString("#include");
  addIncludeToFile("omp.h", header, true);
  header.pushString("#include");
  addIncludeToFile("../graph.hpp", header, false);
  header.NewLine();
  main.pushString("#include");
  sprintf(temp, "%s.h", fileName);
  addIncludeToFile(temp, main, false);
  main.NewLine();
}

void add_InitialDeclarations(dslCodePad* main, iterateBFS* bfsAbstraction) {
  char strBuffer[1024];
  char* graphId = bfsAbstraction->getGraphCandidate()->getIdentifier();
  sprintf(strBuffer, "std::vector<std::vector<int>> %s(%s.%s()) ;", "levelNodes", graphId, "num_nodes");
  main->pushstr_newL(strBuffer);
  sprintf(strBuffer, "std::vector<std::vector<int>>  %s(%s()) ;", "levelNodes_later", "omp_get_max_threads");
  main->pushstr_newL(strBuffer);
  sprintf(strBuffer, "std::vector<int>  %s(%s.%s()) ;", "levelCount", graphId, "num_nodes");
  main->pushstr_newL(strBuffer);
  main->pushstr_newL("int phase = 0 ;");
  sprintf(strBuffer, "levelNodes[phase].push_back(%s) ;", bfsAbstraction->getRootNode()->getIdentifier());
  main->pushstr_newL(strBuffer);
  sprintf(strBuffer, "%s bfsCount = %s ;", "int", "1");
  main->pushstr_newL(strBuffer);
  main->pushstr_newL("levelCount[phase] = bfsCount;");
}

void add_BFSIterationLoop(dslCodePad* main, iterateBFS* bfsAbstraction) {
  char strBuffer[1024];
  char* iterNode = bfsAbstraction->getIteratorNode()->getIdentifier();
  char* graphId = bfsAbstraction->getGraphCandidate()->getIdentifier();
  main->pushstr_newL("while ( bfsCount > 0 )");
  main->pushstr_newL("{");
  main->pushstr_newL(" int prev_count = bfsCount ;");
  main->pushstr_newL("bfsCount = 0 ;");
  main->pushstr_newL("#pragma omp parallel for");
  sprintf(strBuffer, "for( %s %s = %s; %s < prev_count ; %s++)", "int", "l", "0", "l", "l");
  main->pushstr_newL(strBuffer);
  main->pushstr_newL("{");
  sprintf(strBuffer, "int %s = levelNodes[phase][%s] ;", iterNode, "l");
  main->pushstr_newL(strBuffer);
  sprintf(strBuffer, "for(%s %s = %s.%s[%s] ; %s < %s.%s[%s+1] ; %s++) ", "int", "edge", graphId, "indexofNodes", iterNode, "edge", graphId, "indexofNodes", iterNode, "edge");
  main->pushString(strBuffer);
  main->pushstr_newL("{");
  sprintf(strBuffer, "%s %s = %s.%s[%s] ;", "int", "nbr", graphId, "edgeList", "edge");
  main->pushstr_newL(strBuffer);
  main->pushstr_newL("int dnbr ;");
  main->pushstr_newL("if(bfsDist[nbr]<0)");
  main->pushstr_newL("{");
  sprintf(strBuffer, "dnbr = %s(&bfsDist[nbr],-1,bfsDist[%s]+1);", "__sync_val_compare_and_swap", iterNode);
  main->pushstr_newL(strBuffer);
  main->pushstr_newL("if (dnbr < 0)");
  main->pushstr_newL("{");
  sprintf(strBuffer, "%s %s = %s();", "int", "num_thread", "omp_get_thread_num");
  //sprintf(strBuffer,"int %s = bfsCount.fetch_add(%s,%s) ;","loc","1","std::memory_order_relaxed");
  main->pushstr_newL(strBuffer);
  sprintf(strBuffer, " levelNodes_later[%s].push_back(%s) ;", "num_thread", "nbr");
  main->pushstr_newL(strBuffer);
  main->pushstr_newL("}");
  main->pushstr_newL("}");
  main->pushstr_newL("}");
}

void add_RBFSIterationLoop(dslCodePad* main, iterateBFS* bfsAbstraction) {
  char strBuffer[1024];
  main->pushstr_newL("while (phase > 0)");
  main->pushstr_newL("{");
  main->pushstr_newL("#pragma omp parallel for");
  sprintf(strBuffer, "for( %s %s = %s; %s < levelCount[phase] ; %s++)", "int", "l", "0", "l", "l");
  main->pushstr_newL(strBuffer);
  main->pushstr_newL("{");
  sprintf(strBuffer, "int %s = levelNodes[phase][l] ;", bfsAbstraction->getIteratorNode()->getIdentifier());
  main->pushstr_newL(strBuffer);
}

void dsl_cpp_generator::generateBFSAbstraction(iterateBFS* bfsAbstraction) {
  char strBuffer[1024];
  add_InitialDeclarations(&main, bfsAbstraction);
  //printf("BFS ON GRAPH %s",bfsAbstraction->getGraphCandidate()->getIdentifier());
  add_BFSIterationLoop(&main, bfsAbstraction);
  statement* body = bfsAbstraction->getBody();
  assert(body->getTypeofNode() == NODE_BLOCKSTMT);
  blockStatement* block = (blockStatement*)body;
  list<statement*> statementList = block->returnStatements();
  for (statement* stmt : statementList) {
    generateStatement(stmt);
  }
  main.pushstr_newL("}");

  main.pushstr_newL("phase = phase + 1 ;");
  /* main.pushstr_newL("levelCount[phase] = bfsCount ;");
   main.pushstr_newL(" levelNodes[phase].assign(levelNodes_later.begin(),levelNodes_later.begin()+bfsCount);");*/
  sprintf(strBuffer, "for(int %s = 0;%s < %s();%s++)", "i", "i", "omp_get_max_threads", "i");
  main.pushstr_newL(strBuffer);
  main.pushstr_newL("{");
  sprintf(strBuffer, " levelNodes[phase].insert(levelNodes[phase].end(),levelNodes_later[%s].begin(),levelNodes_later[%s].end());", "i", "i");
  main.pushstr_newL(strBuffer);
  sprintf(strBuffer, " bfsCount=bfsCount+levelNodes_later[%s].size();", "i");
  main.pushstr_newL(strBuffer);
  sprintf(strBuffer, " levelNodes_later[%s].clear();", "i");
  main.pushstr_newL(strBuffer);
  main.pushstr_newL("}");
  main.pushstr_newL(" levelCount[phase] = bfsCount ;");
  main.pushstr_newL("}");
  main.pushstr_newL("phase = phase -1 ;");
  add_RBFSIterationLoop(&main, bfsAbstraction);
  blockStatement* revBlock = (blockStatement*)bfsAbstraction->getRBFS()->getBody();
  list<statement*> revStmtList = revBlock->returnStatements();

  for (statement* stmt : revStmtList) {
    generateStatement(stmt);
  }

  main.pushstr_newL("}");
  main.pushstr_newL("phase = phase - 1 ;");
  main.pushstr_newL("}");
}

void dsl_cpp_generator::generateWhileStmt(whileStmt* whilestmt) {
  Expression* conditionExpr = whilestmt->getCondition();
  main.pushString("while (");
  generateExpr(conditionExpr);
  main.pushString(" )");
  generateStatement(whilestmt->getBody());
}

void dsl_cpp_generator::generateStatement(statement* stmt) {
  if (stmt->getTypeofNode() == NODE_BLOCKSTMT) {
    generateBlock((blockStatement*)stmt);
  }
  if (stmt->getTypeofNode() == NODE_DECL) {
    generateVariableDecl((declaration*)stmt);
  }
  if (stmt->getTypeofNode() == NODE_ASSIGN) {
    generateAssignmentStmt((assignment*)stmt);
  }

  if (stmt->getTypeofNode() == NODE_WHILESTMT) {
    generateWhileStmt((whileStmt*)stmt);
  }

  if (stmt->getTypeofNode() == NODE_IFSTMT) {
    generateIfStmt((ifStmt*)stmt);
  }

  if (stmt->getTypeofNode() == NODE_DOWHILESTMT) {
    generateDoWhileStmt((dowhileStmt*)stmt);
  }

  if (stmt->getTypeofNode() == NODE_FORALLSTMT) {
    generateForAll((forallStmt*)stmt);
  }

  if (stmt->getTypeofNode() == NODE_FIXEDPTSTMT) {
    generateFixedPoint((fixedPointStmt*)stmt);
  }
  if (stmt->getTypeofNode() == NODE_REDUCTIONCALLSTMT) {
    generateReductionStmt((reductionCallStmt*)stmt);
  }
  if (stmt->getTypeofNode() == NODE_ITRBFS) {
    generateBFSAbstraction((iterateBFS*)stmt);
  }
  if (stmt->getTypeofNode() == NODE_PROCCALLSTMT) {
    generateProcCall((proc_callStmt*)stmt);
  }
  if (stmt->getTypeofNode() == NODE_UNARYSTMT) {
    unary_stmt* unaryStmt = (unary_stmt*)stmt;
    generateExpr(unaryStmt->getUnaryExpr());
    main.pushstr_newL(";");
  }
  if (stmt->getTypeofNode() == NODE_RETURN) {
    returnStmt* returnStmtNode = (returnStmt*)stmt;
    main.pushstr_space("return");
    generateExpr(returnStmtNode->getReturnExpression());
    main.pushstr_newL(";");
  }
}

void dsl_cpp_generator::generateReductionCallStmt(reductionCallStmt* stmt) {
  reductionCall* reduceCall = stmt->getReducCall();
  char strBuffer[1024];
  if (reduceCall->getReductionType() == REDUCE_MIN) {
    if (stmt->isListInvolved()) {
      //cout<<"INSIDE THIS OF LIST PRESENT"<<"\n";
      list<argument*> argList = reduceCall->getargList();
      list<ASTNode*> leftList = stmt->getLeftList();
      int i = 0;
      list<ASTNode*> rightList = stmt->getRightList();
      //printf("LEFT LIST SIZE %d \n",leftList.size());

      main.space();
      if (stmt->getAssignedId()->getSymbolInfo()->getType()->isPropType()) {
        Type* type = stmt->getAssignedId()->getSymbolInfo()->getType();

        main.pushstr_space(convertToCppType(type->getInnerTargetType()));
      }
      sprintf(strBuffer, "%s_new", stmt->getAssignedId()->getIdentifier());
      main.pushString(strBuffer);
      list<argument*>::iterator argItr;
      argItr = argList.begin();
      argItr++;
      main.pushString(" = ");
      generateExpr((*argItr)->getExpr());
      main.pushstr_newL(";");
      list<ASTNode*>::iterator itr1;
      list<ASTNode*>::iterator itr2;
      itr2 = rightList.begin();
      itr1 = leftList.begin();
      itr1++;
      for (; itr1 != leftList.end(); itr1++) {
        ASTNode* node = *itr1;
        ASTNode* node1 = *itr2;

        if (node->getTypeofNode() == NODE_ID) {
          main.pushstr_space(convertToCppType(((Identifier*)node)->getSymbolInfo()->getType()));
          sprintf(strBuffer, "%s_new", ((Identifier*)node)->getIdentifier());
          main.pushString(strBuffer);
          main.pushString(" = ");
          generateExpr((Expression*)node1);
        }
        if (node->getTypeofNode() == NODE_PROPACCESS) {
          PropAccess* p = (PropAccess*)node;
          Type* type = p->getIdentifier2()->getSymbolInfo()->getType();
          if (type->isPropType()) {
            main.pushstr_space(convertToCppType(type->getInnerTargetType()));
          }

          sprintf(strBuffer, "%s_new", p->getIdentifier2()->getIdentifier());
          main.pushString(strBuffer);
          main.pushString(" = ");
          Expression* expr = (Expression*)node1;
          generateExpr((Expression*)node1);
          main.pushstr_newL(";");
        }
        itr2++;
      }
      main.pushString("if(");
      generate_exprPropId(stmt->getTargetPropId());
      sprintf(strBuffer, " > %s_new)", stmt->getAssignedId()->getIdentifier());
      main.pushstr_newL(strBuffer);
      main.pushstr_newL("{");  //added for testing then doing atomic min.
      /* storing the old value before doing a atomic operation on the node property */
      if (stmt->isTargetId()) {
        Identifier* targetId = stmt->getTargetId();
        main.pushstr_space(convertToCppType(targetId->getSymbolInfo()->getType()));
        main.pushstr_space("oldValue");
        main.pushstr_space("=");
        generate_exprIdentifier(stmt->getTargetId());
        main.pushstr_newL(";");
      } else {
        PropAccess* targetProp = stmt->getTargetPropId();
        Type* type = targetProp->getIdentifier2()->getSymbolInfo()->getType();
        if (type->isPropType()) {
          main.pushstr_space(convertToCppType(type->getInnerTargetType()));
          main.pushstr_space("oldValue");
          main.pushstr_space("=");
          generate_exprPropId(stmt->getTargetPropId());
          main.pushstr_newL(";");
        }
      }

      Identifier* min_onId = stmt->getAssignedId();
      Type* min_onIdType;
      if (min_onId->getSymbolInfo()->getType()->isPropType()) {
        min_onIdType = min_onId->getSymbolInfo()->getType()->getInnerTargetType();
      } else
        min_onIdType = min_onId->getSymbolInfo()->getType();

      if (leftList.size() == 2 && (min_onIdType->gettypeId() == TYPE_INT ||
                                   min_onIdType->gettypeId() == TYPE_LONG ||
                                   min_onIdType->gettypeId() == TYPE_BOOL)) {
        main.pushString("atomicMin(&");
        generate_exprPropId(stmt->getTargetPropId());
        sprintf(strBuffer, ",%s_new);", stmt->getAssignedId()->getIdentifier());
        main.pushstr_newL(strBuffer);
      } else {
        sprintf(strBuffer, "omp_set_lock(&lock[%s]);", stmt->getTargetPropId()->getIdentifier1()->getIdentifier());
        main.pushstr_newL(strBuffer);
        main.pushString("if(");
        generate_exprPropId(stmt->getTargetPropId());
        sprintf(strBuffer, " > %s_new)", stmt->getAssignedId()->getIdentifier());
        main.pushstr_newL(strBuffer);
        main.pushstr_newL("{");

        itr1 = leftList.begin();
        itr1;
        for (; itr1 != leftList.end(); itr1++) {
          ASTNode* node = *itr1;
          Identifier* affected_Id = NULL;
          if (node->getTypeofNode() == NODE_ID) {
            generate_exprIdentifier((Identifier*)node);
          }
          if (node->getTypeofNode() == NODE_PROPACCESS) {
            affected_Id = ((PropAccess*)node)->getIdentifier2();
            if (!(affected_Id->getSymbolInfo()->getId()->get_fp_association()))
              generate_exprPropId((PropAccess*)node);
          }

          if (node->getTypeofNode() == NODE_ID) {
            main.space();
            main.pushstr_space("=");
            generate_exprIdentifier((Identifier*)node);
            main.pushString("_new");
            main.pushstr_newL(";");
          }
          if (node->getTypeofNode() == NODE_PROPACCESS) {
            if (!(affected_Id->getSymbolInfo()->getId()->get_fp_association())) {
              main.space();
              main.pushstr_space("=");
              generate_exprIdentifier(((PropAccess*)node)->getIdentifier2());
              main.pushString("_new");
              main.pushstr_newL(";");
            }
          }
        }

        main.pushstr_newL("}");
        sprintf(strBuffer, "omp_unset_lock(&lock[%s]);", stmt->getTargetPropId()->getIdentifier1()->getIdentifier());
        main.pushstr_newL(strBuffer);

        // main.pushstr_newL(strBuffer);
      }
      main.pushString("if( oldValue > ");
      generate_exprPropId(stmt->getTargetPropId());
      main.pushstr_newL(")");
      main.pushString("{");
      itr1 = leftList.begin();
      itr1++;
      for (; itr1 != leftList.end(); itr1++) {
        ASTNode* node = *itr1;
        Identifier* affected_Id = NULL;
        if (node->getTypeofNode() == NODE_ID) {
          affected_Id = (Identifier*)node;
          if (affected_Id->getSymbolInfo()->getId()->get_fp_association())
            generate_exprIdentifier((Identifier*)node);
        }
        if (node->getTypeofNode() == NODE_PROPACCESS) {
          affected_Id = ((PropAccess*)node)->getIdentifier2();
          if (affected_Id->getSymbolInfo()->getId()->get_fp_association())
            generate_exprPropId((PropAccess*)node);
        }

        if (node->getTypeofNode() == NODE_ID) {
          if (affected_Id->getSymbolInfo()->getId()->get_fp_association()) {
            main.space();
            main.pushstr_space("=");
            generate_exprIdentifier((Identifier*)node);
            main.pushString("_new");
            main.pushstr_newL(";");
          }
        }
        if (node->getTypeofNode() == NODE_PROPACCESS) {
          if (affected_Id->getSymbolInfo()->getId()->get_fp_association()) {
            main.space();
            main.pushstr_space("=");
            generate_exprIdentifier(((PropAccess*)node)->getIdentifier2());
            main.pushString("_new");
            main.pushstr_newL(";");
          }
        }

        if (node->getTypeofNode() == NODE_PROPACCESS && affected_Id->getSymbolInfo()->getId()->get_fp_association()) {
          generateFixedPointUpdate((PropAccess*)node);
        }

        /* if(affected_Id->getSymbolInfo()->getId()->get_fp_association())
                  {
                    char* fpId=affected_Id->getSymbolInfo()->getId()->get_fpId();
                    sprintf(strBuffer,"%s = %s ;",fpId,"false");
                    main.pushstr_newL(strBuffer);
                  } */
      }

      main.pushstr_newL("}");
      main.pushstr_newL("}");  //added for testing condition..then atomicmin.
    }
  }
}

void dsl_cpp_generator::generateReductionOpStmt(reductionCallStmt* stmt) {
  if (stmt->isLeftIdentifier()) {
    Identifier* id = stmt->getLeftId();
    main.pushString(id->getIdentifier());
    main.pushString(" = ");
    main.pushstr_space(id->getIdentifier());
    const char* operatorString = getOperatorString(stmt->reduction_op());
    main.pushstr_space(operatorString);
    generateExpr(stmt->getRightSide());
    main.pushstr_newL(";");

  } else {
    generate_exprPropId(stmt->getPropAccess());
    main.pushString(" = ");
    generate_exprPropId(stmt->getPropAccess());
    const char* operatorString = getOperatorString(stmt->reduction_op());
    main.pushstr_space(operatorString);
    generateExpr(stmt->getRightSide());
    main.pushstr_newL(";");
  }
}

void dsl_cpp_generator::generateReductionStmt(reductionCallStmt* stmt) {
  char strBuffer[1024];

  if (stmt->is_reducCall()) {
    generateReductionCallStmt(stmt);
  } else {
    generateReductionOpStmt(stmt);
  }
}

void dsl_cpp_generator::generateDoWhileStmt(dowhileStmt* doWhile) {
  main.pushstr_newL("do");
  generateStatement(doWhile->getBody());
  main.pushString("while(");
  generateExpr(doWhile->getCondition());
  main.pushString(");");
}

void dsl_cpp_generator::generateIfStmt(ifStmt* ifstmt) {
  cout << "INSIDE IF STMT"
       << "\n";
  Expression* condition = ifstmt->getCondition();
  main.pushString("if (");
  cout << "TYPE OF IFSTMT" << condition->getTypeofExpr() << "\n";
  generateExpr(condition);
  main.pushstr_newL(" )");
  main.space();
  main.space();
  generateStatement(ifstmt->getIfBody());
  if (ifstmt->getElseBody() == NULL)
    return;
  main.pushstr_newL("else");
  generateStatement(ifstmt->getElseBody());
}

void dsl_cpp_generator::findTargetGraph(vector<Identifier*> graphTypes, Type* type) {
  if (graphTypes.size() > 1) {
    cerr << "TargetGraph can't match";
  } else {
    Identifier* graphId = graphTypes[0];

    type->setTargetGraph(graphId);
  }
}

bool dsl_cpp_generator::checkFixedPointAssociation(PropAccess* propId) {
  char strBuffer[1024];
  Identifier* id2 = propId->getIdentifier2();
  Expression* fpExpr = id2->getSymbolInfo()->getId()->get_dependentExpr();
  Identifier* depPropId = NULL;
  if (fpExpr->getExpressionFamily() == EXPR_UNARY) {
    if (fpExpr->getUnaryExpr()->getExpressionFamily() == EXPR_ID) {
      depPropId = fpExpr->getUnaryExpr()->getId();
    }
  } else if (fpExpr->getExpressionFamily() == EXPR_ID) {
    depPropId = fpExpr->getId();
  }

  if (fixedPointEnv != NULL) {
    Expression* dependentPropExpr = fixedPointEnv->getDependentProp();
    Identifier* dependentPropId = NULL;
    if (dependentPropExpr->getExpressionFamily() == EXPR_UNARY) {
      if (dependentPropExpr->getUnaryExpr()->getExpressionFamily() == EXPR_ID) {
        dependentPropId = dependentPropExpr->getUnaryExpr()->getId();
      }
    } else if (dependentPropExpr->getExpressionFamily() == EXPR_ID) {
      dependentPropId = dependentPropExpr->getId();
    }

    string fpStmtProp(depPropId->getIdentifier());
    string dependentPropString(dependentPropId->getIdentifier());
    if (fpStmtProp == dependentPropString) {
      return true;
    }
  }

  return false;
}

void dsl_cpp_generator::generateFixedPointUpdate(PropAccess* propId) {
  char strBuffer[1024];
  Identifier* id2 = propId->getIdentifier2();
  if (checkFixedPointAssociation(propId)) {
    sprintf(strBuffer, "%s = %s ;", id2->getSymbolInfo()->getId()->get_fpId(), "false");
    main.pushstr_newL(strBuffer);
  }
}

void dsl_cpp_generator::generateAssignmentStmt(assignment* asmt) {
  char strBuffer[1024];
  Expression* exprAssigned = asmt->getExpr();
  vector<Identifier*> graphIds = graphId[curFuncType][curFuncCount()];

  if (asmt->lhs_isIdentifier()) {
    Identifier* id = asmt->getId();
    if (asmt->hasPropCopy())  // prop_copy is of the form (propId = propId)
    {
      char strBuffer[1024];
      Identifier* rhsPropId2 = exprAssigned->getId();
      main.pushstr_newL("#pragma omp parallel for");
      sprintf(strBuffer, "for (%s %s = 0; %s < %s.%s(); %s ++) ", "int", "node", "node", graphIds[0]->getIdentifier(), "num_nodes", "node");
      main.pushstr_newL(strBuffer);
      /* the graph associated                          */
      main.pushstr_newL("{");
      sprintf(strBuffer, "%s [%s] = %s [%s] ;", id->getIdentifier(), "node", rhsPropId2->getIdentifier(), "node");
      main.pushstr_newL(strBuffer);
      main.pushstr_newL("}");

    } else
      main.pushString(id->getIdentifier());
  } else if (asmt->lhs_isProp())  //the check for node and edge property to be carried out.
  {
    PropAccess* propId = asmt->getPropId();
    if (asmt->getAtomicSignal()) {
      /*if(asmt->getParent()->getParent()->getParent()->getParent()->getTypeofNode()==NODE_ITRBFS)
           if(asmt->getExpr()->isArithmetic()||asmt->getExpr()->isLogical())*/
      main.pushstr_newL("#pragma omp atomic");
    }

    generate_exprPropId(propId);
  }

  if (!asmt->hasPropCopy()) {
    main.pushString(" = ");
    generateExpr(asmt->getExpr());
    main.pushstr_newL(";");
    if (asmt->lhs_isProp()) {
      PropAccess* propId = asmt->getPropId();
      /* this is executed when there is an update on property associated with fixedpoint */
      Identifier* id2 = propId->getIdentifier2();
      if (id2->getSymbolInfo() != NULL && id2->getSymbolInfo()->getId()->get_fp_association()) {
        generateFixedPointUpdate(propId);
      }
    }
  }
}

void dsl_cpp_generator::generateProcCall(proc_callStmt* proc_callStmt) {  // cout<<"INSIDE PROCCALL OF GENERATION"<<"\n";
  proc_callExpr* procedure = proc_callStmt->getProcCallExpr();
  // cout<<"FUNCTION NAME"<<procedure->getMethodId()->getIdentifier();
  string methodID(procedure->getMethodId()->getIdentifier());
  string nodeCall("attachNodeProperty");
  string edgeCall("attachEdgeProperty");
  int compareVal = methodID.compare(nodeCall);
  int compareVal1 = methodID.compare(edgeCall);

  if (compareVal == 0) {
    // cout<<"MADE IT TILL HERE";
    char strBuffer[1024];
    list<argument*> argList = procedure->getArgList();
    list<argument*>::iterator itr;
    main.pushstr_newL("#pragma omp parallel for");
    sprintf(strBuffer, "for (%s %s = 0; %s < %s.%s(); %s ++) ", "int", "t", "t", procedure->getId1()->getIdentifier(), "num_nodes", "t");
    main.pushstr_newL(strBuffer);
    main.pushstr_newL("{");
    for (itr = argList.begin(); itr != argList.end(); itr++) {
      assignment* assign = (*itr)->getAssignExpr();
      Identifier* lhsId = assign->getId();
      Expression* exprAssigned = assign->getExpr();
      sprintf(strBuffer, "%s[%s] = ", lhsId->getIdentifier(), "t");
      main.pushString(strBuffer);
      generateExpr(exprAssigned);

      main.pushstr_newL(";");

      if (lhsId->getSymbolInfo()->getId()->require_redecl()) {
        sprintf(strBuffer, "%s_nxt[%s] = ", lhsId->getIdentifier(), "t");
        main.pushString(strBuffer);
        generateExpr(exprAssigned);
        main.pushstr_newL(";");
      }
    }

    main.pushstr_newL("}");

  } else if (compareVal1 == 0) {
    char strBuffer[1024];
    list<argument*> argList = procedure->getArgList();
    list<argument*>::iterator itr;
    main.pushstr_newL("#pragma omp parallel for");
    sprintf(strBuffer, "for (%s %s = 0; %s < %s.%s(); %s ++) ", "int", "t", "t", procedure->getId1()->getIdentifier(), "num_edges", "t");
    main.pushstr_newL(strBuffer);
    main.pushstr_newL("{");
    for (itr = argList.begin(); itr != argList.end(); itr++) {
      assignment* assign = (*itr)->getAssignExpr();
      Identifier* lhsId = assign->getId();
      Expression* exprAssigned = assign->getExpr();
      sprintf(strBuffer, "%s[%s] = ", lhsId->getIdentifier(), "t");
      main.pushString(strBuffer);
      generateExpr(exprAssigned);

      main.pushstr_newL(";");

      if (lhsId->getSymbolInfo()->getId()->require_redecl()) {
        sprintf(strBuffer, "%s_nxt[%s] = ", lhsId->getIdentifier(), "t");
        main.pushString(strBuffer);
        generateExpr(exprAssigned);
        main.pushstr_newL(";");
      }
    }

    main.pushstr_newL("}");

  } else {
    generate_exprProcCall(procedure);
    main.pushstr_newL(";");
    main.NewLine();
  }
}

void dsl_cpp_generator::generatePropertyDefination(Type* type, char* Id) {
  Type* targetType = type->getInnerTargetType();
  vector<Identifier*> graphIds = graphId[curFuncType][curFuncCount()];
  printf("currentFuncType %d\n", curFuncType);
  printf("currentFuncCount %d graphIds[0] %ld\n", curFuncCount(), graphIds.size());

  if (targetType->gettypeId() == TYPE_INT) {
    main.pushString("=");
    main.pushString(INTALLOCATION);
    main.pushString("[");
    // printf("%d SIZE OF VECTOR",)
    // findTargetGraph(graphId,type);

    if (graphIds.size() > 1) {
      cerr << "TargetGraph can't match";
    } else {
      Identifier* id = graphIds[0];

      type->setTargetGraph(id);
    }
    char strBuffer[100];
    if (type->gettypeId() == TYPE_PROPNODE)
      sprintf(strBuffer, "%s.%s()", type->getTargetGraph()->getIdentifier(), "num_nodes");
    else
      sprintf(strBuffer, "%s.%s()", type->getTargetGraph()->getIdentifier(), "num_edges");

    main.pushString(strBuffer);
    main.pushString("]");
    main.pushstr_newL(";");
  }
  if (targetType->gettypeId() == TYPE_BOOL) {
    main.pushString("=");
    main.pushString(BOOLALLOCATION);
    main.pushString("[");
    //findTargetGraph(graphId,type);
    if (graphIds.size() > 1) {
      cerr << "TargetGraph can't match";
    } else {
      Identifier* id = graphIds[0];
      printf("Inside Boolean Target \n");
      type->setTargetGraph(id);
    }
    char strBuffer[100];

    if (type->gettypeId() == TYPE_PROPNODE)
      sprintf(strBuffer, "%s.%s()", type->getTargetGraph()->getIdentifier(), "num_nodes");
    else
      sprintf(strBuffer, "%s.%s()", type->getTargetGraph()->getIdentifier(), "num_edges");

    main.pushString(strBuffer);
    main.pushString("]");
    main.pushstr_newL(";");
  }

  if (targetType->gettypeId() == TYPE_FLOAT) {
    main.pushString("=");
    main.pushString(FLOATALLOCATION);
    main.pushString("[");
    //findTargetGraph(graphId,type);
    if (graphIds.size() > 1) {
      cerr << "TargetGraph can't match";
    } else {
      Identifier* id = graphIds[0];

      type->setTargetGraph(id);
    }
    char strBuffer[100];
    if (type->gettypeId() == TYPE_PROPNODE)
      sprintf(strBuffer, "%s.%s()", type->getTargetGraph()->getIdentifier(), "num_nodes");
    else
      sprintf(strBuffer, "%s.%s()", type->getTargetGraph()->getIdentifier(), "num_edges");

    main.pushString(strBuffer);
    main.pushString("]");
    main.pushstr_newL(";");
  }

  if (targetType->gettypeId() == TYPE_DOUBLE) {
    main.pushString("=");
    main.pushString(DOUBLEALLOCATION);
    main.pushString("[");
    //findTargetGraph(graphId,type);
    if (graphIds.size() > 1) {
      cerr << "TargetGraph can't match";
    } else {
      Identifier* id = graphIds[0];

      type->setTargetGraph(id);
    }
    char strBuffer[100];
    if (TYPE_PROPNODE)
      sprintf(strBuffer, "%s.%s()", type->getTargetGraph()->getIdentifier(), "num_nodes");
    else
      sprintf(strBuffer, "%s.%s()", type->getTargetGraph()->getIdentifier(), "num_edges");

    main.pushString(strBuffer);
    main.pushString("]");
    main.pushstr_newL(";");
  }
}

void dsl_cpp_generator::getDefaultValueforTypes(int type) {
  switch (type) {
    case TYPE_INT:
    case TYPE_LONG:
      main.pushstr_space("0");
      break;
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
      main.pushstr_space("0.0");
      break;
    case TYPE_BOOL:
      main.pushstr_space("false");
    default:
      assert(false);
      return;
  }
}

void dsl_cpp_generator::generateForAll_header(forallStmt* forAll) {
  main.pushString("#pragma omp parallel for");  //This needs to be changed to checked for 'For' inside a parallel section.
  if (forAll->get_reduceKeys().size() > 0) {
    printf("INSIDE GENERATE FOR ALL FOR KEYS\n");

    set<int> reduce_Keys = forAll->get_reduceKeys();
    assert(reduce_Keys.size() == 1);
    char strBuffer[1024];
    set<int>::iterator it;
    it = reduce_Keys.begin();
    list<Identifier*> op_List = forAll->get_reduceIds(*it);
    list<Identifier*>::iterator list_itr;
    main.space();
    sprintf(strBuffer, "reduction(%s : ", getOperatorString(*it));
    main.pushString(strBuffer);
    for (list_itr = op_List.begin(); list_itr != op_List.end(); list_itr++) {
      Identifier* id = *list_itr;
      main.pushString(id->getIdentifier());
      if (std::next(list_itr) != op_List.end())
        main.pushString(",");
    }
    main.pushString(")");
  }
  main.NewLine();
}

bool dsl_cpp_generator::allGraphIteration(char* methodId) {
  string methodString(methodId);

  return (methodString == "nodes" || methodString == "edges");
}

bool dsl_cpp_generator::neighbourIteration(char* methodId) {
  string methodString(methodId);
  return (methodString == "neighbors" || methodString == "nodes_to");
}

bool dsl_cpp_generator::elementsIteration(char* extractId) {
  string extractString(extractId);
  return (extractString == "elements");
}

void dsl_cpp_generator::generateForAllSignature(forallStmt* forAll) {
  char strBuffer[1024];
  Identifier* iterator = forAll->getIterator();
  if (forAll->isSourceProcCall()) {
    Identifier* sourceGraph = forAll->getSourceGraph();
    proc_callExpr* extractElemFunc = forAll->getExtractElementFunc();
    Identifier* iteratorMethodId = extractElemFunc->getMethodId();
    if (allGraphIteration(iteratorMethodId->getIdentifier())) {
      char* graphId = sourceGraph->getIdentifier();
      char* methodId = iteratorMethodId->getIdentifier();
      string s(methodId);
      if (s.compare("nodes") == 0) {
        cout << "INSIDE NODES VALUE"
             << "\n";
        sprintf(strBuffer, "for (%s %s = 0; %s < %s.%s(); %s ++) ", "int", iterator->getIdentifier(), iterator->getIdentifier(), graphId, "num_nodes", iterator->getIdentifier());
      } else
        sprintf(strBuffer, "for (%s %s = 0; %s < %s.%s(); %s ++) ", "int", iterator->getIdentifier(), iterator->getIdentifier(), graphId, "num_edges", iterator->getIdentifier());

      main.pushstr_newL(strBuffer);

    } else if (neighbourIteration(iteratorMethodId->getIdentifier())) {
      char* graphId = sourceGraph->getIdentifier();
      char* methodId = iteratorMethodId->getIdentifier();
      string s(methodId);
      if (s.compare("neighbors") == 0) {
        list<argument*> argList = extractElemFunc->getArgList();
        assert(argList.size() == 1);
        Identifier* nodeNbr = argList.front()->getExpr()->getId();
        sprintf(strBuffer, "for (%s %s = %s.%s[%s]; %s < %s.%s[%s+1]; %s ++) ", "int", "edge", graphId, "indexofNodes", nodeNbr->getIdentifier(), "edge", graphId, "indexofNodes", nodeNbr->getIdentifier(), "edge");
        main.pushstr_newL(strBuffer);
        main.pushString("{");
        sprintf(strBuffer, "%s %s = %s.%s[%s] ;", "int", iterator->getIdentifier(), graphId, "edgeList", "edge");  //needs to move the addition of
        main.pushstr_newL(strBuffer);
      }
      if (s.compare("nodes_to") == 0) {
        list<argument*> argList = extractElemFunc->getArgList();
        assert(argList.size() == 1);
        Identifier* nodeNbr = argList.front()->getExpr()->getId();
        sprintf(strBuffer, "for (%s %s = %s.%s[%s]; %s < %s.%s[%s+1]; %s ++) ", "int", "edge", graphId, "rev_indexofNodes", nodeNbr->getIdentifier(), "edge", graphId, "rev_indexofNodes", nodeNbr->getIdentifier(), "edge");
        main.pushstr_newL(strBuffer);
        main.pushString("{");
        sprintf(strBuffer, "%s %s = %s.%s[%s] ;", "int", iterator->getIdentifier(), graphId, "srcList", "edge");  //needs to move the addition of
        main.pushstr_newL(strBuffer);
      }  //statement to a different method.
    }
  } else if (forAll->isSourceField()) {
    /*PropAccess* sourceField=forAll->getPropSource();
    Identifier* dsCandidate = sourceField->getIdentifier1();
    Identifier* extractId=sourceField->getIdentifier2();
    
    if(dsCandidate->getSymbolInfo()->getType()->gettypeId()==TYPE_SETN)
    {

      main.pushstr_newL("std::set<int>::iterator itr;");
      sprintf(strBuffer,"for(itr=%s.begin();itr!=%s.end();itr++)",dsCandidate->getIdentifier(),dsCandidate->getIdentifier());
      main.pushstr_newL(strBuffer);

    }
        
    
    if(elementsIteration(extractId->getIdentifier()))
      {
        Identifier* collectionName=forAll->getPropSource()->getIdentifier1();
        int typeId=collectionName->getSymbolInfo()->getType()->gettypeId();
        if(typeId==TYPE_SETN)
        {
          main.pushstr_newL("std::set<int>::iterator itr;");
          sprintf(strBuffer,"for(itr=%s.begin();itr!=%s.end();itr++)",collectionName->getIdentifier(),collectionName->getIdentifier());
          main.pushstr_newL(strBuffer);
        }

      }*/

  } else {
    Identifier* sourceId = forAll->getSource();
    if (sourceId != NULL) {
      if (sourceId->getSymbolInfo()->getType()->gettypeId() == TYPE_SETN) {
        main.pushstr_newL("std::set<int>::iterator itr;");
        sprintf(strBuffer, "for(itr=%s.begin();itr!=%s.end();itr++)", sourceId->getIdentifier(), sourceId->getIdentifier());
        main.pushstr_newL(strBuffer);
      }
    }
  }
}

blockStatement* dsl_cpp_generator::includeIfToBlock(forallStmt* forAll) {
  Expression* filterExpr = forAll->getfilterExpr();
  statement* body = forAll->getBody();
  Expression* modifiedFilterExpr = filterExpr;
  if (filterExpr->getExpressionFamily() == EXPR_RELATIONAL) {
    Expression* expr1 = filterExpr->getLeft();
    Expression* expr2 = filterExpr->getRight();
    if (expr1->isIdentifierExpr()) {
      /*if it is a nodeproperty, the filter is on the nodes that are iterated on
    One more check can be applied to check if the iterating type is a neigbor iteration
    or allgraph iterations.
   */
      if (expr1->getId()->getSymbolInfo() != NULL) {
        if (expr1->getId()->getSymbolInfo()->getType()->isPropNodeType()) {
          Identifier* iterNode = forAll->getIterator();
          Identifier* nodeProp = expr1->getId();
          PropAccess* propIdNode = (PropAccess*)Util::createPropIdNode(iterNode, nodeProp);
          Expression* propIdExpr = Expression::nodeForPropAccess(propIdNode);
          modifiedFilterExpr = (Expression*)Util::createNodeForRelationalExpr(propIdExpr, expr2, filterExpr->getOperatorType());
        }
      }
    }
    /* if(expr1->isPropIdExpr()&&expr2->isBooleanLiteral()) //specific to sssp. Need to revisit again to change it.
    {   PropAccess* propId=expr1->getPropId();
        bool value=expr2->getBooleanConstant();
        Expression* exprBool=(Expression*)Util::createNodeForBval(!value);
        assignment* assign=(assignment*)Util::createAssignmentNode(propId,exprBool);
        if(body->getTypeofNode()==NODE_BLOCKSTMT)
        {
          blockStatement* newbody=(blockStatement*)body;
          newbody->addToFront(assign);
          body=newbody;
        }

        modifiedFilterExpr = filterExpr;
    }
  */
  }
  ifStmt* ifNode = (ifStmt*)Util::createNodeForIfStmt(modifiedFilterExpr, forAll->getBody(), NULL);
  blockStatement* newBlock = new blockStatement();
  newBlock->setTypeofNode(NODE_BLOCKSTMT);
  newBlock->addStmtToBlock(ifNode);
  return newBlock;
}

void dsl_cpp_generator::checkAndGenerateFixedPtFilter(forallStmt* forAll) {
  Expression* filterExpr = NULL;
  if (forAll->hasFilterExpr())
    filterExpr = forAll->getfilterExpr();
  else
    filterExpr = forAll->getAssocExpr();
  if (fixedPointEnv != NULL)
    generatefixedpt_filter(filterExpr);
}

void dsl_cpp_generator::generateForAll(forallStmt* forAll) {
  proc_callExpr* extractElemFunc = forAll->getExtractElementFunc();
  PropAccess* sourceField = forAll->getPropSource();
  Identifier* sourceId = forAll->getSource();
  Identifier* collectionId;

  if (sourceField != NULL) {
    collectionId = sourceField->getIdentifier1();
  }
  if (sourceId != NULL) {
    collectionId = sourceId;
  }

  Identifier* iteratorMethodId;
  if (extractElemFunc != NULL)
    iteratorMethodId = extractElemFunc->getMethodId();
  statement* body = forAll->getBody();
  char strBuffer[1024];

  if (forAll->isForall()) {
    if (forAll->hasFilterExpr() || forAll->hasFilterExprAssoc()) {
      Expression* filterExpr = NULL;
      if (forAll->hasFilterExpr())
        filterExpr = forAll->getfilterExpr();
      else
        filterExpr = forAll->getAssocExpr();

      Expression* lhs = filterExpr->getLeft();
      Identifier* filterId = lhs->isIdentifierExpr() ? lhs->getId() : NULL;
      TableEntry* tableEntry = filterId != NULL ? filterId->getSymbolInfo() : NULL;
      if (tableEntry != NULL && tableEntry->getId()->get_fp_association()) {
        main.pushstr_newL("#pragma omp parallel");
        main.pushstr_newL("{");
        main.pushstr_newL("#pragma omp for");
      } else {
        generateForAll_header(forAll);
      }
    } else
      generateForAll_header(forAll);

    // parallelConstruct.push_back(forAll);
  }

  generateForAllSignature(forAll);

  if (forAll->hasFilterExpr()) {
    blockStatement* changedBody = includeIfToBlock(forAll);
    forAll->setBody(changedBody);
  }

  if (extractElemFunc != NULL) {
    forallStack.push_back(make_pair(forAll->getIterator(), forAll->getExtractElementFunc()));
    if (neighbourIteration(iteratorMethodId->getIdentifier())) {
      if (forAll->getParent()->getParent()->getTypeofNode() == NODE_ITRBFS) {
        list<argument*> argList = extractElemFunc->getArgList();
        assert(argList.size() == 1);
        Identifier* nodeNbr = argList.front()->getExpr()->getId();
        sprintf(strBuffer, "if(bfsDist[%s]==bfsDist[%s]+1)", forAll->getIterator()->getIdentifier(), nodeNbr->getIdentifier());
        main.pushstr_newL(strBuffer);
        main.pushstr_newL("{");
      }

      /* This can be merged with above condition through or operator but separating 
        both now, for any possible individual construct updation.*/

      if (forAll->getParent()->getParent()->getTypeofNode() == NODE_ITRRBFS) {
        char strBuffer[1024];
        list<argument*> argList = extractElemFunc->getArgList();
        assert(argList.size() == 1);
        Identifier* nodeNbr = argList.front()->getExpr()->getId();
        sprintf(strBuffer, "if(bfsDist[%s]==bfsDist[%s]+1)", forAll->getIterator()->getIdentifier(), nodeNbr->getIdentifier());
        main.pushstr_newL(strBuffer);
        main.pushstr_newL("{");
      }

      generateBlock((blockStatement*)forAll->getBody(), false);
      if (forAll->getParent()->getParent()->getTypeofNode() == NODE_ITRBFS || forAll->getParent()->getParent()->getTypeofNode() == NODE_ITRRBFS)
        main.pushstr_newL("}");
      main.pushstr_newL("}");
    } else {
      generateStatement(forAll->getBody());
    }

    if (forAll->isForall() && (forAll->hasFilterExpr() || forAll->hasFilterExprAssoc())) {
      checkAndGenerateFixedPtFilter(forAll);
    }

    forallStack.pop_back();

  } else {
    if (collectionId->getSymbolInfo()->getType()->gettypeId() == TYPE_SETN) {
      if (body->getTypeofNode() == NODE_BLOCKSTMT)
        main.pushstr_newL("{");
      sprintf(strBuffer, "int %s = *itr;", forAll->getIterator()->getIdentifier());
      main.pushstr_newL(strBuffer);
      if (body->getTypeofNode() == NODE_BLOCKSTMT) {
        generateBlock((blockStatement*)body, false);
        main.pushstr_newL("}");
      } else
        generateStatement(forAll->getBody());

    } else if (collectionId->getSymbolInfo()->getType()->gettypeId() == TYPE_UPDATES) {
      if (body->getTypeofNode() == NODE_BLOCKSTMT)
        main.pushstr_newL("{");
      sprintf(strBuffer, "update %s = %s[i];", forAll->getIterator()->getIdentifier(), collectionId->getIdentifier());
      main.pushstr_newL(strBuffer);
      if (body->getTypeofNode() == NODE_BLOCKSTMT) {
        generateBlock((blockStatement*)body, false);
        main.pushstr_newL("}");
      } else
        generateStatement(forAll->getBody());

    } else {
      cout << iteratorMethodId->getIdentifier() << "\n";
      generateStatement(forAll->getBody());
    }

    if (forAll->isForall() && (forAll->hasFilterExpr() || forAll->hasFilterExprAssoc())) {
      checkAndGenerateFixedPtFilter(forAll);
    }
  }
}

void dsl_cpp_generator::generatefixedpt_filter(Expression* filterExpr) {
  Expression* lhs = filterExpr->getLeft();
  char strBuffer[1024];

  vector<Identifier*> graphIds = graphId[curFuncType][curFuncCount()];
  /*printf("curFuncType %d curFuncCount() %d \n",curFuncType,curFuncCount());
  printf("size %d\n",graphIds.size());*/

  if (lhs->isIdentifierExpr()) {
    Identifier* filterId = lhs->getId();
    TableEntry* tableEntry = filterId->getSymbolInfo();
    if (tableEntry->getId()->get_fp_association()) {
      main.pushstr_newL("#pragma omp for");
      sprintf(strBuffer, "for (%s %s = 0; %s < %s.%s(); %s ++) ", "int", "v", "v", graphIds[0]->getIdentifier(), "num_nodes", "v");
      main.pushstr_newL(strBuffer);
      main.pushstr_space("{");
      sprintf(strBuffer, "%s[%s] =  %s_nxt[%s] ;", filterId->getIdentifier(), "v", filterId->getIdentifier(), "v");
      main.pushstr_newL(strBuffer);
      /*  Expression* initializer = filterId->getSymbolInfo()->getId()->get_assignedExpr();
            if(initializer!=NULL)
              assert(initializer->isBooleanLiteral());*/
      bool initializer = false;
      Expression* fixedPtDependentExpr = filterId->getSymbolInfo()->getId()->get_dependentExpr();
      if (fixedPtDependentExpr->isUnary())
        initializer = false;
      if (fixedPtDependentExpr->isIdentifierExpr())
        initializer = true;
      sprintf(strBuffer, "%s_nxt[%s] = %s ;", filterId->getIdentifier(), "v", initializer ? "true" : "false");
      main.pushstr_newL(strBuffer);
      main.pushstr_newL("}");
      main.pushstr_newL("}");
    }
  }
}

void dsl_cpp_generator::castIfRequired(Type* type, Identifier* methodID, dslCodePad& main) {
  /* This needs to be made generalized, extended for all predefined function,
      made available by the DSL*/
  string predefinedFunc("num_nodes");
  if (predefinedFunc.compare(methodID->getIdentifier()) == 0) {
    if (type->gettypeId() != TYPE_INT) {
      char strBuffer[1024];
      sprintf(strBuffer, "(%s)", convertToCppType(type));
      main.pushString(strBuffer);
    }
  }
}

void dsl_cpp_generator::generateVariableDecl(declaration* declStmt) {
  Type* type = declStmt->getType();
  char strBuffer[1024];

  if (type->isPropType()) {
    if (type->getInnerTargetType()->isPrimitiveType()) {
      Type* innerType = type->getInnerTargetType();
      main.pushString(convertToCppType(innerType));  //convertToCppType need to be modified.
      main.pushString("*");
      main.space();
      main.pushString(declStmt->getdeclId()->getIdentifier());
      generatePropertyDefination(type, declStmt->getdeclId()->getIdentifier());
      printf("added symbol %s\n", declStmt->getdeclId()->getIdentifier());
      printf("value requ %d\n", declStmt->getdeclId()->getSymbolInfo()->getId()->require_redecl());
      /* decl with variable name as var_nxt is required for double buffering
          ex :- In case of fixedpoint */
      if (declStmt->getdeclId()->getSymbolInfo()->getId()->require_redecl()) {
        main.pushString(convertToCppType(innerType));  //convertToCppType need to be modified.
        main.pushString("*");
        main.space();
        sprintf(strBuffer, "%s_nxt", declStmt->getdeclId()->getIdentifier());
        main.pushString(strBuffer);
        generatePropertyDefination(type, declStmt->getdeclId()->getIdentifier());
      }

      /*placeholder for adding code for declarations that are initialized as well*/
    }

    if (insideBatchBlock) /* the properties are malloced, so they need 
                                    to be freed to manage memory.*/
      freeIdStore.push_back(declStmt->getdeclId());

  } else if (type->isPrimitiveType()) {
    main.pushstr_space(convertToCppType(type));
    main.pushString(declStmt->getdeclId()->getIdentifier());
    if (declStmt->isInitialized()) {
      main.pushString(" = ");
      /* the following if conditions is for cases where the 
          predefined functions are used as initializers
          but the variable's type doesnot match*/
      if (declStmt->getExpressionAssigned()->getExpressionFamily() == EXPR_PROCCALL) {
        proc_callExpr* pExpr = (proc_callExpr*)declStmt->getExpressionAssigned();
        Identifier* methodId = pExpr->getMethodId();
        castIfRequired(type, methodId, main);
      }
      generateExpr(declStmt->getExpressionAssigned());
      main.pushstr_newL(";");
    } else {
      main.pushString(" = ");
      getDefaultValueforTypes(type->gettypeId());
      main.pushstr_newL(";");
    }

  } else if (type->isNodeEdgeType()) {
    main.pushstr_space(convertToCppType(type));
    main.pushString(declStmt->getdeclId()->getIdentifier());
    if (declStmt->isInitialized()) {
      main.pushString(" = ");
      generateExpr(declStmt->getExpressionAssigned());
      main.pushstr_newL(";");
    }

  } else if (type->isCollectionType()) {
    if (type->gettypeId() == TYPE_UPDATES) {
      main.pushstr_space(convertToCppType(type));
      main.pushString(declStmt->getdeclId()->getIdentifier());
      if (declStmt->isInitialized()) {
        main.pushString(" = ");
        generateExpr(declStmt->getExpressionAssigned());
        main.pushstr_newL(";");
      }

      if (insideBatchBlock)
        freeIdStore.push_back(declStmt->getdeclId());
    }
  }
}

void dsl_cpp_generator::generate_exprLiteral(Expression* expr) {
  char valBuffer[1024];

  int expr_valType = expr->getExpressionFamily();

  switch (expr_valType) {
    case EXPR_INTCONSTANT:
      sprintf(valBuffer, "%ld", expr->getIntegerConstant());
      break;

    case EXPR_FLOATCONSTANT:
      sprintf(valBuffer, "%lf", expr->getFloatConstant());
      break;
    case EXPR_BOOLCONSTANT:
      sprintf(valBuffer, "%s", expr->getBooleanConstant() ? "true" : "false");
      break;
    default:
      assert(false);

      // return valBuffer;
  }

  // printf("VALBUFFER %s",valBuffer);
  main.pushString(valBuffer);
}

void dsl_cpp_generator::generate_exprInfinity(Expression* expr) {
  char valBuffer[1024];
  if (expr->getTypeofExpr() != 0) {
    int typeClass = expr->getTypeofExpr();
    switch (typeClass) {
      case TYPE_INT:
        sprintf(valBuffer, "%s", expr->isPositiveInfinity() ? "INT_MAX" : "INT_MIN");
        break;
      case TYPE_LONG:
        sprintf(valBuffer, "%s", expr->isPositiveInfinity() ? "LLONG_MAX" : "LLONG_MIN");
        break;
      case TYPE_FLOAT:
        sprintf(valBuffer, "%s", expr->isPositiveInfinity() ? "FLT_MAX" : "FLT_MIN");
        break;
      case TYPE_DOUBLE:
        sprintf(valBuffer, "%s", expr->isPositiveInfinity() ? "DBL_MAX" : "DBL_MIN");
        break;
      default:
        sprintf(valBuffer, "%s", expr->isPositiveInfinity() ? "INT_MAX" : "INT_MIN");
        break;
    }

  } else

  {
    sprintf(valBuffer, "%s", expr->isPositiveInfinity() ? "INT_MAX" : "INT_MIN");
  }

  main.pushString(valBuffer);
}

const char* dsl_cpp_generator::getOperatorString(int operatorId) {
  switch (operatorId) {
    case OPERATOR_ADD:
      return "+";
    case OPERATOR_DIV:
      return "/";
    case OPERATOR_MUL:
      return "*";
    case OPERATOR_MOD:
      return "%";
    case OPERATOR_SUB:
      return "-";
    case OPERATOR_EQ:
      return "==";
    case OPERATOR_NE:
      return "!=";
    case OPERATOR_LT:
      return "<";
    case OPERATOR_LE:
      return "<=";
    case OPERATOR_GT:
      return ">";
    case OPERATOR_GE:
      return ">=";
    case OPERATOR_AND:
      return "&&";
    case OPERATOR_OR:
      return "||";
    case OPERATOR_INC:
      return "++";
    case OPERATOR_DEC:
      return "--";
    case OPERATOR_ADDASSIGN:
      return "+";
    case OPERATOR_ANDASSIGN:
      return "&&";
    case OPERATOR_ORASSIGN:
      return "||";
    case OPERATOR_MULASSIGN:
      return "*";
    case OPERATOR_SUBASSIGN:
      return "-";
    case OPERATOR_NOT:
      return "!";
    default:
      return "NA";
  }
}

void dsl_cpp_generator::generate_exprRelational(Expression* expr) {
  if (expr->hasEnclosedBrackets()) {
    main.pushString("(");
  }
  generateExpr(expr->getLeft());

  main.space();
  const char* operatorString = getOperatorString(expr->getOperatorType());
  main.pushstr_space(operatorString);
  generateExpr(expr->getRight());
  if (expr->hasEnclosedBrackets()) {
    main.pushString(")");
  }
}

void dsl_cpp_generator::generate_exprIdentifier(Identifier* id) {
  main.pushString(id->getIdentifier());
}

void dsl_cpp_generator::generateExpr(Expression* expr) {
  if (expr->isLiteral()) {
    // cout<<"INSIDE THIS FOR LITERAL"<<"\n";
    generate_exprLiteral(expr);
  } else if (expr->isInfinity()) {
    generate_exprInfinity(expr);
  }

  else if (expr->isIdentifierExpr()) {
    generate_exprIdentifier(expr->getId());
  } else if (expr->isPropIdExpr()) {
    generate_exprPropId(expr->getPropId());
  } else if (expr->isArithmetic() || expr->isLogical()) {
    generate_exprArL(expr);
  } else if (expr->isRelational()) {
    generate_exprRelational(expr);
  } else if (expr->isProcCallExpr()) {
    generate_exprProcCall(expr);
  } else if (expr->isUnary()) {
    generate_exprUnary(expr);
  } else {
    assert(false);
  }
}

void dsl_cpp_generator::generate_exprArL(Expression* expr) {
  if (expr->hasEnclosedBrackets()) {
    main.pushString("(");
  }
  generateExpr(expr->getLeft());

  main.space();
  const char* operatorString = getOperatorString(expr->getOperatorType());
  main.pushstr_space(operatorString);
  generateExpr(expr->getRight());
  if (expr->hasEnclosedBrackets()) {
    main.pushString(")");
  }
}

void dsl_cpp_generator::generate_exprUnary(Expression* expr) {
  if (expr->hasEnclosedBrackets()) {
    main.pushString("(");
  }

  if (expr->getOperatorType() == OPERATOR_NOT) {
    const char* operatorString = getOperatorString(expr->getOperatorType());
    main.pushString(operatorString);
    generateExpr(expr->getUnaryExpr());
  }

  if (expr->getOperatorType() == OPERATOR_INC || expr->getOperatorType() == OPERATOR_DEC) {
    generateExpr(expr->getUnaryExpr());
    const char* operatorString = getOperatorString(expr->getOperatorType());
    main.pushString(operatorString);
  }

  if (expr->hasEnclosedBrackets()) {
    main.pushString(")");
  }
}

/* edge datatype translation that needs to be followed for dynamic algos */
void dsl_cpp_generator::getEdgeTranslation(Expression* expr) {
  char strBuffer[1024];
  int sizeofStack = forallStack.size();
  int count = sizeofStack - 1;
  proc_callExpr* proc = (proc_callExpr*)expr;
  list<argument*> argList = proc->getArgList();
  assert(argList.size() == 2);
  Identifier* srcId = argList.front()->getExpr()->getId();
  Identifier* destId = argList.back()->getExpr()->getId();
  string methodId(proc->getMethodId()->getIdentifier());
  // printf("method id %s \n",proc->getMethodId()->getIdentifier());
  while (count >= 0) {
    std::pair<Identifier*, proc_callExpr*> element = forallStack[count];
    Identifier* iterator = element.first;
    proc_callExpr* elementFunc = element.second;

    // methodString=="neighbors"||methodString=="nodes_to"
    if (neighbourIteration(elementFunc->getMethodId()->getIdentifier())) {
      //printf("Inside edge Translation check !!\n");
      Identifier* sourceNode = elementFunc->getArgList().front()->getExpr()->getId();
      string sourceNodeId(sourceNode->getIdentifier());
      string edgesrcId(srcId->getIdentifier());
      string elementFuncId(elementFunc->getMethodId()->getIdentifier());
      if (sourceNodeId == edgesrcId) {
        if (elementFuncId == "neighbors") {
          if (curFuncType == INCREMENTAL_FUNC || curFuncType == DECREMENTAL_FUNC || curFuncType == DYNAMIC_FUNC)
            sprintf(strBuffer, "%s_edge", sourceNode->getIdentifier());
          else
            sprintf(strBuffer, "edge");
        } else {
          if (curFuncType == INCREMENTAL_FUNC || curFuncType == DECREMENTAL_FUNC || curFuncType == DYNAMIC_FUNC)
            sprintf(strBuffer, "%s_inedge", sourceNode->getIdentifier());
          else
            sprintf(strBuffer, "edge");
        }
        main.pushString(strBuffer);
        break;
      }
    }
    count--;
  }

  if (count < 0) {
    vector<Identifier*> graphVar = graphId[curFuncType][curFuncCount()];
    if (curFuncType == INCREMENTAL_FUNC || curFuncType == DECREMENTAL_FUNC || curFuncType == DYNAMIC_FUNC) {
      sprintf(strBuffer, "%s.getEdge(%s,%s)", graphVar[0]->getIdentifier(), srcId->getIdentifier(), destId->getIdentifier());
      main.pushString(strBuffer);
    } else {
      sprintf(strBuffer, "%s.getEdge(%s,%s).id", graphVar[0]->getIdentifier(), srcId->getIdentifier(), destId->getIdentifier());
      main.pushString(strBuffer);
    }
  }
}

void dsl_cpp_generator::generateArgList(list<argument*> argList) {
  char strBuffer[1024];
  main.pushString("(");
  int argListSize = argList.size();
  int commaCounts = 0;
  list<argument*>::iterator itr;
  for (itr = argList.begin(); itr != argList.end(); itr++) {
    commaCounts++;
    argument* arg = *itr;
    Identifier* id = arg->getExpr()->getId();
    sprintf(strBuffer, "%s", id->getIdentifier());
    main.pushString(strBuffer);

    if (commaCounts < argListSize)
      main.pushString(",");
  }

  main.pushString(")");
}

void dsl_cpp_generator::generate_exprProcCall(Expression* expr) {
  proc_callExpr* proc = (proc_callExpr*)expr;
  string methodId(proc->getMethodId()->getIdentifier());
  if (methodId == "get_edge") {
    // if(curFuncType == INCREMENTAL_FUNC || curFuncType == DECREMENTAL_FUNC)
    getEdgeTranslation(expr);
    /*else    
     main.pushString("edge");*/
    //To be changed..need to check for a neighbour iteration
    // and then replace by the iterator.
  } else if (methodId == "count_outNbrs") {
    char strBuffer[1024];
    list<argument*> argList = proc->getArgList();
    assert(argList.size() == 1);
    Identifier* nodeId = argList.front()->getExpr()->getId();
    Identifier* objectId = proc->getId1();
    sprintf(strBuffer, "(%s.%s[%s+1]-%s.%s[%s])", objectId->getIdentifier(), "indexofNodes", nodeId->getIdentifier(), objectId->getIdentifier(), "indexofNodes", nodeId->getIdentifier());
    main.pushString(strBuffer);
  } else if (methodId == "is_an_edge") {
    char strBuffer[1024];
    list<argument*> argList = proc->getArgList();
    assert(argList.size() == 2);
    Identifier* srcId = argList.front()->getExpr()->getId();
    Identifier* destId = argList.back()->getExpr()->getId();
    Identifier* objectId = proc->getId1();
    sprintf(strBuffer, "%s.%s(%s, %s)", objectId->getIdentifier(), "check_if_nbr", srcId->getIdentifier(), destId->getIdentifier());
    main.pushString(strBuffer);

  } else {
    char strBuffer[1024];
    list<argument*> argList = proc->getArgList();
    if (argList.size() == 0) {
      Identifier* objectId = proc->getId1();
      sprintf(strBuffer, "%s.%s( )", objectId->getIdentifier(), proc->getMethodId()->getIdentifier());
      main.pushString(strBuffer);
    }
  }
}

void dsl_cpp_generator::generate_exprPropId(PropAccess* propId)  //This needs to be made more generic.
{
  char strBuffer[1024];
  Identifier* id1 = propId->getIdentifier1();
  Identifier* id2 = propId->getIdentifier2();
  ASTNode* propParent = propId->getParent();

  if (id2->getSymbolInfo() != NULL && id2->getSymbolInfo()->getId()->get_fp_association()) {
    bool relatedToUpdation = propParent != NULL ? ((propParent->getTypeofNode() == NODE_REDUCTIONCALLSTMT || propParent->getTypeofNode() == NODE_ASSIGN) && checkFixedPointAssociation(propId)) : false;
    if (relatedToUpdation) {
      sprintf(strBuffer, "%s_nxt[%s]", id2->getIdentifier(), id1->getIdentifier());
      // printf("Inside this checked !\n");
    } else {
      sprintf(strBuffer, "%s[%s]", id2->getIdentifier(), id1->getIdentifier());
    }

  } else {
    sprintf(strBuffer, "%s[%s]", id2->getIdentifier(), id1->getIdentifier());
  }
  main.pushString(strBuffer);
}

void dsl_cpp_generator::generateFixedPoint(fixedPointStmt* fixedPointConstruct) {
  char strBuffer[1024];
  Expression* convergeExpr = fixedPointConstruct->getDependentProp();
  Identifier* fixedPointId = fixedPointConstruct->getFixedPointId();
  statement* blockStmt = fixedPointConstruct->getBody();
  fixedPointEnv = fixedPointConstruct;
  assert(convergeExpr->getExpressionFamily() == EXPR_UNARY || convergeExpr->getExpressionFamily() == EXPR_ID);
  main.pushString("while ( ");
  main.push('!');
  main.pushString(fixedPointId->getIdentifier());
  main.pushstr_newL(" )");
  main.pushstr_newL("{");
  sprintf(strBuffer, "%s = %s;", fixedPointId->getIdentifier(), "true");
  main.pushstr_newL(strBuffer);
  if (fixedPointConstruct->getBody()->getTypeofNode() != NODE_BLOCKSTMT)
    generateStatement(fixedPointConstruct->getBody());
  else
    generateBlock((blockStatement*)fixedPointConstruct->getBody(), false);
  Identifier* dependentId = NULL;
  bool isNot = false;
  assert(convergeExpr->getExpressionFamily() == EXPR_UNARY || convergeExpr->getExpressionFamily() == EXPR_ID);
  if (convergeExpr->getExpressionFamily() == EXPR_UNARY) {
    if (convergeExpr->getUnaryExpr()->getExpressionFamily() == EXPR_ID) {
      dependentId = convergeExpr->getUnaryExpr()->getId();
      isNot = true;
    }
  }
  if (convergeExpr->getExpressionFamily() == EXPR_ID)
    dependentId = convergeExpr->getId();
  /* if(dependentId!=NULL)
     {
       if(dependentId->getSymbolInfo()->getType()->isPropType())
       {   
       
         if(dependentId->getSymbolInfo()->getType()->isPropNodeType())
         {  Type* type=dependentId->getSymbolInfo()->getType();
              main.pushstr_newL("#pragma omp parallel for");
            if(graphId.size()>1)
             {
               cerr<<"GRAPH AMBIGUILTY";
             }
             else
            sprintf(strBuffer,"for (%s %s = 0; %s < %s.%s(); %s ++) ","int","v","v",graphId[0]->getIdentifier(),"num_nodes","v");
            main.pushstr_newL(strBuffer);
            main.pushstr_space("{");
            sprintf(strBuffer,"%s[%s] =  %s_nxt[%s] ;",dependentId->getIdentifier(),"v",dependentId->getIdentifier(),"v");
            main.pushstr_newL(strBuffer);
            Expression* initializer = dependentId->getSymbolInfo()->getId()->get_assignedExpr();
            assert(initializer->isBooleanLiteral());
            sprintf(strBuffer,"%s_nxt[%s] = %s ;",dependentId->getIdentifier(),"v",initializer->getBooleanConstant()?"true":"false");
            main.pushstr_newL(strBuffer);
            main.pushstr_newL("}");
           if(isNot)------chopped out.
           {
            sprintf(strBuffer,"%s = !%s_fp ;",fixedPointId->getIdentifier(),dependentId->getIdentifier());
            main.pushstr_newL(strBuffer);
             }
             else
             {
               sprintf(strBuffer,"%s = %s_fp ;",fixedPointId->getIdentifier(),dependentId->getIdentifier());
               main.pushString(strBuffer);
             }--------chopped out.
           
        }
      }
     }*/
  main.pushstr_newL("}");
  fixedPointEnv = NULL;
}

void dsl_cpp_generator::generateBlock(blockStatement* blockStmt, bool includeBrace) {
  list<statement*> stmtList = blockStmt->returnStatements();
  list<statement*>::iterator itr;
  if (includeBrace) {
    main.pushstr_newL("{");
  }
  for (itr = stmtList.begin(); itr != stmtList.end(); itr++) {
    statement* stmt = *itr;

    generateStatement(stmt);
  }
  if (includeBrace) {
    main.pushstr_newL("}");
  }
}

int dsl_cpp_generator::curFuncCount() {
  int count;
  if (curFuncType == GEN_FUNC)
    count = genFuncCount;

  else if (curFuncType == STATIC_FUNC)
    count = staticFuncCount;

  else if (curFuncType == INCREMENTAL_FUNC)
    count = inFuncCount;

  else if (curFuncType == DECREMENTAL_FUNC)
    count = decFuncCount;

  else if (curFuncType == DYNAMIC_FUNC)
    count = dynFuncCount;

  return count;
}

void dsl_cpp_generator::incFuncCount(int funcType) {
  if (funcType == GEN_FUNC)
    genFuncCount++;
  else if (funcType == STATIC_FUNC)
    staticFuncCount++;
  else if (funcType == INCREMENTAL_FUNC)
    inFuncCount++;
  else if (funcType == DECREMENTAL_FUNC)
    decFuncCount++;
  else
    dynFuncCount++;
}

void dsl_cpp_generator::setCurrentFunc(Function* func) {
  currentFunc = func;
}

Function* dsl_cpp_generator::getCurrentFunc() {
  return currentFunc;
}

void dsl_cpp_generator::generateFunc(ASTNode* proc) {
  char strBuffer[1024];
  Function* func = (Function*)proc;
  generateFuncHeader(func, false);
  generateFuncHeader(func, true);
  curFuncType = func->getFuncType();
  currentFunc = func;
  std::cout << "############3Current Func count : " << curFuncCount() << std::endl;
  //including locks before hand in the body of function.
  main.pushstr_newL("{");
  if (func->getInitialLockDecl()) {
    vector<Identifier*> graphVar = graphId[curFuncType][curFuncCount()];
    sprintf(strBuffer, "omp_lock_t* lock = (omp_lock_t*)malloc(%s.num_nodes()*sizeof(omp_lock_t));", graphVar[0]->getIdentifier());
    main.pushstr_newL(strBuffer);
    main.NewLine();
    sprintf(strBuffer, "for(%s %s = %s; %s<%s.%s(); %s++)", "int", "v", "0", "v", graphVar[0]->getIdentifier(), "num_nodes", "v");
    main.pushstr_newL(strBuffer);
    sprintf(strBuffer, "omp_init_lock(&lock[%s]);", "v");
    main.space();
    main.space();
    main.pushstr_newL(strBuffer);
    main.NewLine();
  }
  /* Locks declaration and initialization */
  /* sprintf(strBuffer,"const int %s=%s.%s();","node_count",graphId[0]->getIdentifier(),"num_nodes");
   main.pushstr_newL(strBuffer);
   sprintf(strBuffer,"omp_lock_t lock[%s+1];","node_count");
   main.pushstr_newL(strBuffer);
   generateForAll_header();
   sprintf(strBuffer,"for(%s %s = %s; %s<%s.%s(); %s++)","int","v","0","v",graphId[0]->getIdentifier(),"num_nodes","v");
   main.pushstr_newL(strBuffer);
   sprintf(strBuffer,"omp_init_lock(&lock[%s]);","v");
   main.pushstr_newL(strBuffer);*/
  //including locks before hand in the body of function.
  generateBlock(func->getBlockStatement(), false);
  main.NewLine();
  main.pushstr_newL("}");

  incFuncCount(func->getFuncType());

  return;
}

const char* dsl_cpp_generator::convertToCppType(Type* type) {
  if (type->isPrimitiveType()) {
    int typeId = type->gettypeId();
    switch (typeId) {
      case TYPE_INT:
        return "int";
      case TYPE_BOOL:
        return "bool";
      case TYPE_LONG:
        return "long";
      case TYPE_FLOAT:
        return "float";
      case TYPE_DOUBLE:
        return "double";
      case TYPE_NODE:
        return "int";
      case TYPE_EDGE:
        return "int";
      default:
        assert(false);
    }
  } else if (type->isPropType()) {
    Type* targetType = type->getInnerTargetType();
    if (targetType->isPrimitiveType()) {
      int typeId = targetType->gettypeId();
      cout << "TYPEID IN CPP" << typeId << "\n";
      switch (typeId) {
        case TYPE_INT:
          return "int*";
        case TYPE_BOOL:
          return "bool*";
        case TYPE_LONG:
          return "long*";
        case TYPE_FLOAT:
          return "float*";
        case TYPE_DOUBLE:
          return "double*";
        default:
          assert(false);
      }
    }
  } else if (type->isNodeEdgeType()) {
    if (type->isEdgeType() && (curFuncType == INCREMENTAL_FUNC || curFuncType == DECREMENTAL_FUNC || curFuncType == DYNAMIC_FUNC))
      return "edge";
    else
      return "int";  //need to be modified.

  } else if (type->isGraphType()) {
    return "graph&";
  } else if (type->isCollectionType()) {
    printf("Collection Type \n");
    int typeId = type->gettypeId();

    switch (typeId) {
      case TYPE_SETN:
        return "std::set<int>&";
      case TYPE_UPDATES:
        return "std::vector<update>&";
      default:
        assert(false);
    }
  }

  return "NA";
}

void dsl_cpp_generator::generateParamList(list<formalParam*> paramList, dslCodePad& targetFile) {
  int maximum_arginline = 4;
  int arg_currNo = 0;
  int argumentTotal = paramList.size();
  list<formalParam*>::iterator itr;
  for (itr = paramList.begin(); itr != paramList.end(); itr++) {
    arg_currNo++;
    argumentTotal--;

    Type* type = (*itr)->getType();
    targetFile.pushString(convertToCppType(type));
    /*if(type->isPropType())
      {
          targetFile.pushString("* ");
      }
      else 
      {*/
    targetFile.pushString(" ");
    // targetFile.space();
    //}
    targetFile.pushString(/*createParamName(*/ (*itr)->getIdentifier()->getIdentifier());
    if (argumentTotal > 0)
      targetFile.pushString(",");

    if (arg_currNo == maximum_arginline) {
      targetFile.NewLine();
      arg_currNo = 0;
    }
    // if(argumentTotal==0)
  }
}

void dsl_cpp_generator::generateFuncHeader(Function* proc, bool isMainFile) {
  dslCodePad& targetFile = isMainFile ? main : header;

  if (proc->containsReturn())
    targetFile.pushString("auto ");
  else
    targetFile.pushString("void ");

  targetFile.pushString(proc->getIdentifier()->getIdentifier());
  targetFile.push('(');

  generateParamList(proc->getParamList(), targetFile);

  /* int maximum_arginline=4;
  int arg_currNo=0;
  int argumentTotal=proc->getParamList().size();
  list<formalParam*> paramList=proc->getParamList();
  list<formalParam*>::iterator itr;
  for(itr=paramList.begin();itr!=paramList.end();itr++)
  {
      arg_currNo++;
      argumentTotal--;

      Type* type=(*itr)->getType();
      targetFile.pushString(convertToCppType(type));
      /*if(type->isPropType())
      {
          targetFile.pushString("* ");
      }
      else 
      {*/
  // targetFile.pushString(" ");
  // targetFile.space();
  //}
  /*  targetFile.pushString( /*createParamName(*/ /*(*itr)->getIdentifier()->getIdentifier());
      if(argumentTotal>0)
         targetFile.pushString(",");

      if(arg_currNo==maximum_arginline)
      {
         targetFile.NewLine();  
         arg_currNo=0;  
      } 
     // if(argumentTotal==0)
         
  }*/

  targetFile.pushString(")");
  if (!isMainFile)
    targetFile.pushString(";");
  targetFile.NewLine();

  return;
}

bool dsl_cpp_generator::openFileforOutput() {
  char temp[1024];
  printf("fileName %s\n", fileName);
  sprintf(temp, "%s/%s.h", "../graphcode/generated_omp", fileName);
  headerFile = fopen(temp, "w");
  if (headerFile == NULL)
    return false;
  header.setOutputFile(headerFile);

  sprintf(temp, "%s/%s.cc", "../graphcode/generated_omp", fileName);
  bodyFile = fopen(temp, "w");
  if (bodyFile == NULL)
    return false;
  main.setOutputFile(bodyFile);

  return true;
}
void dsl_cpp_generator::generation_end() {
  header.NewLine();
  header.pushstr_newL("#endif");
}

void dsl_cpp_generator::closeOutputFile() {
  if (headerFile != NULL) {
    header.outputToFile();
    fclose(headerFile);
  }
  headerFile = NULL;

  if (bodyFile != NULL) {
    main.outputToFile();
    fclose(bodyFile);
  }

  bodyFile = NULL;
}

bool dsl_cpp_generator::generate() {
  cout<<"FRONTEND VALUES "<<frontEndContext.getFuncList().front()->getBlockStatement()->returnStatements().size();    //openFileforOutput();
  if (!openFileforOutput())
    return false;
  generation_begin();

  list<Function*> funcList = frontEndContext.getFuncList();
  std::cout<< "Funclist siz" << funcList.size() << '\n';
  for (Function* func : funcList) {
    generateFunc(func);
    std::cout<< "in FUN" << '\n';
  }

  generation_end();

  closeOutputFile();

  return true;
}

void dsl_cpp_generator::setFileName(char* f)  // to be changed to make it more universal.
{
  //printf("%s \n", f);
  char* token = strtok(f, "/");
  char* prevtoken;

  while (token != NULL) {
    prevtoken = token;
    token = strtok(NULL, "/");
  }
  fileName = prevtoken;
  printf("OutFile: %s \n", fileName);
}

}  // namespace spomp
