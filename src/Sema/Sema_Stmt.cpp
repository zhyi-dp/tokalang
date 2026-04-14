// Copyright (c) 2025 YiZhonghua<zhyi@dpai.com>. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "toka/AST.h"
#include "toka/DiagnosticEngine.h"
#include "toka/Sema.h"
#include "toka/Type.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <set>

namespace toka {

static SourceLocation getLoc(ASTNode *Node) { return Node->Loc; }

bool Sema::allPathsReturn(Stmt *S) {
  if (!S)
    return false;
  if (dynamic_cast<ReturnStmt *>(S) || dynamic_cast<UnreachableStmt *>(S))
    return true;
  if (auto *B = dynamic_cast<BlockStmt *>(S)) {
    for (const auto &Sub : B->Statements) {
      if (allPathsReturn(Sub.get()))
        return true;
    }
    return false;
  }
  if (auto *Unsafe = dynamic_cast<UnsafeStmt *>(S)) {
    return allPathsReturn(Unsafe->Statement.get());
  }
  // Expressions wrapped in Stmt
  if (auto *ES = dynamic_cast<ExprStmt *>(S)) {
    Expr *E = ES->Expression.get();
    if (auto *If = dynamic_cast<IfExpr *>(E)) {
      if (If->Else && allPathsReturn(If->Then.get()) &&
          allPathsReturn(If->Else.get()))
        return true;
      return false;
    }
    if (auto *Match = dynamic_cast<MatchExpr *>(E)) {
      for (const auto &Arm : Match->Arms) {
        if (!allPathsReturn(Arm->Body.get()))
          return false;
      }
      return true;
    }
    if (auto *Loop = dynamic_cast<LoopExpr *>(E)) {
      if (allPathsReturn(Loop->Body.get()))
        return true;
      return false;
    }
  }
  return false;
}

bool Sema::allPathsJump(Stmt *S) {
  if (!S)
    return false;
  if (dynamic_cast<ReturnStmt *>(S) || dynamic_cast<UnreachableStmt *>(S))
    return true;
  if (auto *B = dynamic_cast<BlockStmt *>(S)) {
    for (const auto &Sub : B->Statements) {
      if (allPathsJump(Sub.get()))
        return true;
    }
    return false;
  }
  if (auto *Unsafe = dynamic_cast<UnsafeStmt *>(S)) {
    return allPathsJump(Unsafe->Statement.get());
  }
  if (auto *ES = dynamic_cast<ExprStmt *>(S)) {
    Expr *E = ES->Expression.get();
    if (dynamic_cast<BreakExpr *>(E) || dynamic_cast<ContinueExpr *>(E))
      return true;
    if (auto *If = dynamic_cast<IfExpr *>(E)) {
      if (If->Else && allPathsJump(If->Then.get()) &&
          allPathsJump(If->Else.get()))
        return true;
      return false;
    }
    if (auto *Match = dynamic_cast<MatchExpr *>(E)) {
      for (const auto &Arm : Match->Arms) {
        if (!allPathsJump(Arm->Body.get()))
          return false;
      }
      return true;
    }
    if (auto *Loop = dynamic_cast<LoopExpr *>(E)) {
      // Loop body jumping out counts as jump
      if (allPathsJump(Loop->Body.get()))
        return true;
      return false;
    }
  }
  return false;
}

void Sema::checkStmt(Stmt *S) {
  if (!S)
    return;

  if (auto *Block = dynamic_cast<BlockStmt *>(S)) {
    enterScope();
    for (auto &SubStmt : Block->Statements) {
      checkStmt(SubStmt.get());
    }

    // SCOPE GUARD: Hot Potato Check
    // Iterate over symbols in the current scope before exiting.
    // If any symbol is a Reference with DirtyReferentMask != Full,
    // we must ensure its Referent (BorrowedFrom) is now Clean.
    // However, the Referent might be in a parent scope. We need to check the
    // Referent's CURRENT state. Wait, simpler model first: If the Ref is marked
    // Dirty, it means it TOOK responsibility. We check if the Ref *itself*
    // thinks it's done? No, the Ref doesn't update its own DirtyMask
    // automatically unless we implement flow sensitive updates to
    // DirtyReferentMask on assignment. BETTER APPROACH based on plan: "Check if
    // the referent (Source) has been fully initialized (Cleaned) within this
    // scope."

    // We need to iterate the Symbols in CurrentScope.
    for (auto const &[name, info] : CurrentScope->Symbols) {
      if (info.IsReference() && info.DirtyReferentMask != ~0ULL) {
        // It was a dirty reference.
        // Check if it's still dirty?
        // Actually, we should check the SOURCE variable's current InitMask.
        SymbolInfo *sourceInfo = nullptr;
        if (!info.BorrowedFrom.empty() &&
            CurrentScope->findSymbol(info.BorrowedFrom, sourceInfo)) {
          bool referentIsShape =
              (sourceInfo->TypeObj && sourceInfo->TypeObj->isShape());
          uint64_t signature = ~0ULL;
          if (referentIsShape) {
            std::string soul = sourceInfo->TypeObj->getSoulName();
            if (ShapeMap.count(soul)) {
              ShapeDecl *SD = ShapeMap[soul];
              signature = (1ULL << SD->Members.size()) - 1;
              if (SD->Members.size() >= 64)
                signature = ~0ULL;
            }
          }

          // Check if Source is now fully initialized
          if ((sourceInfo->InitMask & signature) != signature) {
            DiagnosticEngine::report(getLoc(Block),
                                     DiagID::ERR_DIRTY_REF_ESCAPE, name,
                                     info.BorrowedFrom);
            HasError = true;
          }
        }
      }
    }

    exitScope();
  } else if (auto *Ret = dynamic_cast<ReturnStmt *>(S)) {
    std::string ExprType = "void";
    std::shared_ptr<toka::Type> ExprTypeObj = toka::Type::fromString("void");
    if (Ret->ReturnValue) {
      Ret->ReturnValue = foldGenericConstant(std::move(Ret->ReturnValue));
      m_ControlFlowStack.push_back(
          {"", CurrentFunctionReturnType, nullptr, false, true});
      auto RetTypeObj = checkExpr(Ret->ReturnValue.get());
      ExprTypeObj = RetTypeObj;
      ExprType = RetTypeObj->toString();
      m_ControlFlowStack.pop_back();

      // Escape Blockade: Check for Dirty Reference
      if (auto *Var = dynamic_cast<VariableExpr *>(Ret->ReturnValue.get())) {
        SymbolInfo *info = nullptr;
        if (CurrentScope->findSymbol(Var->Name, info)) {
          if (info->IsReference() && info->DirtyReferentMask != ~0ULL) {
            DiagnosticEngine::report(getLoc(Ret), DiagID::ERR_ESCAPE_UNSET,
                                     Var->Name);
            HasError = true;
          }
        }
      }

      // [NEW] Unified Lifetime Check logic
      std::set<std::string> returnedDeps;

      // Helper to extract path
      std::function<std::string(Expr *)> getPath = [&](Expr *E) -> std::string {
          if (!E) return "";
          if (auto *ve = dynamic_cast<VariableExpr *>(E)) return ve->Name;
          if (auto *me = dynamic_cast<MemberExpr *>(E)) return getPath(me->Object.get()) + "." + toka::Type::stripMorphology(me->Member);
          if (auto *ue = dynamic_cast<UnaryExpr *>(E)) return getPath(ue->RHS.get());
          if (auto *ae = dynamic_cast<AddressOfExpr *>(E)) return getPath(ae->Expression.get());
          return "";
      };

      // Helper to collect dependencies from the returned expression
      std::function<void(Expr *)> collectDeps = [&](Expr *E) {
        if (!E)
          return;

        // Case 1: Taking address `&var` or `&var.field`
        if (auto *Addr = dynamic_cast<UnaryExpr *>(E)) {
          if (Addr->Op == TokenType::Ampersand) {
            std::string path = getPath(Addr->RHS.get());
            if (!path.empty()) {
                if (dynamic_cast<VariableExpr*>(Addr->RHS.get()) && Addr->RHS->ResolvedType && !Addr->RHS->ResolvedType->isReference() && !Addr->RHS->ResolvedType->isPointer() && !Addr->RHS->ResolvedType->isSharedPtr() && !Addr->RHS->ResolvedType->isUniquePtr()) {
                    DiagnosticEngine::report(getLoc(E), DiagID::ERR_ESCAPE_LOCAL, path);
                    HasError = true;
                } else {
                    returnedDeps.insert(path);
                }
            }
          } else if (auto *Paren = dynamic_cast<TupleExpr *>(E)) {
            // Parentheses around expression
            for (auto &sub : Paren->Elements)
              collectDeps(sub.get());
          }
        }
        // Case 2: Returning existing reference variable `x`
        else if (auto *Var = dynamic_cast<VariableExpr *>(E)) {
          SymbolInfo info;
          if (CurrentScope->lookup(Var->Name, info)) {
            if (info.IsReference()) {
              // It depends on whatever 'info' borrowed from
              if (!info.BorrowedFrom.empty()) {
                returnedDeps.insert(info.BorrowedFrom);
              }
              // Also merge its transitive dependencies if we track them
              returnedDeps.insert(info.LifeDependencySet.begin(),
                                  info.LifeDependencySet.end());
            }
          }
        }
        // Case 3: Tuple Return `(&a, &b)`
        else if (auto *Tup = dynamic_cast<TupleExpr *>(E)) {
          for (auto &Elem : Tup->Elements) {
            collectDeps(Elem.get());
          }
        }
        // Case 4: CallExpr
        else if (auto *Call = dynamic_cast<CallExpr *>(E)) {
            for (auto &Arg : Call->Args) {
                collectDeps(Arg.get());
            }
        }
        // Case 5: InitStructExpr / AnonymousRecordExpr
        else if (auto *Init = dynamic_cast<InitStructExpr *>(E)) {
            for (auto &Mem : Init->Members) {
                collectDeps(Mem.second.get());
            }
        } else if (auto *Anon = dynamic_cast<AnonymousRecordExpr *>(E)) {
            for (auto &Field : Anon->Fields) {
                collectDeps(Field.second.get());
            }
        }
        // Case 6: Fallback for BinaryExpr named arg init if kept as CallExpr
        else if (auto *Bin = dynamic_cast<BinaryExpr *>(E)) {
            if (Bin->Op == "=") {
                collectDeps(Bin->RHS.get());
            }
        }
        // Case 7: MemberExpr (e.g., e.&val)
        else if (auto *Memb = dynamic_cast<MemberExpr *>(E)) {
            bool isRef = false;
            bool isAddressOf = Memb->Member.find('&') != std::string::npos;
            if (isAddressOf || Memb->Member.find('^') != std::string::npos || Memb->Member.find('~') != std::string::npos) {
                isRef = true;
            }
            if (isRef) {
                std::string path = getPath(Memb);
                if (!path.empty()) {
                    returnedDeps.insert(path);
                }
            }
        }
      };

      collectDeps(Ret->ReturnValue.get());

      // Validate dependencies against declared LifeDependencies
      if (CurrentFunction) {
        for (const auto &dep : returnedDeps) {
          // 1. Is it a parameter that can outlive the function?
          bool isParam = false;
          std::string baseDep = dep;
          size_t dotPos = baseDep.find('.');
          if (dotPos != std::string::npos) baseDep = baseDep.substr(0, dotPos);

          for (const auto &Arg : CurrentFunction->Args) {
            if (Arg.Name == baseDep) {
                isParam = true;
                break;
            }
          }

          if (!isParam) {
            DiagnosticEngine::report(getLoc(Ret), DiagID::ERR_ESCAPE_LOCAL, dep);
            HasError = true;
            continue;
          }

          // 2. Is it allowed via effects?
          bool allowed = false;
          for (const auto &allowedDep : CurrentFunction->LifeDependencies) {
            if (dep == allowedDep || (dep.size() > allowedDep.size() && dep.substr(0, allowedDep.size() + 1) == allowedDep + ".")) {
              allowed = true;
              break;
            }
          }
          if (!allowed) {
            for (const auto &pair : CurrentFunction->MemberDependencies) {
               for (const auto &allowedDep : pair.second) {
                 if (dep == allowedDep || (dep.size() > allowedDep.size() && dep.substr(0, allowedDep.size() + 1) == allowedDep + ".")) {
                   allowed = true;
                   break;
                 }
               }
               if (allowed) break;
            }
          }

          if (!allowed) {
            DiagnosticEngine::report(getLoc(Ret), DiagID::ERR_LIFETIME_UNION_REQUIRED, dep, dep);
            HasError = true;
          }
        }
      }
    }


    std::shared_ptr<toka::Type> expectedRetObj = nullptr;
    if (CurrentFunction && CurrentFunction->ResolvedReturnType && CurrentFunctionReturnType == CurrentFunction->ReturnType) {
        expectedRetObj = CurrentFunction->ResolvedReturnType;
    } else {
        expectedRetObj = resolveType(toka::Type::fromString(CurrentFunctionReturnType));
    }

    bool bypassNullRet = false;
    if (m_InUnsafeContext && expectedRetObj && expectedRetObj->isRawPointer() && ExprTypeObj && ExprTypeObj->isNullType()) {
        bypassNullRet = true;
    }
    if (ExprTypeObj && ExprTypeObj->isNullType()) {
         std::cerr << "[DEBUG] Return null check: unsafe=" << m_InUnsafeContext 
                   << " expected=" << (expectedRetObj ? expectedRetObj->toString() : "null")
                   << " isRaw=" << (expectedRetObj ? std::to_string(expectedRetObj->isRawPointer()) : "0")
                   << " bypass=" << bypassNullRet << "\n";
    }

    if (!bypassNullRet && !isTypeCompatible(expectedRetObj, ExprTypeObj)) {
      DiagnosticEngine::report(getLoc(Ret), DiagID::ERR_TYPE_MISMATCH, ExprType,
                               CurrentFunctionReturnType);
      HasError = true;
    } else {
      // Strict Morphology Check for Return
      if (expectedRetObj && expectedRetObj->IsCede) {
        if (Ret->ReturnValue && !dynamic_cast<CedeExpr*>(Ret->ReturnValue.get())) {
          DiagnosticEngine::report(getLoc(Ret), DiagID::ERR_EXPECTED_CEDE_RETURN, CurrentFunctionReturnType);
          HasError = true;
        }
      }

      MorphKind targetMorph = MorphKind::None;
      if (!CurrentFunctionReturnType.empty()) {
        char c = CurrentFunctionReturnType[0];
        if (c == '*')
          targetMorph = MorphKind::Raw;
        else if (c == '^')
          targetMorph = MorphKind::Unique;
        else if (c == '~')
          targetMorph = MorphKind::Shared;
        else if (c == '&')
          targetMorph = MorphKind::Ref;
      }
      MorphKind sourceMorph = getSyntacticMorphology(Ret->ReturnValue.get());
      
      bool exempt = false;
      Expr *e = Ret->ReturnValue.get();
      while (e) {
          if (e->IsMorphicExempt) { exempt = true; break; }
          if (auto *un = dynamic_cast<UnaryExpr *>(e)) e = un->RHS.get();
          else break;
      }
      
      if (!exempt) {
          checkStrictMorphology(Ret, targetMorph, sourceMorph, "return value");
      }
    }
  } else if (auto *Free = dynamic_cast<FreeStmt *>(S)) {
    Free->Expression = foldGenericConstant(std::move(Free->Expression));
    auto FreeTypeObj = checkExpr(Free->Expression.get());
    if (!FreeTypeObj->isRawPointer()) {
      std::string ExprType = FreeTypeObj->toString();
      if (FreeTypeObj->isSmartPointer()) {
        DiagnosticEngine::report(getLoc(Free), DiagID::ERR_FREE_SMART,
                                 ExprType);
        HasError = true;
      } else {
        DiagnosticEngine::report(getLoc(Free), DiagID::ERR_FREE_NON_PTR,
                                 ExprType);
        HasError = true;
      }
    }
  } else if (auto *Unsafe = dynamic_cast<UnsafeStmt *>(S)) {
    bool oldUnsafe = m_InUnsafeContext;
    m_InUnsafeContext = true;
    checkStmt(Unsafe->Statement.get());
    m_InUnsafeContext = oldUnsafe;
  } else if (auto *ExprS = dynamic_cast<ExprStmt *>(S)) {
    // Standalone expressions are NOT receivers
    m_ControlFlowStack.push_back({"", "void", nullptr, false, false});
    ExprS->Expression = foldGenericConstant(std::move(ExprS->Expression));
    checkExpr(ExprS->Expression.get());
    m_ControlFlowStack.pop_back();

  } else if (auto *Var = dynamic_cast<VariableDecl *>(S)) {
    // [Constitutional 1.3] Adversarial Principle: $ is only for contesting
    // inheritance.
    if (Var->IsValueBlocked || Var->IsRebindBlocked) {
      DiagnosticEngine::report(getLoc(Var), DiagID::ERR_REDUNDANT_BLOCK,
                               Var->Name);
      HasError = true;
    }

    std::string InitType = "";
    std::shared_ptr<toka::Type> InitTypeObj = nullptr;
    if (Var->Init) {
      Var->Init = foldGenericConstant(std::move(Var->Init));
      // [Auto-Clone]
      if (!Var->IsReference) {
        tryInjectAutoClone(Var->Init);
      }
      if (Var->IsReference)
        m_AllowUnsetUsage = true;
      m_ControlFlowStack.push_back({Var->Name, "void", nullptr, false, true});
      if (!Var->TypeName.empty() && Var->TypeName != "auto") {
        auto declTargetTy = resolveType(toka::Type::fromString(Var->TypeName), false);
        if (declTargetTy && (declTargetTy->typeKind == toka::Type::Function || declTargetTy->typeKind == toka::Type::DynFn)) {
           if (auto clo = dynamic_cast<ClosureExpr*>(Var->Init.get())) {
              std::vector<std::shared_ptr<Type>> paramTypes;
              std::shared_ptr<Type> returnType;
              if (declTargetTy->typeKind == toka::Type::DynFn) {
                  auto fnTy = std::static_pointer_cast<toka::DynFnType>(declTargetTy);
                  paramTypes = fnTy->ParamTypes;
                  returnType = fnTy->ReturnType;
              } else {
                  auto fnTy = std::static_pointer_cast<toka::FunctionType>(declTargetTy);
                  paramTypes = fnTy->ParamTypes;
                  returnType = fnTy->ReturnType;
              }
              
              clo->InjectedParamTypes = paramTypes;
              if ((clo->ReturnType.empty() || clo->ReturnType == "unknown") && returnType) {
                  clo->ReturnType = returnType->toString();
              }
           }
        }
      }
      
      bool oldExpectedWritability = m_ExpectedWritability;
      if (Var->IsReference) {
          m_ExpectedWritability = Var->IsValueMutable;
      }
      m_LastBorrowSource = ""; // [NEW] Clear stale borrow source
      InitTypeObj = checkExpr(Var->Init.get());
      m_ExpectedWritability = oldExpectedWritability;
      InitType = InitTypeObj->toString();
      m_AllowUnsetUsage = false;
    }

    // 4. If type not specified, infer from init
    if (Var->TypeName.empty() || Var->TypeName == "auto") {
      if (InitType.empty() || InitType == "void") {
        DiagnosticEngine::report(getLoc(Var), DiagID::ERR_TYPE_REQUIRED,
                                 Var->Name);
        HasError = true;
        Var->TypeName = "unknown";
      } else {
        std::string Inferred = InitType;
        if (Inferred == "null") {
          DiagnosticEngine::report(getLoc(Var), DiagID::ERR_INFER_NULLPTR);
          HasError = true;
          Var->TypeName = "unknown";
          return;
        }

        // If variable declares morphology (auto ^p = ...), strip matching
        // morphology from inferred soul
        if (Var->HasPointer || Var->IsUnique || Var->IsShared ||
            Var->IsReference) {
          if (Inferred.find("nul ") == 0) Inferred = Inferred.substr(4);
          if (!Inferred.empty() && (Inferred[0] == '*' || Inferred[0] == '^' ||
                                    Inferred[0] == '~' || Inferred[0] == '&')) {
            Inferred = Inferred.substr(1);
            if (!Inferred.empty() && (Inferred[0] == '?' ||
                                      Inferred[0] == '!' || Inferred[0] == '#'))
              Inferred = Inferred.substr(1);
          } else {
            // [NEW] Implicit Box Support for `auto`
            if (Var->IsShared || Var->IsUnique) {
              auto boxExpr = std::make_unique<ImplicitBoxExpr>(std::move(Var->Init), Var->IsShared, Var->IsUnique);
              std::string boxTy = (Var->IsShared ? "~" : "^") + Inferred;
              if (Var->IsValueMutable) boxTy += "#";
              boxExpr->ResolvedType = toka::Type::fromString(resolveType(boxTy));
              boxExpr->Loc = boxExpr->Initializer->Loc;
              Var->Init = std::move(boxExpr);
              // Now the assigned expression *IS* the pointer, so Inferred effectively matches the requested pointer type.
              // However, since we are in the `auto` block, setting `Var->TypeName = Inferred` (which is just 'Point') 
              // will cause `DeclFullTy` reconstruction at the end to prepend the sigil correctly without stripping it!
            } else if (Var->HasPointer) {
               DiagnosticEngine::report(getLoc(Var), DiagID::ERR_INIT_TYPE_MISMATCH, "*(Raw Pointer)", Inferred);
               HasError = true;
               Var->TypeName = "unknown";
               return;
            }
          }
        } else {
          // Strict: No sigil, no pointer, except for "cstring" (implicit *char)
          if (!Var->IsMorphicExempt && !Inferred.empty() &&
              (Inferred[0] == '*' || Inferred[0] == '^' || Inferred[0] == '~' ||
               Inferred[0] == '&') &&
              Inferred.substr(0, 3) != "cstring") {
            std::string sigilStr = std::string(1, Inferred[0]);
            DiagnosticEngine::report(getLoc(Var),
                                     DiagID::ERR_POINTER_SIGIL_MISSING,
                                     Var->Name, Inferred, sigilStr, Var->Name);
            HasError = true;
            Var->TypeName = "unknown";
            return;
          }
        }
        

        
        // [New] Decay inherently mutable auto-inferred types to ReadOnly if var doesn't declare it.
        if (!Var->IsValueMutable && !Inferred.empty() && Inferred.back() == '#') {
            Inferred.pop_back();
        }

        Var->TypeName = Inferred;
      }
    } else {
      // Compatibility Check
      std::string DeclFullTy = Var->TypeName;
      std::string Morph = "";
      if (Var->HasPointer)
        Morph = "*";
      else if (Var->IsUnique)
        Morph = "^";
      else if (Var->IsShared)
        Morph = "~";
      else if (Var->IsReference)
        Morph = "&";
      if (!Morph.empty()) {
        if (Var->IsRebindable)
          Morph += "#";
        if (Var->IsPointerNullable)
          Morph = "nul " + Morph;
        DeclFullTy = Morph + DeclFullTy;
      }
      if (Var->IsValueMutable) {
        DeclFullTy += "#";
      }

      if (!InitType.empty() && !isTypeCompatible(toka::Type::fromString(resolveType(DeclFullTy)), InitTypeObj)) {
        // [NEW] Implicit Box Support

        std::string boxedType = (Var->IsShared ? "~" : (Var->IsUnique ? "^" : ""));
        if (!boxedType.empty()) {
           std::string testTy = boxedType + InitType;
           if (Var->IsValueMutable) testTy += "#";
           if (Var->IsPointerNullable) testTy = "nul " + testTy;
           

           if (isTypeCompatible(toka::Type::fromString(resolveType(DeclFullTy)), toka::Type::fromString(resolveType(testTy)))) {

              auto boxExpr = std::make_unique<ImplicitBoxExpr>(std::move(Var->Init), Var->IsShared, Var->IsUnique);
              boxExpr->ResolvedType = toka::Type::fromString(resolveType(DeclFullTy));
              boxExpr->Loc = boxExpr->Initializer->Loc;
              Var->Init = std::move(boxExpr);
              InitType = DeclFullTy; // Trick subsequent checks
           } else {
             DiagnosticEngine::report(getLoc(Var), DiagID::ERR_INIT_TYPE_MISMATCH, DeclFullTy, InitType);
             HasError = true;
           }
        } else {
           DiagnosticEngine::report(getLoc(Var), DiagID::ERR_INIT_TYPE_MISMATCH, DeclFullTy, InitType);
           HasError = true;
        }
      }
    }

    // 5. Strict Morphology Check
    if (Var->Init && !Var->IsReference) {
      MorphKind lhsMorph = MorphKind::None;
      if (Var->IsUnique)
        lhsMorph = MorphKind::Unique;
      else if (Var->IsShared)
        lhsMorph = MorphKind::Shared;
      else if (Var->IsReference)
        lhsMorph = MorphKind::Ref;
      else if (Var->HasPointer)
        lhsMorph = MorphKind::Raw;

      MorphKind rhsMorph = getSyntacticMorphology(Var->Init.get());

      checkStrictMorphology(Var, lhsMorph, rhsMorph, Var->Name);
    }

    SymbolInfo Info;
    std::string morph = "";
    if (Var->HasPointer)
      morph = "*";
    else if (Var->IsUnique)
      morph = "^";
    else if (Var->IsShared)
      morph = "~";
    else if (Var->IsReference)
      morph = "&";

    std::string baseType = Var->TypeName;
    bool hadNul = false;
    if (baseType.size() > 4 && baseType.substr(0, 4) == "nul ") {
      hadNul = true;
      baseType = baseType.substr(4);
    }
    
    // Strip redundant sigil from baseType if it matches morph
    if (baseType.size() > 1 && (baseType[0] == '^' || baseType[0] == '~' ||
                                baseType[0] == '*' || baseType[0] == '&')) {
      if (morph.empty()) {
        morph = std::string(1, baseType[0]);
        baseType = baseType.substr(1);
      } else if (morph[0] == baseType[0]) {
        baseType = baseType.substr(1);
      }
    }
    
    if (baseType.size() > 1 && baseType[0] == '#') {
       baseType = baseType.substr(1);
    }

    if (!morph.empty()) {
      if (Var->IsRebindable && morph.find('#') == std::string::npos)
        morph += "#";
      if ((Var->IsPointerNullable || hadNul) && morph.find("nul") == std::string::npos)
        morph = "nul " + morph;
    }
    if (morph == "&" && !m_LastBorrowSource.empty()) {
      Info.BorrowedFrom = m_LastBorrowSource;
      Info.LifeDependencySet.insert(m_LastBorrowSource);

      // [NEW] Lifetime check: Depth(Me) >= Depth(Src)
      int srcDepth = getScopeDepth(m_LastBorrowSource);
      int myDepth = CurrentScope->Depth;
      if (myDepth < srcDepth) {
        DiagnosticEngine::report(getLoc(Var), DiagID::ERR_BORROW_LIFETIME,
                                 Var->Name, m_LastBorrowSource);
        HasError = true;
      }

      // [Hot Potato] Propagate InitMask from Source to Reference
      SymbolInfo *srcPtr = nullptr;
      if (CurrentScope->findSymbol(m_LastBorrowSource, srcPtr)) {
        // If source is not fully initialized, mark this reference as holding a
        // Hot Potato We use the same 'signature' logic as elsewhere to
        // determine "Full"
        uint64_t fullMask = ~0ULL;
        if (srcPtr->TypeObj && srcPtr->TypeObj->isShape()) {
          std::string soul = srcPtr->TypeObj->getSoulName();
          if (ShapeMap.count(soul)) {
            ShapeDecl *SD = ShapeMap[soul];
            uint64_t bits = (1ULL << SD->Members.size()) - 1;
            if (SD->Members.size() >= 64)
              bits = ~0ULL;
            fullMask = bits;
          }
        }

        if ((srcPtr->InitMask & fullMask) != fullMask) {
          // It's Dirty!
          Info.DirtyReferentMask = srcPtr->InitMask;
        } else {
          Info.DirtyReferentMask = ~0ULL; // Clean
        }
      }

      Info.BorrowedFrom = m_LastBorrowSource;
      if (!m_LastBorrowSource.empty()) {
          PALCheckerState.commitTransient(m_LastBorrowSource);
      }
    }

    if (!m_LastLifeDependencies.empty()) {
      for (const auto &dep : m_LastLifeDependencies) {
        Info.LifeDependencySet.insert(dep);
        int srcDepth = getScopeDepth(dep);
        int myDepth = CurrentScope->Depth;
        if (myDepth < srcDepth) {
          DiagnosticEngine::report(getLoc(Var), DiagID::ERR_BORROW_LIFETIME,
                                   Var->Name, dep);
          HasError = true;
        }
      }
      m_LastLifeDependencies.clear();
    }
    
    if (!m_LastFieldDependencies.empty()) {
      Info.FieldDependencySet = m_LastFieldDependencies;
      m_LastFieldDependencies.clear();
    }

    m_LastBorrowSource = ""; // Clear for next var

    // [Constitution 1.3] Dual-Attribute Synthesis
    std::string fullType = "";
    // 1. Morphology sigil (Constitutional 1.3 - Leading)
    fullType += morph;

    // 2. Identity/Handle prefix attributes
    // (Already handled in morph string construction)

    // 3. Base Type Name
    fullType += baseType;

    // 4. Soul/Value suffix attributes
    if (Var->IsValueNullable)
      fullType += "?";
    if (Var->IsValueMutable || (morph.empty() && Var->IsRebindable))
      fullType += "#";

    Info.TypeObj = resolveType(toka::Type::fromString(fullType), false);
    if (!Info.TypeObj) {
      Info.TypeObj = toka::Type::fromString(fullType);
    }
    Info.IsRebindable = Var->IsRebindable;
    Info.IsMorphicExempt = Var->IsMorphicExempt; // [NEW]
    Var->ResolvedType = Info.TypeObj;

    if (Var->Init) {
      Info.InitMask = m_LastInitMask;
    } else {
      Info.InitMask = 0;
    }

    // Rule: Numeric Substitution (Constant variables)
    if (Var->Init && Var->TypeName == "i32" && !Var->IsValueMutable) {
      if (auto *Num = dynamic_cast<NumberExpr *>(Var->Init.get())) {
        Info.HasConstValue = true;
        Info.ConstValue = Num->Value;
      }
      // Or if initialized with ANOTHER const variable (like N = M)
      else if (auto *RefVar = dynamic_cast<VariableExpr *>(Var->Init.get())) {
        SymbolInfo RefInfo;
        if (CurrentScope->lookup(RefVar->Name, RefInfo) &&
            RefInfo.HasConstValue) {
          Info.HasConstValue = true;
          Info.ConstValue = RefInfo.ConstValue;
        }
      }
    }

    CurrentScope->define(Var->Name, Info);

    // Move Logic: If initializing from a Unique Variable, move it.
    if (Var->Init && Info.IsUnique()) {
      Expr *InitExpr = Var->Init.get();
      // Unwrap unary ^ or ~ or * if it matches
      if (auto *Unary = dynamic_cast<UnaryExpr *>(InitExpr)) {
        InitExpr = Unary->RHS.get();
      }

      if (auto *RHSVar = dynamic_cast<VariableExpr *>(InitExpr)) {
        SymbolInfo *SourceInfoPtr = nullptr;
        std::string actName;
        if (CurrentScope->findVariableWithDeref(RHSVar->Name, SourceInfoPtr, actName)) {
          if (SourceInfoPtr->IsUnique()) {
            std::string conflictPath = PALCheckerState.verifyMutation(actName);
            if (!conflictPath.empty()) {
              DiagnosticEngine::report(getLoc(Var), DiagID::ERR_MOVE_BORROWED, conflictPath);
              HasError = true;
            }
            CurrentScope->markMoved(actName);
          }
        }
      } else if (auto *Memb = dynamic_cast<MemberExpr *>(InitExpr)) {
        // [Move Restriction Rule] Prohibit moving member out of shape that
        // has drop() Rule ONLY applies if we are moving a resource
        // (UniquePtr)
        if (InitTypeObj->isUniquePtr()) {
          auto objType = checkExpr(Memb->Object.get());
          std::shared_ptr<toka::Type> soulType = objType->getSoulType();
          std::string soul = soulType->getSoulName();
          if (m_ShapeProps.count(soul) && m_ShapeProps[soul].HasDrop) {
            error(Var, DiagID::ERR_MOVE_MEMBER_DROP, Memb->Member, soul);
          }
        }
      }
    }

    if (Var->Init) {
      m_ControlFlowStack.pop_back();
    }
  } else if (auto *Destruct = dynamic_cast<DestructuringDecl *>(S)) {
    auto initType = checkExpr(Destruct->Init.get());
    auto declType = toka::Type::fromString(Destruct->TypeName);

    // Basic check: declType should match initType
    if (!Destruct->TypeName.empty() && !initType->isUnknown() &&
        !initType->isTuple() &&
        !isTypeCompatible(declType, initType)) {
      DiagnosticEngine::report(getLoc(Destruct), DiagID::ERR_TYPE_MISMATCH,
                               initType->toString(), declType->toString());
      HasError = true;
    }

    std::string soulName = Type::stripMorphology(Destruct->TypeName);
    if (!soulName.empty() && ShapeMap.count(soulName)) {
      ShapeDecl *SD = ShapeMap[soulName];
      size_t Limit = std::min(SD->Members.size(), Destruct->Variables.size());
      for (size_t i = 0; i < Limit; ++i) {
        SymbolInfo Info;
        std::string fullType = SD->Members[i].Type;
        auto baseTypeObj = toka::Type::fromString(fullType);
        // Apply attributes to the SOUL
        auto soulType =
            baseTypeObj->withAttributes(Destruct->Variables[i].IsValueMutable,
                                        Destruct->Variables[i].IsValueNullable,
                                        Destruct->Variables[i].IsValueBlocked);

        if (Destruct->Variables[i].IsReference) {
          Info.TypeObj = std::make_shared<toka::ReferenceType>(soulType);
          Info.BorrowedFrom = m_LastBorrowSource;

        } else {
          Info.TypeObj = soulType;
        }
        CurrentScope->define(Destruct->Variables[i].Name, Info);
      }
    } else if (initType->typeKind == Type::Tuple) {
      auto tt = std::dynamic_pointer_cast<TupleType>(initType);
      size_t Limit = std::min(tt->Elements.size(), Destruct->Variables.size());
      for (size_t i = 0; i < Limit; ++i) {
        SymbolInfo Info;
        auto memType = tt->Elements[i];
        // Apply attributes to the SOUL
        auto soulType =
            memType->withAttributes(Destruct->Variables[i].IsValueMutable,
                                    Destruct->Variables[i].IsValueNullable,
                                    Destruct->Variables[i].IsValueBlocked);

        if (Destruct->Variables[i].IsReference) {
          Info.TypeObj = std::make_shared<toka::ReferenceType>(soulType);
          Info.BorrowedFrom = m_LastBorrowSource;
          // Distribute specific field dependencies to destructured references
          std::string fieldKey = std::to_string(i);
          if (m_LastFieldDependencies.count(fieldKey)) {
             for (const auto &dep : m_LastFieldDependencies[fieldKey]) {
                 Info.LifeDependencySet.insert(dep);
                 Info.BorrowedFrom = dep; // Simplified to just track the primary borrow source since token tracks only 1
             }
          }
          if (m_LastFieldDependencies.count(Destruct->Variables[i].Name)) { // Handle if mapped by name somehow
             for (const auto &dep : m_LastFieldDependencies[Destruct->Variables[i].Name]) {
                 Info.LifeDependencySet.insert(dep);
                 Info.BorrowedFrom = dep;
             }
          }
        } else {
          Info.TypeObj = soulType;
        }
        CurrentScope->define(Destruct->Variables[i].Name, Info);
      }
      m_LastFieldDependencies.clear();
    } else if (!soulName.empty() && TypeAliasMap.count(soulName)) {
      for (const auto &Var : Destruct->Variables) {
        SymbolInfo Info;
        std::string fullType = "i32"; // Fallback
        auto baseTypeObj = toka::Type::fromString(fullType);
        auto soulType = baseTypeObj->withAttributes(
            Var.IsValueMutable, Var.IsValueNullable, Var.IsValueBlocked);
        if (Var.IsReference) {
          Info.TypeObj = std::make_shared<toka::ReferenceType>(soulType);
          Info.BorrowedFrom = m_LastBorrowSource;
          if (!m_LastBorrowSource.empty()) {
              PALCheckerState.commitTransient(m_LastBorrowSource);
          }
        } else {
          Info.TypeObj = soulType;
        }
        CurrentScope->define(Var.Name, Info);
      }
    } else {
      for (const auto &Var : Destruct->Variables) {
        SymbolInfo Info;
        Info.TypeObj = toka::Type::fromString("unknown");
        CurrentScope->define(Var.Name, Info);
      }
    }
  } else if (auto *GuardBind = dynamic_cast<GuardBindStmt *>(S)) {
    auto targetTypeObj = checkExpr(GuardBind->Target.get());
    std::string targetType = targetTypeObj->toString();

    // [New] Temporary Lifetime Extension
    // Signal CodeGen that this target expression should have its lifetime extended
    // to the end of the current scope (block) if it is a temporary value.
    if (GuardBind->Target) {
        GuardBind->Target->ExtendLifetime = true;
    }

    // Check Pattern and bind variables into CurrentScope
    checkPattern(GuardBind->Pat.get(), targetType, false);

    bool isReceiver = false;
    if (!m_ControlFlowStack.empty()) {
      isReceiver = m_ControlFlowStack.back().IsReceiver;
    }
    m_ControlFlowStack.push_back({"", "void", nullptr, false, isReceiver});
    checkStmt(GuardBind->ElseBody.get());
    m_ControlFlowStack.pop_back();

    if (!allPathsJump(GuardBind->ElseBody.get())) {
      DiagnosticEngine::report(getLoc(GuardBind), DiagID::ERR_GUARD_MUST_DIVERGE);
      HasError = true;
    }
  } else if (auto *Unreachable = dynamic_cast<UnreachableStmt *>(S)) {
    // No-op for now, it's just a marker
  }

  // Clear uncommitted transient borrows created during this statement
  PALCheckerState.clearTransient();
}

} // namespace toka
