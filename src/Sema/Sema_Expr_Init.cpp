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
#include "toka/SourceManager.h"
#include "toka/Type.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace toka {

static SourceLocation getLoc(ASTNode *Node) { return Node->Loc; }

static std::string getStringifyPath(Expr *E) {
  if (!E)
    return "";
  if (auto *ve = dynamic_cast<VariableExpr *>(E)) {
    return ve->Name;
  }
  if (auto *me = dynamic_cast<MemberExpr *>(E)) {
    std::string member = toka::Type::stripMorphology(me->Member);
    return getStringifyPath(me->Object.get()) + "." + member;
  }
  if (auto *ue = dynamic_cast<UnaryExpr *>(E)) {
    return getStringifyPath(ue->RHS.get());
  }
  if (auto *ae = dynamic_cast<AddressOfExpr *>(E)) {
    return getStringifyPath(ae->Expression.get());
  }
  if (auto *ae = dynamic_cast<ArrayIndexExpr *>(E)) {
    return getStringifyPath(ae->Array.get());
  }
  return "";
}

void Sema::checkPattern(MatchArm::Pattern *Pat, const std::string &TargetType,
                        bool SourceIsMutable) {
  if (!Pat)
    return;

  std::string T = resolveType(TargetType);
  while (!T.empty() && (T.back() == '#' || T.back() == '?' || T.back() == '!')) {
      T.pop_back();
  }

  switch (Pat->PatternKind) {
  case MatchArm::Pattern::Literal: {
    // Literal patterns don't bind variables, but we must check types.
    // Pat->Name contains the raw text of the literal (Integer, String, true, false)
    std::shared_ptr<toka::Type> litType;
    if (Pat->Name == "true" || Pat->Name == "false") {
      litType = toka::Type::fromString("bool");
    } else if (!Pat->Name.empty() && Pat->Name[0] == '"') {
      litType = toka::Type::fromString("String");
    } else if (!Pat->Name.empty() && Pat->Name[0] == '\'') {
      litType = toka::Type::fromString("char");
    } else {
      // Assume integer for now (Parser sets LiteralVal for these)
      // We'll use the target type to disambiguate if it's an integer type
      auto targetObj = toka::Type::fromString(T);
      if (targetObj && targetObj->isInteger()) {
        litType = targetObj;
      } else {
        litType = toka::Type::fromString("i32");
      }
    }

    auto targetObj = toka::Type::fromString(T);
    if (targetObj && litType && !isTypeCompatible(targetObj, litType)) {
      error(Pat, DiagID::ERR_TYPE_MISMATCH, litType->toString(), T);
    }

    // Trait Check: If it's not a primitive, it must implement @PartialEq
    if (targetObj && !targetObj->isInteger() && !targetObj->isBoolean()) {
        std::string resolvedTarget = resolveType(T);
        std::string implKey = resolvedTarget + "@PartialEq";
        if (ImplMap.find(implKey) == ImplMap.end()) {
            error(Pat, DiagID::ERR_TRAIT_NOT_FOUND, "@PartialEq", T);
        }
    }
    break;
  }

  case MatchArm::Pattern::Or: {
    for (auto &sub : Pat->SubPatterns) {
      checkPattern(sub.get(), TargetType, false);
      if (sub->PatternKind == MatchArm::Pattern::Variable) {
          error(sub.get(), "Or-patterns currently do not support variable bindings. Use literals or _ instead.");
      }
    }
    break;
  }

  case MatchArm::Pattern::Wildcard:
    break;

  case MatchArm::Pattern::Variable: {
    // Check if Pattern->Name is actually a zero-payload variant of target T
    bool isVariant = false;
    std::string baseShapeName = T;
    size_t scopePos = Pat->Name.find("::");
    std::string patName = Pat->Name;
    if (scopePos != std::string::npos) {
      baseShapeName = resolveType(patName.substr(0, scopePos));
      patName = patName.substr(scopePos + 2);
    }
    if (ShapeMap.count(baseShapeName)) {
      ShapeDecl *SD = ShapeMap[baseShapeName];
      for (auto &Memb : SD->Members) {
        bool noPayload = Memb.Type.empty() || Memb.Type == "void";
        if (Memb.Name == patName && noPayload && Memb.SubMembers.empty()) {
          isVariant = true;
          break;
        } else if (Memb.Name == patName) {
          DiagnosticEngine::report(getLoc(Pat), DiagID::ERR_VARIANT_NO_PAYLOAD, patName);
          HasError = true;
          isVariant = true;
          break;
        }
      }
    }

    if (isVariant) {
      // It's a zero-payload variant, do not bind as a variable
      break;
    }

    SymbolInfo Info;
    // Type Migration Stage 1: Coexistence
    // Construct type string to parse object. Pattern bindings infer
    // type T. If Reference, it is &T.
    std::string fullType = "";
    if (Pat->IsReference)
      fullType = "&";
    fullType += T;
    // Patterns usually don't have rebind/nullable sigils unless
    // explicit? In match arms, we trust the inferred type T. But wait,
    // T comes from resolveType(TargetType).
    Info.TypeObj = toka::Type::fromString(fullType);

    if (!Pat->Name.empty() && Pat->Name[0] == '\'') {
        Info.IsMorphicExempt = true;
    }

    if (Info.TypeObj) {
        Info.TypeObj = Info.TypeObj->withAttributes(Pat->IsValueMutable, false);
        
        // [Safety Gate] Prevent implicit destructure copying of Resources
        if (!Pat->IsReference && !Info.IsMorphicExempt) {
            std::string soulName = Info.TypeObj->getSoulName();
            if (!soulName.empty() && ShapeMap.count(soulName)) {
                if (!ShapeMap[soulName]->MangledDestructorName.empty()) {
                    DiagnosticEngine::report(getLoc(Pat), DiagID::ERR_ILLEGAL_RESOURCE_COPY, soulName, Pat->Name);
                    HasError = true;
                }
            }
        }
    }

    CurrentScope->define(Pat->Name, Info);
    break;
  }

  case MatchArm::Pattern::Decons: {
    // Pat->Name might be "Ok" or "Result::Ok"
    std::string variantName = Pat->Name;
    std::string shapeName = T;

    size_t pos = variantName.find("::");
    if (pos != std::string::npos) {
      std::string requestedShapeCore = variantName.substr(0, pos);
      size_t ltReq = requestedShapeCore.find("<");
      if (ltReq != std::string::npos) requestedShapeCore = requestedShapeCore.substr(0, ltReq);
      variantName = variantName.substr(pos + 2);
      while (TypeAliasMap.count(requestedShapeCore) && !TypeAliasMap[requestedShapeCore].IsStrong) {
          requestedShapeCore = TypeAliasMap[requestedShapeCore].Target;
          size_t lt = requestedShapeCore.find("<");
          if (lt != std::string::npos) requestedShapeCore = requestedShapeCore.substr(0, lt);
      }
      
      std::string T_base = T;
      size_t ltT = T.find("_M"); 
      if (ltT == std::string::npos) ltT = T.find("<");
      if (ltT != std::string::npos) T_base = T.substr(0, ltT);
      
      if (TypeAliasMap.count(T_base) && !TypeAliasMap[T_base].IsStrong) {
          T_base = TypeAliasMap[T_base].Target;
          size_t lt = T_base.find("<");
          if (lt != std::string::npos) T_base = T_base.substr(0, lt);
      }
      
      bool isMatch = (T_base == requestedShapeCore);
      
      if (!isMatch) {
          DiagnosticEngine::report(getLoc(Pat), DiagID::ERR_UNKNOWN_SHAPE_IN_PAT, requestedShapeCore);
          HasError = true;
      }
      shapeName = T;
    } else {
      shapeName = T;
    }

    if (ShapeMap.count(shapeName)) {
      ShapeDecl *SD = ShapeMap[shapeName];
      ShapeMember *foundMemb = nullptr;
      for (auto &Memb : SD->Members) {
        if (Memb.Name == variantName) {
          foundMemb = &Memb;
          break;
        }
      }

      if (foundMemb) {
        if (Pat->SubPatterns.size() > 0) {
          bool noPayload = foundMemb->Type.empty() || foundMemb->Type == "void";
          if (noPayload && foundMemb->SubMembers.empty()) {
            DiagnosticEngine::report(
                getLoc(Pat), DiagID::ERR_VARIANT_NO_PAYLOAD, variantName);
            HasError = true;
          } else {
            if (!foundMemb->SubMembers.empty()) {
              // Multi-field tuple variant
              if (Pat->SubPatterns.size() != foundMemb->SubMembers.size()) {
                DiagnosticEngine::report(
                    getLoc(Pat), DiagID::ERR_VARIANT_ARG_MISMATCH, variantName,
                    foundMemb->SubMembers.size(), Pat->SubPatterns.size());
                HasError = true;
              } else {
                for (size_t i = 0; i < Pat->SubPatterns.size(); ++i) {
                  // Rebind check
                  checkPattern(Pat->SubPatterns[i].get(),
                               foundMemb->SubMembers[i].Type, SourceIsMutable);
                }
              }
            } else {
              // Legacy single-field variant
              if (Pat->SubPatterns.size() != 1) {
                DiagnosticEngine::report(
                    getLoc(Pat), DiagID::ERR_VARIANT_ARG_MISMATCH, variantName,
                    1, Pat->SubPatterns.size());
                HasError = true;
              }
              checkPattern(Pat->SubPatterns[0].get(), foundMemb->Type,
                           SourceIsMutable);
            }
          }
        }
      } else {
        DiagnosticEngine::report(
            getLoc(Pat), DiagID::ERR_UNKNOWN_SHAPE_IN_PAT,
            shapeName); // Actually variant not found in shape
        HasError = true;
      }
    } else {
      DiagnosticEngine::report(getLoc(Pat), DiagID::ERR_UNKNOWN_SHAPE_IN_PAT,
                               shapeName);
      HasError = true;
    }
    break;
  }
  }
}

std::shared_ptr<toka::Type> Sema::checkShapeInit(InitStructExpr *Init) {
  std::string OriginalName = Init->ShapeName; // [Fix] Capture original name
  std::map<std::string, uint64_t> memberMasks;
  std::string resolvedName = resolveType(Init->ShapeName, true);

  // Helper lambda for inference (copied from original)
  auto performInference = [&](std::string &currentName, ShapeDecl *&SD) {
    if (!SD)
      return;
    // Try inference from m_ExpectedType first
    if (!SD->GenericParams.empty() && m_ExpectedType) {
      auto expShape =
          std::dynamic_pointer_cast<toka::ShapeType>(m_ExpectedType);
      if (expShape && (expShape->Name == SD->Name ||
                       expShape->Name.find(SD->Name + "_M") == 0)) {
        currentName = resolveType(m_ExpectedType->toString());
        SD = ShapeMap[currentName];
      }
    }

    // Inference from fields if still a template
    if (SD && !SD->GenericParams.empty() &&
        (currentName == SD->Name || currentName == SD->Name + "<>")) {
      std::map<std::string, std::string> inferred;
      for (auto const &pair : Init->Members) {
        std::string fieldName = Type::stripMorphology(pair.first);
        const ShapeMember *pM = nullptr;
        for (const auto &m : SD->Members) {
          if (m.Name == fieldName) {
            pM = &m;
            break;
          }
        }
        if (!pM)
          continue;

        auto valType = checkExpr(pair.second.get(), nullptr);
        if (!valType)
          continue;

        if (pM->Type.find("[") == 0) {
          auto arrTy = std::dynamic_pointer_cast<toka::ArrayType>(valType);
          if (arrTy) {
            size_t semi = pM->Type.find(';');
            size_t close = pM->Type.find(']');
            if (semi != std::string::npos && close != std::string::npos) {
              auto trim_inline = [](std::string s) {
                size_t first = s.find_first_not_of(" \t\n\r");
                if (first == std::string::npos)
                  return std::string("");
                size_t last = s.find_last_not_of(" \t\n\r");
                return s.substr(first, (last - first + 1));
              };
              std::string tName = trim_inline(pM->Type.substr(1, semi - 1));
              std::string nName =
                  trim_inline(pM->Type.substr(semi + 1, close - semi - 1));

              for (const auto &gp : SD->GenericParams) {
                if (gp.Name == tName && !gp.IsConst)
                  inferred[tName] = arrTy->ElementType->toString();
                if (gp.Name == nName && gp.IsConst)
                  inferred[nName] = std::to_string(arrTy->Size);
              }
            }
          }
        } else {
          for (const auto &gp : SD->GenericParams) {
            if (gp.Name == pM->Type && !gp.IsConst)
              inferred[gp.Name] = valType->toString();
          }
        }
      }

      if (inferred.size() > 0) {
        std::string fullName = SD->Name + "<";
        for (size_t i = 0; i < SD->GenericParams.size(); ++i) {
          std::string pName = SD->GenericParams[i].Name;
          if (inferred.count(pName))
            fullName += inferred[pName];
          else
            fullName += "unknown";
          if (i < SD->GenericParams.size() - 1)
            fullName += ", ";
        }
        fullName += ">";
        currentName = resolveType(fullName, true);
        SD = ShapeMap[currentName];
      }
    }
  };

  if (ShapeMap.count(resolvedName)) {
    ShapeDecl *SD = ShapeMap[resolvedName];
    performInference(resolvedName, SD);
    Init->ShapeName = resolvedName;

    if (!checkVisibility(Init, SD)) {
      return toka::Type::fromString("unknown");
    }

    std::shared_ptr<toka::Type> ResultType;
    if (SD->Kind == ShapeKind::Union || SD->Kind == ShapeKind::Enum) {
      ResultType = checkUnionInit(Init, SD, resolvedName, memberMasks);
    } else {
      ResultType = checkStructInit(Init, SD, resolvedName, memberMasks);
    }

    // [Fix] Strong Alias Preservation
    std::string BaseName = toka::Type::stripMorphology(OriginalName);
    size_t lt = BaseName.find('<');
    if (lt != std::string::npos) {
      BaseName = BaseName.substr(0, lt);
    }

    if (TypeAliasMap.count(BaseName) && TypeAliasMap[BaseName].IsStrong) {
      return toka::Type::fromString(OriginalName);
    }
    return ResultType;
  }

  DiagnosticEngine::report(getLoc(Init), DiagID::ERR_UNKNOWN_STRUCT,
                           Init->ShapeName);
  HasError = true;
  return toka::Type::fromString("unknown");
}

std::shared_ptr<toka::Type>
Sema::checkStructInit(InitStructExpr *Init, ShapeDecl *SD,
                      const std::string &resolvedName,
                      std::map<std::string, uint64_t> &memberMasks) {
  m_LastLifeDependencies.clear();
  std::set<std::string> providedFields;
  bool hasElision = false;
  Expr* elisionTarget = nullptr;

  for (size_t i = 0; i < Init->Members.size(); ++i) {
    auto &pair = Init->Members[i];
    if (pair.first == "..") {
      if (i != Init->Members.size() - 1) {
        error(Init, DiagID::ERR_ELISION_NOT_AT_END);
      }
      hasElision = true;
      if (auto *elExpr = dynamic_cast<ElisionExpr *>(pair.second.get())) {
        if (elExpr->Target) {
            elisionTarget = elExpr->Target.get();
            auto targetType = checkExpr(elisionTarget);
            if (targetType && targetType->getSoulName() != SD->Name) {
                error(elisionTarget, "Target elision must be of type '" + SD->Name + "', but got '" + targetType->toString() + "'");
                elisionTarget = nullptr;
            } else if (elisionTarget && !dynamic_cast<VariableExpr*>(elisionTarget) && !dynamic_cast<MemberExpr*>(elisionTarget) && !dynamic_cast<ArrayIndexExpr*>(elisionTarget)) {
                error(elisionTarget, "Target elision source must be a simple variable or access chain to avoid repeated side-effects");
                elisionTarget = nullptr;
            }
        }
      }
      continue;
    }

    if (providedFields.count(pair.first)) {
      DiagnosticEngine::report(getLoc(Init), DiagID::ERR_DUPLICATE_FIELD,
                               pair.first);
      HasError = true;
    }
    providedFields.insert(pair.first);

    bool fieldFound = false;
    const ShapeMember *pDefMember = nullptr;
    for (const auto &defField : SD->Members) {
      auto cleanDef = defField.Name;
      while (!cleanDef.empty() &&
             (cleanDef.back() == '#' || cleanDef.back() == '!' ||
              cleanDef.back() == '?'))
        cleanDef.pop_back();

      auto cleanProv = pair.first;
      while (!cleanProv.empty() &&
             (cleanProv.back() == '#' || cleanProv.back() == '!' ||
              cleanProv.back() == '?'))
        cleanProv.pop_back();

      if (cleanDef == cleanProv || toka::Type::stripMorphology(cleanDef) ==
                                       toka::Type::stripMorphology(cleanProv)) {
        fieldFound = true;
        pDefMember = &defField;
        break;
      }
    }

    if (!fieldFound) {
      error(Init, DiagID::ERR_NO_SUCH_MEMBER, resolvedName, pair.first);
      continue;
    }

    auto memberTypeObj = pDefMember->ResolvedType;
    if (!memberTypeObj)
      memberTypeObj = toka::Type::fromString(pDefMember->Type);

    // [New] Morphic Exemption: Validating Caller Transparency
    if (!pDefMember->IsMorphicExempt) {
      MorphKind providedMorph = MorphKind::None;
      if (pair.first.find('^') != std::string::npos) providedMorph = MorphKind::Unique;
      else if (pair.first.find('~') != std::string::npos) providedMorph = MorphKind::Shared;
      else if (pair.first.find('&') != std::string::npos) providedMorph = MorphKind::Ref;
      else if (pair.first.find('*') != std::string::npos) providedMorph = MorphKind::Raw;

      MorphKind expectedMorph = MorphKind::None;
      if (memberTypeObj->isRawPointer()) expectedMorph = MorphKind::Raw;
      else if (memberTypeObj->isUniquePtr()) expectedMorph = MorphKind::Unique;
      else if (memberTypeObj->isSharedPtr()) expectedMorph = MorphKind::Shared;
      else if (memberTypeObj->isReference()) expectedMorph = MorphKind::Ref;

      checkStrictMorphology(Init, expectedMorph, providedMorph, pDefMember->Name);
    }

    std::shared_ptr<toka::Type> exprTypeObj =
        checkExpr(pair.second.get(), memberTypeObj);
    memberMasks[pair.first] = m_LastInitMask;

    if (isTypeCompatible(memberTypeObj, exprTypeObj) &&
        !memberTypeObj->equals(*exprTypeObj)) {
      auto origLoc = pair.second->Loc;
      pair.second = std::make_unique<CastExpr>(std::move(pair.second),
                                               memberTypeObj->toString());
      pair.second->Loc = origLoc;
      pair.second->ResolvedType = memberTypeObj;
      exprTypeObj = memberTypeObj;
    }

    bool bypassNullStruct = false;
    if (m_InUnsafeContext && memberTypeObj && memberTypeObj->isRawPointer() && exprTypeObj && exprTypeObj->isNullType()) {
        bypassNullStruct = true;
    }

    if (!bypassNullStruct && !isTypeCompatible(memberTypeObj, exprTypeObj)) {
      error(Init, DiagID::ERR_MEMBER_TYPE_MISMATCH, pair.first,
            memberTypeObj->toString(), exprTypeObj->toString());
    }

    // Lifetime dependency tracking
    std::string cleanName = toka::Type::stripMorphology(pDefMember->Name);
    for (const auto &dep : SD->LifeDependencies) {
      if (dep == cleanName) {
        if (!m_LastBorrowSource.empty()) {
          m_LastLifeDependencies.insert(m_LastBorrowSource);
        }
      }
    }
  }

  // Missing fields check for Struct/Tuple
  for (const auto &defField : SD->Members) {
    if (!providedFields.count(defField.Name) &&
        !providedFields.count("^" + defField.Name) &&
        !providedFields.count("*" + defField.Name) &&
        !providedFields.count("~" + defField.Name) &&
        !providedFields.count("&" + defField.Name) &&
        !providedFields.count("^?" + defField.Name)) {
      
      if (!hasElision) {
        if (defField.DefaultValue) {
           error(Init, "Missing field '" + defField.Name + "' in constructor for '" + Init->ShapeName + "'. Use '..' to explicitly fallback to default values.");
        } else {
           error(Init, "Missing field '" + defField.Name + "' in constructor for '" + Init->ShapeName + "' (no default value)");
        }
        continue;
      }
      
      if (elisionTarget) {
          auto clonedTarget = std::unique_ptr<Expr>(static_cast<Expr*>(elisionTarget->clone().release()));
          auto memExpr = std::make_unique<MemberExpr>(std::move(clonedTarget), defField.Name);
          memExpr->Loc = elisionTarget->Loc;
          
          auto memberTypeObj = defField.ResolvedType;
          if (!memberTypeObj) memberTypeObj = toka::Type::fromString(defField.Type);
          
          std::shared_ptr<toka::Type> exprTypeObj = checkExpr(memExpr.get(), memberTypeObj);
          memberMasks[defField.Name] = m_LastInitMask;

          if (isTypeCompatible(memberTypeObj, exprTypeObj) && !memberTypeObj->equals(*exprTypeObj)) {
            auto origLoc = memExpr->Loc;
            auto castExpr = std::make_unique<CastExpr>(std::move(memExpr), memberTypeObj->toString());
            castExpr->Loc = origLoc;
            castExpr->ResolvedType = memberTypeObj;
            exprTypeObj = memberTypeObj;
            providedFields.insert(defField.Name);
            Init->Members.push_back({defField.Name, std::move(castExpr)});
          } else {
            providedFields.insert(defField.Name);
            Init->Members.push_back({defField.Name, std::move(memExpr)});
          }
          continue;
      }
      
      if (!defField.DefaultValue) {
        error(Init, DiagID::ERR_MISSING_DEFAULT_FOR_ELIDED, defField.Name, Init->ShapeName);
        continue;
      }

      // Inject default value
      auto cloned = std::unique_ptr<Expr>(
          static_cast<Expr *>(defField.DefaultValue->clone().release()));

      auto memberTypeObj = defField.ResolvedType;
      if (!memberTypeObj)
          memberTypeObj = toka::Type::fromString(defField.Type);

        std::shared_ptr<toka::Type> exprTypeObj =
            checkExpr(cloned.get(), memberTypeObj);
        memberMasks[defField.Name] = m_LastInitMask;

        if (isTypeCompatible(memberTypeObj, exprTypeObj) &&
            !memberTypeObj->equals(*exprTypeObj)) {
          auto origLoc = cloned->Loc;
          cloned = std::make_unique<CastExpr>(std::move(cloned),
                                              memberTypeObj->toString());
          cloned->Loc = origLoc;
          cloned->ResolvedType = memberTypeObj;
          exprTypeObj = memberTypeObj;
        }

        bool bypassNullStruct = false;
        if (m_InUnsafeContext && memberTypeObj && memberTypeObj->isRawPointer() && exprTypeObj && exprTypeObj->isNullType()) {
            bypassNullStruct = true;
        }

        if (!bypassNullStruct && !isTypeCompatible(memberTypeObj, exprTypeObj)) {
          error(Init, DiagID::ERR_MEMBER_TYPE_MISMATCH, defField.Name,
                memberTypeObj->toString(), exprTypeObj->toString());
        }
        providedFields.insert(defField.Name);
        Init->Members.push_back({defField.Name, std::move(cloned)});
    }
  }

  if (hasElision) {
    Init->Members.erase(std::remove_if(Init->Members.begin(), Init->Members.end(),
        [](const auto& pair) { return pair.first == ".."; }), Init->Members.end());
  }

  // Mask Calculation
  uint64_t mask = 0;
  for (int i = 0; i < (int)SD->Members.size(); ++i) {
    std::string memName = SD->Members[i].Name;
    for (const auto &pair : Init->Members) {
      if (toka::Type::stripMorphology(pair.first) ==
          toka::Type::stripMorphology(memName)) {
        m_LastInitMask = memberMasks[pair.first];
        std::shared_ptr<Type> memTypeObj =
            toka::Type::fromString(SD->Members[i].Type);
        uint64_t expected = 1;
        if (memTypeObj->isShape()) {
          std::string sName = memTypeObj->getSoulName();
          if (ShapeMap.count(sName)) {
            size_t sz = ShapeMap[sName]->Members.size();
            expected = (sz >= 64) ? ~0ULL : (1ULL << sz) - 1;
          }
        }
        if ((m_LastInitMask & expected) == expected) {
          if (i < 64)
            mask |= (1ULL << i);
        }
        break;
      }
    }
  }
  m_LastInitMask = mask;

  return toka::Type::fromString(resolvedName);
}

std::shared_ptr<toka::Type>
Sema::checkUnionInit(InitStructExpr *Init, ShapeDecl *SD,
                     const std::string &resolvedName,
                     std::map<std::string, uint64_t> &memberMasks) {
  if (Init->Members.empty()) {
    error(Init, DiagID::ERR_MISSING_MEMBER, "at least one variant",
          Init->ShapeName);
    m_LastInitMask = 0;
    return toka::Type::fromString(resolvedName);
  }

  if (Init->Members.size() > 1) {
    error(Init, DiagID::ERR_GENERIC_PARSE,
          "Union '{}' initialization must specify exactly "
          "one variant, but {} were provided.",
          Init->ShapeName, Init->Members.size());
  }

  auto &pair = Init->Members[0];
  bool fieldFound = false;
  const ShapeMember *pDefMember = nullptr;
  for (const auto &defField : SD->Members) {
    if (toka::Type::stripMorphology(defField.Name) ==
        toka::Type::stripMorphology(pair.first)) {
      fieldFound = true;
      pDefMember = &defField;
      break;
    }
  }

  if (!fieldFound) {
    error(Init, DiagID::ERR_NO_SUCH_MEMBER, resolvedName, pair.first);
  } else {
    auto memberTypeObj = pDefMember->ResolvedType;
    if (!memberTypeObj)
      memberTypeObj = toka::Type::fromString(pDefMember->Type);

    // [New] Morphic Exemption: Validating Caller Transparency for Unions
    if (!pDefMember->IsMorphicExempt) {
      MorphKind providedMorph = MorphKind::None;
      if (pair.first.find('^') != std::string::npos) providedMorph = MorphKind::Unique;
      else if (pair.first.find('~') != std::string::npos) providedMorph = MorphKind::Shared;
      else if (pair.first.find('&') != std::string::npos) providedMorph = MorphKind::Ref;
      else if (pair.first.find('*') != std::string::npos) providedMorph = MorphKind::Raw;

      MorphKind expectedMorph = MorphKind::None;
      if (memberTypeObj->isRawPointer()) expectedMorph = MorphKind::Raw;
      else if (memberTypeObj->isUniquePtr()) expectedMorph = MorphKind::Unique;
      else if (memberTypeObj->isSharedPtr()) expectedMorph = MorphKind::Shared;
      else if (memberTypeObj->isReference()) expectedMorph = MorphKind::Ref;

      checkStrictMorphology(Init, expectedMorph, providedMorph, pDefMember->Name);
    }

    std::shared_ptr<toka::Type> exprTypeObj =
        checkExpr(pair.second.get(), memberTypeObj);
    m_LastInitMask = ~0ULL; // Union is fully initialized if one field is set

    bool bypassNullStruct = false;
    if (m_InUnsafeContext && memberTypeObj && memberTypeObj->isRawPointer() && exprTypeObj && exprTypeObj->isNullType()) {
        bypassNullStruct = true;
    }

    if (!bypassNullStruct && !isTypeCompatible(memberTypeObj, exprTypeObj)) {
      error(Init, DiagID::ERR_MEMBER_TYPE_MISMATCH, pair.first,
            memberTypeObj->toString(), exprTypeObj->toString());
    }

    // [Rule] Strict Union Initialization Size Check
    // Ensure initialized member covers the full size of the Union.
    uint64_t unionSize = 0;
    for (auto &m : SD->Members) {
      auto mT = m.ResolvedType ? m.ResolvedType
                               : toka::Type::fromString(resolveType(m.Type));
      uint64_t s = getTypeSize(mT);
      if (s > unionSize)
        unionSize = s;
    }
    uint64_t variantSize = getTypeSize(memberTypeObj);
    if (variantSize < unionSize) {
      DiagnosticEngine::report(getLoc(Init), DiagID::ERR_UNION_PARTIAL_INIT,
                               SD->Name, pair.first, variantSize, unionSize);
      HasError = true;
    }
  }

  m_LastInitMask = ~0ULL;
  auto retTy = std::make_shared<toka::ShapeType>(resolvedName);
  retTy->resolve(SD);
  return retTy;
}

} // namespace toka
