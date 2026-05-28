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
#include "toka/Parser.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace toka {

std::unique_ptr<Stmt> Parser::parseVariableDecl(bool isPub) {
  bool isConst = match(TokenType::KwConst);
  if (!isConst) {
    if (match(TokenType::KwLet)) {
      error(previous(),
            "Deprecated keyword 'let'; use 'auto' for variable declarations.");
    } else if (previous().Kind != TokenType::KwAuto) {
      match(TokenType::KwAuto);
    }
  }

  bool isRef = false;
  bool hasPointer = false;
  bool isUnique = false;
  bool isShared = false;
  bool isPtrNullable = match(TokenType::KwNul); // [New Rule] nul pointer modifier
  bool isRebindable = false;
  bool isRebindBlocked = false;

  std::string morphologyPrefix = "";
  while (true) {
    if (match(TokenType::Ampersand)) {
      isRef = true;
      morphologyPrefix += "&";
      Token tok = previous();
      isRebindable = tok.IsSwappablePtr;
      isPtrNullable = isPtrNullable || tok.HasNull;
      isRebindBlocked = tok.IsBlocked;
      if (isPtrNullable) {
        error(tok, "Borrowed pointers (&) cannot be nullable");
      }
    } else if (match(TokenType::And)) {
      isRef = true;
      morphologyPrefix += "&&";
      Token tok = previous();
      isRebindable = tok.IsSwappablePtr;
      isPtrNullable = isPtrNullable || tok.HasNull;
      isRebindBlocked = tok.IsBlocked;
      if (isPtrNullable) {
        error(tok, "Borrowed pointers (&) cannot be nullable");
      }
    } else if (match(TokenType::Caret)) {
      isUnique = true;
      morphologyPrefix += "^";
      Token tok = previous();
      isRebindable = tok.IsSwappablePtr;
      isPtrNullable = isPtrNullable || tok.HasNull;
      isRebindBlocked = tok.IsBlocked;
    } else if (match(TokenType::Star)) {
      hasPointer = true;
      morphologyPrefix += "*";
      Token tok = previous();
      isRebindable = tok.IsSwappablePtr;
      isPtrNullable = isPtrNullable || tok.HasNull;
      isRebindBlocked = tok.IsBlocked;
    } else if (match(TokenType::Tilde)) {
      isShared = true;
      morphologyPrefix += "~";
      Token tok = previous();
      isRebindable = tok.IsSwappablePtr;
      isPtrNullable = isPtrNullable || tok.HasNull;
      isRebindBlocked = tok.IsBlocked;
    } else {
      break;
    }
  }

  // Check for positional destructuring: let Type(v1, v2) = ... or let (v1, v2)
  // = ...
  if ((check(TokenType::Identifier) && checkAt(1, TokenType::LParen)) ||
      check(TokenType::LParen)) {
    std::string typeName = "";
    if (check(TokenType::Identifier)) {
      typeName = advance().Text;
    }
    consume(TokenType::LParen, "Expected '(' for destructuring");
    std::vector<DestructuredVar> vars;
    bool hasNamed = false;
    bool hasPositional = false;
    while (!check(TokenType::RParen) && !check(TokenType::EndOfFile)) {
      if (match(TokenType::DotDot)) {
        DestructuredVar v;
        v.Name = "..";
        v.FieldName = "..";
        vars.push_back(v);
      } else {
        if (isNextNamedField(0)) {
          hasNamed = true;
          std::string varPrefix = "";
          bool isRef = false;
          if (match(TokenType::Ampersand)) {
            varPrefix = "&";
            isRef = true;
          } else if (match(TokenType::Caret)) {
            varPrefix = "^";
          } else if (match(TokenType::Tilde)) {
            varPrefix = "~";
          } else if (match(TokenType::Star)) {
            varPrefix = "*";
          }

          Token varTok = consume(TokenType::Identifier, "Expected variable name or '_'");
          std::string fullVarName = varPrefix + varTok.Text;

          consume(TokenType::Equal, "Expected '=' after variable name");
          consume(TokenType::Dot, "Expected '.' after '=' in named destructuring");

          std::string fieldPrefix = "";
          if (match(TokenType::Star))
            fieldPrefix = "*";
          else if (match(TokenType::Caret))
            fieldPrefix = "^";
          else if (match(TokenType::Tilde))
            fieldPrefix = "~";
          else if (match(TokenType::Ampersand))
            fieldPrefix = "&";

          if (match(TokenType::TokenNull))
            fieldPrefix += "?";
          if (match(TokenType::TokenWrite))
            fieldPrefix += "#";

          Token fieldNameTok = consume(TokenType::Identifier, "Expected field name");
          std::string fieldName = fieldPrefix + fieldNameTok.Text;
          while (match(TokenType::Minus)) {
            fieldName += "-";
            fieldName += consume(TokenType::Identifier, "Expected identifier after '-'").Text;
          }
          if (fieldNameTok.HasWrite || fieldNameTok.IsBlocked)
            error(fieldNameTok, DiagID::ERR_ILLEGAL_FIELD_MODIFIER);
          
          DestructuredVar v;
          v.Name = fullVarName;
          v.FieldName = fieldName;
          v.IsWildcard = (varTok.Text == "_");
          v.IsValueMutable = varTok.HasWrite;
          v.IsValueNullable = varTok.HasNull;
          v.IsValueBlocked = varTok.IsBlocked;
          v.IsReference = isRef;
          vars.push_back(v);
        } else {
          hasPositional = true;
          std::string varPrefix = "";
          bool isRef = false;
          if (match(TokenType::Ampersand)) {
            varPrefix = "&";
            isRef = true;
          } else if (match(TokenType::Caret)) {
            varPrefix = "^";
          } else if (match(TokenType::Tilde)) {
            varPrefix = "~";
          } else if (match(TokenType::Star)) {
            varPrefix = "*";
          }
          Token varTok = consume(TokenType::Identifier, "Expected variable name or '_'");
          std::string fullVarName = varPrefix + varTok.Text;
          
          DestructuredVar v;
          v.Name = fullVarName;
          v.FieldName = "";
          v.IsWildcard = (varTok.Text == "_");
          v.IsValueMutable = varTok.HasWrite;
          v.IsValueNullable = varTok.HasNull;
          v.IsValueBlocked = varTok.IsBlocked;
          v.IsReference = isRef;
          vars.push_back(v);
        }
      }
      if (!match(TokenType::Comma))
        break;
    }
    if (hasNamed && hasPositional) {
      error(peek(), "Cannot mix positional and named fields in destructuring");
    }
    consume(TokenType::RParen, "Expected ')' after destructuring");
    consume(TokenType::Equal, "Expected '=' for destructuring");
    auto init = parseExpr();
    expectEndOfStatement();
    auto node = std::make_unique<DestructuringDecl>(typeName, std::move(vars),
                                                    std::move(init));
    node->setLocation(previous(),
                      m_CurrentFile); // Use previous (RParen or last consumed)
                                      // as location anchor
    return node;
  }

  bool isMorphicExempt = false;
  Token name = consume(TokenType::Identifier, "Expected variable name");
  if (!name.Text.empty() && name.Text[0] == '\'') {
      isMorphicExempt = true;
      // Do not strip the quote from the name to keep it consistent with
      // function arguments and pattern bindings.
  }
  std::string fullVarName = morphologyPrefix + name.Text;

  std::string typeName = "";
  if (match(TokenType::Colon)) {
    typeName = parseTypeString();
    if (!typeName.empty() && typeName[0] == '\'') {
        isMorphicExempt = true; // [NEW] Generic Morphology Encapsulation
    }
  }

  std::unique_ptr<Expr> init;
  if (match(TokenType::Equal)) {
    init = parseExpr();
  }

  // Use fullVarName uniformly (e.g. `&x` directly as name)
  auto node = std::make_unique<VariableDecl>(fullVarName, std::move(init));
  node->setLocation(name, m_CurrentFile);
  node->IsRawPointer = hasPointer;
  node->IsUnique = isUnique;
  node->IsShared = isShared;
  node->IsReference = isRef;
  node->IsPub = isPub;
  node->IsConst = isConst;
  // node->IsMutable = name.HasWrite; // Deprecated
  // node->IsNullable = name.HasNull; // Deprecated
  // Explicit properties mapping
  node->IsValueMutable = name.HasWrite;
  node->IsValueNullable = name.HasNull;
  node->IsValueBlocked = name.IsBlocked;
  node->IsMorphicExempt = isMorphicExempt; // [NEW]
  node->IsRebindable = isRebindable;
  node->IsPointerNullable = isPtrNullable;
  node->IsRebindBlocked = isRebindBlocked;
  node->TypeName = typeName;

  expectEndOfStatement();
  return node;
}

std::unique_ptr<GuardBindStmt> Parser::parseGuardBindStmt() {
  Token tok = consume(TokenType::KwGuard, "Expected 'guard'");
  consume(TokenType::KwAuto, "Expected 'auto' after 'guard'");
  auto pat = parsePattern();
  consume(TokenType::Equal, "Expected '=' in guard auto statement");
  auto target = parseExpr();
  consume(TokenType::KwElse, "Expected 'else' after guard target expression");
  
  std::unique_ptr<Stmt> elseBody;
  if (check(TokenType::LBrace)) {
      elseBody = parseBlock();
  } else {
      elseBody = parseStmt();
  }
  
  auto node = std::make_unique<GuardBindStmt>(std::move(pat), std::move(target), std::move(elseBody));
  node->setLocation(tok, m_CurrentFile);
  return node;
}

std::unique_ptr<Stmt> Parser::parseStmt() {
  if (check(TokenType::LBrace))
    return parseBlock();
  
  if (check(TokenType::KwGuard) && checkAt(1, TokenType::KwAuto))
    return parseGuardBindStmt();

  if (check(TokenType::KwIf))
    return std::make_unique<ExprStmt>(parseIf());
  if (match(TokenType::KwMatch))
    return std::make_unique<ExprStmt>(parseMatchExpr());
  if (check(TokenType::KwLoop))
    return std::make_unique<ExprStmt>(parseLoop());
  if (check(TokenType::KwFor))
    return std::make_unique<ExprStmt>(parseForExpr());
  if (check(TokenType::KwReturn))
    return parseReturn();
  if (check(TokenType::KwLet) || check(TokenType::KwAuto))
    return parseVariableDecl(false);
  if (check(TokenType::KwDelete))
    return parseDeleteStmt();
  if (check(TokenType::KwUnsafe))
    return parseUnsafeStmt();
  if (check(TokenType::KwFree))
    return parseFreeStmt();
  if (check(TokenType::KwUnreachable))
    return parseUnreachableStmt();

  // ExprStmt
  auto expr = parseExpr();
  if (expr) {
    expectEndOfStatement();
    return std::make_unique<ExprStmt>(std::move(expr));
  }
  return nullptr;
}

std::unique_ptr<BlockStmt> Parser::parseBlock() {
  Token tok = consume(TokenType::LBrace, "Expected '{'");
  auto block = std::make_unique<BlockStmt>();
  block->setLocation(tok, m_CurrentFile);

  while (!check(TokenType::RBrace) && !check(TokenType::EndOfFile)) {
    auto stmt = parseStmt();
    if (stmt)
      block->Statements.push_back(std::move(stmt));
    else
      advance(); // Avoid infinite loop if null
      
    if (PanicMode) {
      synchronize();
    }
  }

  consume(TokenType::RBrace, "Expected '}'");
  return block;
}

std::unique_ptr<ReturnStmt> Parser::parseReturn() {
  Token tok = consume(TokenType::KwReturn, "Expected 'return'");
  std::unique_ptr<Expr> val;
  if (!isEndOfStatement()) {
    val = parseExpr();
  }
  expectEndOfStatement();
  auto node = std::make_unique<ReturnStmt>(std::move(val));
  node->setLocation(tok, m_CurrentFile);
  return node;
}

std::unique_ptr<Stmt> Parser::parseDeleteStmt() {
  Token kw = consume(TokenType::KwDelete, "Expected 'del' or 'delete'");
  auto expr = parseExpr();
  expectEndOfStatement();
  auto node = std::make_unique<DeleteStmt>(std::move(expr));
  node->setLocation(kw, m_CurrentFile);
  return node;
}

std::unique_ptr<Stmt> Parser::parseUnsafeStmt() {
  Token tok = consume(TokenType::KwUnsafe, "Expected 'unsafe'");
  if (check(TokenType::LBrace)) {
    auto block = parseBlock();
    auto node = std::make_unique<UnsafeStmt>(std::move(block));
    node->setLocation(tok, m_CurrentFile);
    return node;
  }
  if (check(TokenType::KwFree)) {
    auto freeStmt = parseFreeStmt();
    auto node = std::make_unique<UnsafeStmt>(std::move(freeStmt));
    node->setLocation(tok, m_CurrentFile);
    return node;
  }
  // Line-level unsafe: unsafe p#[0] = 1
  auto stmt = parseStmt();
  auto node = std::make_unique<UnsafeStmt>(std::move(stmt));
  node->setLocation(tok, m_CurrentFile);
  return node;
}

std::unique_ptr<Stmt> Parser::parseFreeStmt() {
  Token tok = consume(TokenType::KwFree, "Expected 'free'");
  std::unique_ptr<Expr> count = nullptr;
  if (match(TokenType::LBracket)) {
    count = parseExpr();
    consume(TokenType::RBracket, "Expected ']'");
  }
  auto expr = parseExpr();
  expectEndOfStatement();
  auto node = std::make_unique<FreeStmt>(std::move(expr), std::move(count));
  node->setLocation(tok, m_CurrentFile);
  return node;
}
std::unique_ptr<Stmt> Parser::parseUnreachableStmt() {
  Token tok = consume(TokenType::KwUnreachable, "Expected 'unreachable'");
  expectEndOfStatement();
  auto node = std::make_unique<UnreachableStmt>();
  node->setLocation(tok, m_CurrentFile);
  return node;
}

} // namespace toka
