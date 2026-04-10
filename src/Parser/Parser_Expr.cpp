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

std::unique_ptr<MatchArm::Pattern> Parser::parsePattern() {
  bool isMut = false;
  bool isRef = false;
  match(TokenType::KwAuto); // skip auto if present

  if (match(TokenType::Ampersand)) {
    isRef = true;
    if (match(TokenType::KwMut))
      isMut = true;
  }

  if (check(TokenType::Integer) || check(TokenType::String) ||
      check(TokenType::KwTrue) || check(TokenType::KwFalse)) {
    auto p = std::make_unique<MatchArm::Pattern>(MatchArm::Pattern::Literal);
    p->Name = advance().Text;
    if (previous().Kind == TokenType::Integer) {
      try {
        p->LiteralVal = std::stoull(previous().Text, nullptr, 0);
      } catch (...) {
        p->LiteralVal = 0;
      }
    }
    return p;
  }

  if (check(TokenType::Identifier)) {
    if (peek().Text == "_") {
      advance();
      return std::make_unique<MatchArm::Pattern>(MatchArm::Pattern::Wildcard);
    }

    Token nameTok = advance();
    std::string name = nameTok.Text;

    if (match(TokenType::Less) || match(TokenType::GenericLT)) {
      name += "<";
      // Manually parse type args simple way or use parseTypeString?
      // parseTypeString consumes identifiers.
      // Let's loop.
      while (true) {
        name += parseTypeString();
        if (match(TokenType::Comma)) {
          name += ",";
        } else {
          break;
        }
      }
      consume(TokenType::Greater, "Expected '>'");
      name += ">";
    }

    // Handle Path::Variant
    if (check(TokenType::Colon) && checkAt(1, TokenType::Colon)) {
      consume(TokenType::Colon, "");
      consume(TokenType::Colon, "");
      name +=
          "::" + consume(TokenType::Identifier, "Expected variant name").Text;
    }

    if (match(TokenType::LParen)) {
      std::vector<std::unique_ptr<MatchArm::Pattern>> subs;
      while (!check(TokenType::RParen) && !check(TokenType::EndOfFile)) {
        subs.push_back(parsePattern());
        if (!check(TokenType::RParen))
          match(TokenType::Comma);
      }
      consume(TokenType::RParen, "Expected ')' after subpatterns");
      auto p = std::make_unique<MatchArm::Pattern>(MatchArm::Pattern::Decons);
      p->Name = name;
      p->SubPatterns = std::move(subs);
      p->IsReference = isRef;
      return p;
    }

    auto p = std::make_unique<MatchArm::Pattern>(MatchArm::Pattern::Variable);
    p->Name = name;
    p->IsReference = isRef;
    p->IsValueMutable = nameTok.HasWrite;
    p->IsValueBlocked = nameTok.IsBlocked;
    return p;
  }

  if (match(TokenType::KwDefault)) {
    return std::make_unique<MatchArm::Pattern>(MatchArm::Pattern::Wildcard);
  }

  error(peek(), "Expected pattern");
  return nullptr;
}

std::unique_ptr<Expr> Parser::parseMatchExpr() {
  Token matchTok = previous(); // KwMatch or peek()
  auto target = parseExpr(0, false);
  consume(TokenType::LBrace, "Expected '{' after match expression");

  std::vector<std::unique_ptr<MatchArm>> arms;
  while (!check(TokenType::RBrace) && !check(TokenType::EndOfFile)) {
    auto pat = parsePattern();
    std::unique_ptr<Expr> guard = nullptr;
    if (match(TokenType::KwIf)) {
      guard = parseExpr();
    }
    consume(TokenType::FatArrow, "Expected '=>'");
    auto body = parseStmt();
    arms.push_back(std::make_unique<MatchArm>(std::move(pat), std::move(guard),
                                              std::move(body)));
  }
  consume(TokenType::RBrace, "Expected '}' after match arms");
  auto matched =
      std::make_unique<MatchExpr>(std::move(target), std::move(arms));
  matched->setLocation(matchTok, m_CurrentFile);
  return matched;
}

std::unique_ptr<Expr> Parser::parseExpr(int minPrec, bool allowTrailingClosure) {
  auto lhs = parsePrimary(allowTrailingClosure);
  if (!lhs)
    return nullptr;

  while (true) {
    if (check(TokenType::Colon) || check(TokenType::KwAs) ||
        (peek().Kind == TokenType::Identifier && peek().Text == "as")) {
      advance(); // consume ':' or 'as'
      std::string typeName = "";
      int depth = 0;
      while (true) {
        Token t = peek();
        if (depth == 0) {
          bool shouldBreak = false;

          if (check(TokenType::Greater) && depth == 0)
            shouldBreak = true;
          if (check(TokenType::Less) && depth == 0)
            shouldBreak = true;
          if ((check(TokenType::Colon) ||
               (peek().Kind == TokenType::Identifier && peek().Text == "as") ||
               check(TokenType::KwAs)) &&
              depth == 0)
            shouldBreak = true;

          if (check(TokenType::Comma) || check(TokenType::RParen) ||
              check(TokenType::RBrace) || isEndOfStatement() ||
              check(TokenType::Equal) || check(TokenType::DoubleEqual) ||
              check(TokenType::Neq) || check(TokenType::KwIs) ||
              check(TokenType::LBrace) || check(TokenType::EndOfFile) ||
              check(TokenType::Plus) || check(TokenType::Minus) ||
              check(TokenType::Slash) || check(TokenType::And) ||
              check(TokenType::Percent) || check(TokenType::Or)) {
            shouldBreak = true;
          }

          if (shouldBreak)
            break;
        }

        t = advance();
        std::string currentText = t.Text;
        if (t.Kind == TokenType::Identifier && !currentText.empty() && currentText[0] == '\'') {
            // currentText = currentText.substr(1);
        }
        typeName += currentText;
        if (currentText == "cede") {
          typeName += " ";
        }
        if (t.Kind == TokenType::LBracket || t.Kind == TokenType::LParen ||
            t.Kind == TokenType::GenericLT)
          depth++;
        else if (t.Kind == TokenType::RBracket || t.Kind == TokenType::RParen ||
                 t.Kind == TokenType::Greater)
          depth--;
      }
      Token tok = previous();
      auto node = std::make_unique<CastExpr>(std::move(lhs), typeName);
      node->setLocation(tok, m_CurrentFile);
      lhs = std::move(node);
      continue;
    } // Closes if (check(KwAs))

    int prec = getPrecedence(peek().Kind);
    if (prec < minPrec)
      break;

    // Rule: Binary operators must be on the previous line to continue
    if (peek().HasNewlineBefore)
      break;

    Token op = advance();
    auto rhs = parseExpr(prec + 1, allowTrailingClosure);
    if (!rhs) {
      std::cerr << "Parser Error: Expected expression after operator\n";
      break;
    }

    auto node =
        std::make_unique<BinaryExpr>(op.Text, std::move(lhs), std::move(rhs));
    node->setLocation(op, m_CurrentFile);
    lhs = std::move(node);
  }

  return lhs;
}

std::unique_ptr<Expr> Parser::parsePrimary(bool allowTrailingClosure) {
  std::unique_ptr<Expr> expr = nullptr;
  if (match(TokenType::Bang) || match(TokenType::Minus) ||
      match(TokenType::PlusPlus) || match(TokenType::MinusMinus) ||
      match(TokenType::Caret) || match(TokenType::Tilde) ||
      match(TokenType::Star) || match(TokenType::Ampersand) ||
      match(TokenType::And) || match(TokenType::At) ||
      match(TokenType::KwBnot)) {
    Token tok = previous();
    TokenType op = tok.Kind;
    auto sub = parsePrimary(allowTrailingClosure);
    if (op == TokenType::And) {
      auto inner = std::make_unique<UnaryExpr>(TokenType::Ampersand, std::move(sub));
      inner->setLocation(tok, m_CurrentFile);
      auto outer = std::make_unique<UnaryExpr>(TokenType::Ampersand, std::move(inner));
      outer->setLocation(tok, m_CurrentFile);
      return outer;
    }
    auto node = std::make_unique<UnaryExpr>(op, std::move(sub));
    node->HasNull = tok.HasNull;
    node->IsRebindable = tok.IsSwappablePtr;
    node->IsValueMutable = tok.HasWrite;
    node->IsValueNullable = tok.HasNull;
    node->IsRebindBlocked = tok.IsBlocked;
    node->IsValueBlocked = tok.IsBlocked;
    node->setLocation(tok, m_CurrentFile);
    
    // [NEW] Enforce Hat Principle for references on member chains
    if (op == TokenType::Ampersand) {
      if (dynamic_cast<MemberExpr*>(node->RHS.get()) && !node->RHS->HasParens) {
        error(tok, "Use of unary '&' on an access chain without parentheses is visually ambiguous. Either wrap in parentheses '&(a.b)' or use hat-terminal morphology 'a.&b'.");
        return nullptr;
      }
    }
    
    return node;
  }

  if (isClosureExpression()) {
    return parseClosureExpr();
  }

  if (match(TokenType::Integer)) {
    Token tok = previous();
    auto node = std::make_unique<NumberExpr>(std::stoull(tok.Text, nullptr, 0));
    node->setLocation(tok, m_CurrentFile);
    expr = std::move(node);
  } else if (match(TokenType::Float)) {
    Token tok = previous();
    auto node = std::make_unique<FloatExpr>(std::stod(tok.Text));
    node->setLocation(tok, m_CurrentFile);
    expr = std::move(node);
  } else if (match(TokenType::KwTrue)) {
    Token tok = previous();
    auto node = std::make_unique<BoolExpr>(true);
    node->setLocation(tok, m_CurrentFile);
    expr = std::move(node);
  } else if (match(TokenType::KwFalse)) {
    Token tok = previous();
    auto node = std::make_unique<BoolExpr>(false);
    node->setLocation(tok, m_CurrentFile);
    expr = std::move(node);
  } else if (match(TokenType::KwNull)) {
    Token tok = previous();
    auto node = std::make_unique<NullExpr>();
    node->setLocation(tok, m_CurrentFile);
    expr = std::move(node);
  } else if (match(TokenType::KwNone)) {
    Token tok = previous();
    auto node = std::make_unique<NoneExpr>();
    node->setLocation(tok, m_CurrentFile);
    expr = std::move(node);
  } else if (match(TokenType::KwFile) || match(TokenType::KwLine) ||
             match(TokenType::KwLoc)) {
    Token tok = previous();
    auto node = std::make_unique<MagicExpr>(tok.Kind);
    node->setLocation(tok, m_CurrentFile);
    expr = std::move(node);
  } else if (match(TokenType::KwUnset)) {
    Token tok = previous();
    auto node = std::make_unique<UnsetExpr>();
    node->setLocation(tok, m_CurrentFile);
    expr = std::move(node);
  } else if (match(TokenType::String)) {
    Token tok = previous();
    auto node = std::make_unique<StringExpr>(tok.Text);
    node->setLocation(tok, m_CurrentFile);
    expr = std::move(node);
  } else if (match(TokenType::CharLiteral)) {
    Token tok = previous();
    char val = 0;
    if (!tok.Text.empty())
      val = tok.Text[0];
    auto node = std::make_unique<CharLiteralExpr>(val);
    node->setLocation(tok, m_CurrentFile);
    expr = std::move(node);
  } else if (match(TokenType::KwIf)) {
    expr = parseIf();
  } else if (match(TokenType::KwGuard)) {
    expr = parseGuard();
  } else if (match(TokenType::KwWhile)) {
    expr = parseWhile();
  } else if (match(TokenType::KwLoop)) {
    expr = parseLoop();
  } else if (match(TokenType::KwFor)) {
    expr = parseForExpr();
  } else if (match(TokenType::KwMatch)) {
    expr = parseMatchExpr();
  } else if (match(TokenType::KwBreak)) {
    expr = parseBreak();
  } else if (match(TokenType::KwContinue)) {
    expr = parseContinue();
  } else if (match(TokenType::KwPass)) {
    expr = parsePass();
  } else if (match(TokenType::KwYield)) {
    // Treat yield as pass for now, or keep it separate if needed
    Token tok = previous();
    auto val = parseExpr();
    auto node = std::make_unique<PassExpr>(std::move(val));
    node->setLocation(tok, m_CurrentFile);
    expr = std::move(node);
  } else if (match(TokenType::KwCede)) {
    Token tok = previous();
    auto val = parseExpr();
    auto node = std::make_unique<CedeExpr>(std::move(val));
    node->setLocation(tok, m_CurrentFile);
    expr = std::move(node);
  } else if (match(TokenType::KwSizeof)) {
    Token tok = previous();
    if (!match(TokenType::LParen)) {
      error(tok, "Expected '(' after sizeof");
      return nullptr;
    }
    auto typeStr = parseTypeString();
    if (!match(TokenType::RParen)) {
      error(previous(), "Expected ')' after sizeof type string");
      return nullptr;
    }
    auto node = std::make_unique<SizeOfExpr>(typeStr);
    node->setLocation(tok, m_CurrentFile);
    expr = std::move(node);
  } else if (match(TokenType::KwSelf)) {
    Token tok = previous();
    auto node = std::make_unique<VariableExpr>("self");
    node->IsValueMutable = tok.HasWrite;
    node->IsValueNullable = tok.HasNull;
    node->IsValueBlocked = tok.IsBlocked;
    node->setLocation(tok, m_CurrentFile);
    expr = std::move(node);
  } else if (match(TokenType::KwUnsafe)) {
    expr = parseUnsafeExpr();
  } else if (match(TokenType::KwAlloc)) {
    expr = parseAllocExpr();
  } else if (match(TokenType::KwNew)) {
    Token kw = previous();
    Token startTok = peek();
    
    // [NEW] Parse Optional Array Scope for `new [N]Type`
    std::unique_ptr<Expr> arraySize = nullptr;
    if (match(TokenType::LBracket)) {
      if (!check(TokenType::RBracket)) {
        arraySize = parseExpr();
      } else {
        // new []T ? Empty length might be inferred later.
      }
      consume(TokenType::RBracket, "Expected ']' after array size");
    }

    std::string typeStr = "";
    if (check(TokenType::Identifier)) {
      typeStr = advance().Text;
      if (!typeStr.empty() && typeStr[0] == '\'') {
          // typeStr = typeStr.substr(1);
      }
      // [NEW] Handle Generics for New Type: Node<i32>
      if (check(TokenType::GenericLT)) {
        typeStr += advance().Text; // <
        int balance = 1;
        while (balance > 0 && !check(TokenType::EndOfFile)) {
          if (check(TokenType::GenericLT))
            balance++;
          else if (check(TokenType::Greater))
            balance--;
          typeStr += advance().Text;
        }
      }

      while (check(TokenType::Colon) && checkAt(1, TokenType::Colon)) {
        advance();
        advance(); // ::
        typeStr += "::";
        typeStr +=
            consume(TokenType::Identifier, "Expected identifier after ::").Text;
      }
    } else {
      error(peek(), "Expected type after 'new'");
      return nullptr;
    }

    std::unique_ptr<Expr> init = nullptr;
    if (check(TokenType::LBrace)) {
      advance(); // LBrace
      std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields;
      while (!check(TokenType::RBrace) && !check(TokenType::EndOfFile)) {
        std::string prefix = "";
        if (match(TokenType::Star))
          prefix = "*";
        else if (match(TokenType::Caret))
          prefix = "^";
        else if (match(TokenType::Tilde))
          prefix = "~";
        else if (match(TokenType::Ampersand))
          prefix = "&";

        Token fieldName = consume(TokenType::Identifier, "Expected field name");
        consume(TokenType::Equal, "Expected '=' after field name");
        fields.push_back({prefix + fieldName.Text, parseExpr()});
        match(TokenType::Comma);
      }
      consume(TokenType::RBrace, "Expected '}'");
      auto node = std::make_unique<InitStructExpr>(typeStr, std::move(fields));
      node->setLocation(startTok, m_CurrentFile);
      init = std::move(node);
    } else if (check(TokenType::LParen)) {
      consume(TokenType::LParen, "Expected '('");

      // Check for named initializer syntax: Type(field = val)
      bool isNamedInit = false;
      if (check(TokenType::Identifier) && checkAt(1, TokenType::Equal)) {
        isNamedInit = true;
      }
      // Also check if first field has prefix: ^field = val
      if (!isNamedInit &&
          (check(TokenType::Caret) || check(TokenType::Star) ||
           check(TokenType::Tilde) || check(TokenType::Ampersand))) {
        if (checkAt(1, TokenType::Identifier) && checkAt(2, TokenType::Equal)) {
          isNamedInit = true;
        }
        // Handle nullable pointer prefix ^?field = val (4 tokens: ^ ? id =)
        if (!isNamedInit && (checkAt(1, TokenType::TokenNull) ||
                             checkAt(1, TokenType::TokenWrite))) {
          if (checkAt(2, TokenType::Identifier) &&
              checkAt(3, TokenType::Equal)) {
            isNamedInit = true;
          }
        }
      }

      if (isNamedInit) {
        std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields;
        while (!check(TokenType::RParen) && !check(TokenType::EndOfFile)) {
          std::string prefix = "";
          if (match(TokenType::Star))
            prefix = "*";
          else if (match(TokenType::Caret))
            prefix = "^";
          else if (match(TokenType::Tilde))
            prefix = "~";
          else if (match(TokenType::Ampersand))
            prefix = "&";

          // Handle secondary prefixes like ? or # if they are separate tokens?
          // Usually prefixes are attached or separate? Lexer treats ^ as Caret.
          // If ^?next is Caret Question Identifier.
          if (match(TokenType::TokenNull))
            prefix += "?";
          if (match(TokenType::TokenWrite))
            prefix += "#";

          Token fieldName =
              consume(TokenType::Identifier, "Expected field name");
          consume(TokenType::Equal, "Expected '=' after field name");
          fields.push_back({prefix + fieldName.Text, parseExpr()});
          if (!check(TokenType::RParen))
            match(TokenType::Comma);
        }
        consume(TokenType::RParen, "Expected ')'");
        auto node =
            std::make_unique<InitStructExpr>(typeStr, std::move(fields));
        node->setLocation(startTok, m_CurrentFile);
        init = std::move(node);
      } else {
        // new Type(...) -> treat as CallExpr for constructor (positional)
        std::vector<std::unique_ptr<Expr>> args;
        if (!check(TokenType::RParen)) {
          do {
            args.push_back(parseExpr());
          } while (match(TokenType::Comma));
        }
        consume(TokenType::RParen, "Expected ')'");
        auto node = std::make_unique<CallExpr>(typeStr, std::move(args));
        node->setLocation(startTok, m_CurrentFile);
        init = std::move(node);
      }
    } else {
      error(kw, "Expected '{' or '(' initializer for new expression");
      return nullptr;
    }
    auto node = std::make_unique<NewExpr>(typeStr, std::move(init), std::move(arraySize));
    node->setLocation(kw, m_CurrentFile);
    expr = std::move(node);
  } else if (match(TokenType::LBracket)) {
    // Array literal [1, 2, 3]
    std::vector<std::unique_ptr<Expr>> elements;
    if (!check(TokenType::RBracket)) {
      elements.push_back(parseExpr());
      if (match(TokenType::Semicolon)) {
        auto count = parseExpr();
        consume(TokenType::RBracket, "Expected ']' after repeat count");
        auto node = std::make_unique<RepeatedArrayExpr>(std::move(elements[0]),
                                                        std::move(count));
        node->setLocation(m_Tokens[m_Pos - 1], m_CurrentFile);
        expr = std::move(node);
        return expr; // Return immediately
      }
      while (match(TokenType::Comma)) {
        elements.push_back(parseExpr());
      }
    }
    consume(TokenType::RBracket, "Expected ']' after array elements");
    if (elements.size() == 1 && check(TokenType::Identifier)) {
      std::unique_ptr<Expr> arraySize = std::move(elements[0]);
      std::string typeStr = advance().Text;
      if (check(TokenType::GenericLT)) {
        typeStr += advance().Text;
        int balance = 1;
        while (balance > 0 && !check(TokenType::EndOfFile)) {
          if (check(TokenType::GenericLT)) balance++;
          else if (check(TokenType::Greater)) balance--;
          typeStr += advance().Text;
        }
      }
      while (check(TokenType::Colon) && checkAt(1, TokenType::Colon)) {
        advance(); advance();
        typeStr += "::";
        typeStr += consume(TokenType::Identifier, "Expected identifier after ::").Text;
      }
      
      std::unique_ptr<Expr> init = nullptr;
      if (check(TokenType::LParen)) {
        consume(TokenType::LParen, "Expected '('");
        bool isNamedInit = false;
        if (check(TokenType::Identifier) && checkAt(1, TokenType::Equal)) isNamedInit = true;
        if (!isNamedInit && (check(TokenType::Caret) || check(TokenType::Star) || check(TokenType::Tilde) || check(TokenType::Ampersand))) {
          if (checkAt(1, TokenType::Identifier) && checkAt(2, TokenType::Equal)) isNamedInit = true;
          if (!isNamedInit && (checkAt(1, TokenType::TokenNull) || checkAt(1, TokenType::TokenWrite))) {
            if (checkAt(2, TokenType::Identifier) && checkAt(3, TokenType::Equal)) isNamedInit = true;
          }
        }
        if (isNamedInit) {
          std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields;
          while (!check(TokenType::RParen) && !check(TokenType::EndOfFile)) {
            std::string prefix = "";
            if (match(TokenType::Star)) prefix = "*";
            else if (match(TokenType::Caret)) prefix = "^";
            else if (match(TokenType::Tilde)) prefix = "~";
            else if (match(TokenType::Ampersand)) prefix = "&";
            
            if (match(TokenType::TokenNull)) prefix += "?";
            if (match(TokenType::TokenWrite)) prefix += "#";
            
            Token fieldName = consume(TokenType::Identifier, "Expected field name");
            consume(TokenType::Equal, "Expected '='");
            fields.push_back({prefix + fieldName.Text, parseExpr()});
            if (!check(TokenType::RParen)) match(TokenType::Comma);
          }
          consume(TokenType::RParen, "Expected ')'");
          auto node = std::make_unique<InitStructExpr>(typeStr, std::move(fields));
          node->setLocation(m_Tokens[m_Pos-1], m_CurrentFile);
          init = std::move(node);
        } else {
          std::vector<std::unique_ptr<Expr>> args;
          if (!check(TokenType::RParen)) {
            do { args.push_back(parseExpr()); } while (match(TokenType::Comma));
          }
          consume(TokenType::RParen, "Expected ')'");
          auto node = std::make_unique<CallExpr>(typeStr, std::move(args));
          node->setLocation(m_Tokens[m_Pos-1], m_CurrentFile);
          init = std::move(node);
        }
      } else {
        error(peek(), "Expected '(' initializer for Array Init expression");
      }
      auto node = std::make_unique<ArrayInitExpr>(typeStr, std::move(init), std::move(arraySize));
      node->setLocation(m_Tokens[m_Pos-1], m_CurrentFile);
      expr = std::move(node);
    } else {
      expr = std::make_unique<ArrayExpr>(std::move(elements));
    }
  } else if (check(TokenType::LParen)) {
    Token tok = peek();

    // Anonymous Record Detection: ( key = val ... )
    bool isAnonRecord = false;
    if (checkAt(1, TokenType::Identifier) && checkAt(2, TokenType::Equal)) {
      isAnonRecord = true;
    }

    consume(TokenType::LParen, "Expected '('");

    if (isAnonRecord) {
      std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields;
      while (!check(TokenType::RParen) && !check(TokenType::EndOfFile)) {
        Token key = consume(TokenType::Identifier, "Expected field name");
        consume(TokenType::Equal, "Expected '='");
        auto val = parseExpr();
        fields.push_back({key.Text, std::move(val)});
        if (!check(TokenType::RParen)) {
          consume(TokenType::Comma, "Expected ',' or ')'");
        }
      }
      consume(TokenType::RParen, "Expected ')'");
      auto node = std::make_unique<AnonymousRecordExpr>(std::move(fields));
      node->setLocation(tok, m_CurrentFile);
      expr = std::move(node);
    } else {
      // Grouping or Tuple
      std::vector<std::unique_ptr<Expr>> elements;
      bool isTuple = false;
      if (!check(TokenType::RParen)) {
        elements.push_back(parseExpr());
        if (match(TokenType::Comma)) {
          isTuple = true;
          while (!check(TokenType::RParen) && !check(TokenType::EndOfFile)) {
            elements.push_back(parseExpr());
            match(TokenType::Comma);
          }
        }
      }
      consume(TokenType::RParen, "Expected ')'");
      if (isTuple || elements.empty()) {
        auto node = std::make_unique<TupleExpr>(std::move(elements));
        node->setLocation(tok, m_CurrentFile);
        expr = std::move(node);
      } else {
        expr = std::move(elements[0]);
        expr->HasParens = true; // [NEW] Track that it was explicitly paren-wrapped
      }
    }
  } else if (match(TokenType::Identifier) || match(TokenType::KwUpperSelf)) {
    Token name = previous();
    // Normalize KwUpperSelf to appear as Identifier "Self" locally if needed,
    // though Token.Text usually holds "Self" anyway.

    // [NEW] Check for Generics <...>
    std::vector<std::string> genericArgs;
    std::string genericSuffix = "";
    if (check(TokenType::GenericLT)) {
      match(TokenType::GenericLT); // consume <
      genericSuffix += "<";
      do {
        std::string ty = parseTypeString();
        genericArgs.push_back(ty);
        genericSuffix += ty;
        if (check(TokenType::Comma)) {
          genericSuffix += ", ";
        }
      } while (match(TokenType::Comma));
      consume(TokenType::Greater, "Expected '>'");
      genericSuffix += ">";
    }

    if (match(TokenType::LParen)) {
      // Function Call or Named Struct Init: Type(...) or Type(x=1)
      bool isNamed = false;
      if (check(TokenType::Identifier) && checkAt(1, TokenType::Equal)) {
        isNamed = true;
      }
      // Also check for prefixes if any (Toka attributes on fields)
      if (!isNamed &&
          (check(TokenType::Star) || check(TokenType::Caret) ||
           check(TokenType::Tilde) || check(TokenType::Ampersand))) {
        if (checkAt(1, TokenType::Identifier) && checkAt(2, TokenType::Equal)) {
          isNamed = true;
        }
      }

      if (isNamed) {
        std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields;
        while (!check(TokenType::RParen) && !check(TokenType::EndOfFile)) {
          std::string prefix = "";
          if (match(TokenType::Star))
            prefix = "*";
          else if (match(TokenType::Caret))
            prefix = "^";
          else if (match(TokenType::Tilde))
            prefix = "~";
          else if (match(TokenType::Ampersand))
            prefix = "&";

          Token fieldName =
              consume(TokenType::Identifier, "Expected field name");
          consume(TokenType::Equal, "Expected '=' after field name");
          fields.push_back({prefix + fieldName.Text, parseExpr()});
          if (!check(TokenType::RParen))
            match(TokenType::Comma);
        }
        consume(TokenType::RParen, "Expected ')' after arguments");
        expr = std::make_unique<InitStructExpr>(name.Text + genericSuffix,
                                                std::move(fields));
        expr->setLocation(name, m_CurrentFile);
      } else {
        std::vector<std::unique_ptr<Expr>> args;
        if (!check(TokenType::RParen)) {
          do {
            args.push_back(parseExpr());
          } while (match(TokenType::Comma));
        }
        consume(TokenType::RParen, "Expected ')' after arguments");
        auto node =
            std::make_unique<CallExpr>(name.Text, std::move(args), genericArgs);
        node->setLocation(name, m_CurrentFile);
        expr = std::move(node);
      }
    } else {
      // Check for Scope Resolution (State::On)
      if (check(TokenType::Colon) && checkAt(1, TokenType::Colon)) {
        consume(TokenType::Colon, "");
        consume(TokenType::Colon, "");

        Token member;
        if (check(TokenType::Identifier)) {
          member = consume(TokenType::Identifier, "Expected member after ::");
        } else if (check(TokenType::KwNew)) {
          member = advance();
          member.Kind = TokenType::Identifier; // Treat as identifier
        } else {
          // Fallback for other potential keywords?
          error(peek(), "Expected member identifier or 'new' after ::");
          return nullptr;
        }

        auto obj = std::make_unique<VariableExpr>(name.Text + genericSuffix);
        obj->setLocation(name, m_CurrentFile);
        expr = std::make_unique<MemberExpr>(std::move(obj), member.Text, false,
                                            true);
        expr->setLocation(name, m_CurrentFile);

        if (match(TokenType::LParen)) {
          // Function Call on Member
          std::vector<std::unique_ptr<Expr>> args;
          if (!check(TokenType::RParen)) {
            do {
              args.push_back(parseExpr());
            } while (match(TokenType::Comma));
          }
          consume(TokenType::RParen, "Expected ')' after arguments");
          // Static method call e.g. Box<T>::new()
          // We treat "Box<T>::new" as function name??
          // AST says CallExpr(Callee). Callee is string.
          // GenericArgs? If static method itself is generic?
          // Box<T>::method<U>() Here we are inside Scope Resolution block.
          // MemberExpr handles "Box<T>::new".
          // But MemberExpr is "Object . Member". Object is
          // "Box<T>"(VariableExpr). We return specialized CallExpr for static
          // call? Line 561 original: CallExpr(name + "::" + member, args). With
          // generic suffix: CallExpr(name + suffix + "::" + member, args). Does
          // this support method-level generics? No logic here supports
          // `method<U>`. If we want `Type::method<U>()`, we need `parseExpr` to
          // handle it? Suffix loop handles `.member`. This block handles `::`
          // immediately after identifier. So `Type::method` is handled here. If
          // `method` has generics, we aren't parsing them here! We consumed
          // `member` (identifier). We immediately check LParen. We miss `<U>`
          // here! But that's a separate issue. For now, preserve existing
          // behavior + suffix.

          auto node = std::make_unique<CallExpr>(
              name.Text + genericSuffix + "::" + member.Text, std::move(args));
          // Note: Static method calls on generic types don't support explicit
          // method generics yet in this parser logic
          node->setLocation(name, m_CurrentFile);
          expr = std::move(node);
          return expr;
        }
        return expr;
      }

      auto var = std::make_unique<VariableExpr>(name.Text + genericSuffix);
      var->setLocation(name, m_CurrentFile);
      // var->IsMutable = name.HasWrite; // Deprecated
      // var->IsNullable = name.HasNull; // Deprecated
      var->IsValueMutable = name.HasWrite;
      var->IsValueNullable = name.HasNull;
      var->IsValueBlocked = name.IsBlocked;
      expr = std::move(var);
    }
  } else if (match(TokenType::Dot)) {
    // Check if it's .a to .z for implicit closure parameter
    if (!check(TokenType::Identifier)) {
        error(peek(), "Expected implicit parameter name 'a'-'z'");
        return nullptr;
    }
    Token member = advance();
    if (member.Text.length() == 1 && member.Text[0] >= 'a' && member.Text[0] <= 'z') {
        int index = member.Text[0] - 'a';
        if (index > m_CurrentClosureMaxImplicitArg) m_CurrentClosureMaxImplicitArg = index;
        expr = std::make_unique<VariableExpr>("_arg" + std::to_string(index));
        expr->setLocation(member, m_CurrentFile);
    } else {
        error(member, "Invalid implicit parameter. Expected '.a' to '.z'");
        return nullptr;
    }
  } else {
    error(peek(), "Expected expression");
    return nullptr;
  }

  // Suffixes: .member, [index], etc.
  while (expr) {
    if (match(TokenType::Dot)) {
      Token dotTok = previous();

      std::string prefix = "";
      if (match(TokenType::Star))
        prefix = previous().Text;
      else if (match(TokenType::Caret))
        prefix = previous().Text;
      else if (match(TokenType::Tilde))
        prefix = previous().Text;
      else if (match(TokenType::Ampersand))
        prefix = previous().Text;
      else if (match(TokenType::DoubleQuestion))
        prefix = previous().Text;

      if (match(TokenType::Identifier) || match(TokenType::KwUnset) ||
          match(TokenType::KwNull) || match(TokenType::KwSelf)) {
        std::string memberName = prefix + previous().Text;
        if (memberName == "start" && !check(TokenType::LParen)) {
          auto node = std::make_unique<StartExpr>(std::move(expr));
          node->setLocation(dotTok, m_CurrentFile);
          expr = std::move(node);
          continue;
        }
        // Method Call check
        if (match(TokenType::LParen)) {
          std::vector<std::unique_ptr<Expr>> args;
          if (!check(TokenType::RParen)) {
            do {
              args.push_back(parseExpr());
            } while (match(TokenType::Comma));
          }
          consume(TokenType::RParen, "Expected ')' after method arguments");
          auto node = std::make_unique<MethodCallExpr>(
              std::move(expr), memberName, std::move(args));
          node->setLocation(dotTok, m_CurrentFile);
          expr = std::move(node);
        } else {
          // Member Access
          auto node = std::make_unique<MemberExpr>(std::move(expr), memberName);
          node->setLocation(dotTok, m_CurrentFile);

          Token nameTok =
              previous(); // The identifier matched at loop start or later?
          // Wait, 'prefix + previous().Text' was used. previous() is the
          // identifier.
          if (nameTok.HasWrite) {
            auto wrapper = std::make_unique<PostfixExpr>(TokenType::TokenWrite,
                                                         std::move(node));
            wrapper->setLocation(nameTok, m_CurrentFile);
            expr = std::move(wrapper);
          } else if (nameTok.HasNull) {
            auto wrapper = std::make_unique<PostfixExpr>(TokenType::TokenNull,
                                                         std::move(node));
            wrapper->setLocation(nameTok, m_CurrentFile);
            expr = std::move(wrapper);
          } else {
            expr = std::move(node);
          }
        }
      } else if (match(TokenType::KwAwait)) {
        Token opTok = previous();
        auto node = std::make_unique<AwaitExpr>(std::move(expr));
        node->setLocation(opTok, m_CurrentFile);
        expr = std::move(node);
      } else if (match(TokenType::KwWait)) {
        Token opTok = previous();
        auto node = std::make_unique<WaitExpr>(std::move(expr));
        node->setLocation(opTok, m_CurrentFile);
        expr = std::move(node);
      } else if (prefix.empty() && match(TokenType::Integer)) {
        auto node =
            std::make_unique<MemberExpr>(std::move(expr), previous().Text);
        node->setLocation(dotTok, m_CurrentFile);
        expr = std::move(node);
      } else {
        error(peek(), "Expected member name or index after '.'");
      }
    } else if (match(TokenType::Arrow)) {
      // [Abolished] Arrow syntax for member access is removed.
      // Use implicit dereference via dot (.) instead.
      error(previous(),
            "Arrow '->' member access is abolished. Use dot '.' instead.");
      return nullptr;
    } else if (match(TokenType::LBracket)) {
      std::vector<std::unique_ptr<Expr>> indices;
      if (!check(TokenType::RBracket)) {
        do {
          indices.push_back(parseExpr());
        } while (match(TokenType::Comma));
      }
      consume(TokenType::RBracket, "Expected ']' after index");
      expr =
          std::make_unique<ArrayIndexExpr>(std::move(expr), std::move(indices));
    } else if (match(TokenType::PlusPlus)) {
      expr =
          std::make_unique<PostfixExpr>(TokenType::PlusPlus, std::move(expr));
    } else if (match(TokenType::MinusMinus)) {
      expr =
          std::make_unique<PostfixExpr>(TokenType::MinusMinus, std::move(expr));
    } else if (match(TokenType::DoubleQuestion)) {
      Token opTok = previous();
      auto node = std::make_unique<PostfixExpr>(TokenType::DoubleQuestion,
                                                std::move(expr));
      node->setLocation(opTok, m_CurrentFile);
      expr = std::move(node);
    } else if (match(TokenType::TokenWrite)) {
      Token opTok = previous();
      auto node =
          std::make_unique<PostfixExpr>(TokenType::TokenWrite, std::move(expr));
      node->setLocation(opTok, m_CurrentFile);
      expr = std::move(node);
    } else if (match(TokenType::TokenNull)) {
      Token opTok = previous();
      auto node =
          std::make_unique<PostfixExpr>(TokenType::TokenNull, std::move(expr));
      node->setLocation(opTok, m_CurrentFile);
      expr = std::move(node);
    } else if (match(TokenType::TokenNone)) {
      Token opTok = previous();
      auto node =
          std::make_unique<PostfixExpr>(TokenType::TokenNone, std::move(expr));
      node->setLocation(opTok, m_CurrentFile);
      expr = std::move(node);
    } else if (allowTrailingClosure && !isEndOfStatement() && isClosureExpression()) {
      // Trailing Closure Syntax
      auto clo = parseClosureExpr();
      if (auto *call = dynamic_cast<CallExpr*>(expr.get())) {
          call->Args.push_back(std::move(clo));
      } else if (auto *mcall = dynamic_cast<MethodCallExpr*>(expr.get())) {
          mcall->Args.push_back(std::move(clo));
      } else if (auto *member = dynamic_cast<MemberExpr*>(expr.get())) {
          std::vector<std::unique_ptr<Expr>> args;
          args.push_back(std::move(clo));
          auto newCall = std::make_unique<MethodCallExpr>(std::move(member->Object), member->Member, std::move(args));
          newCall->Loc = member->Loc;
          expr = std::move(newCall);
      } else if (auto *var = dynamic_cast<VariableExpr*>(expr.get())) {
          std::vector<std::unique_ptr<Expr>> args;
          args.push_back(std::move(clo));
          auto newCall = std::make_unique<CallExpr>(var->Name, std::move(args));
          newCall->Loc = var->Loc;
          expr = std::move(newCall);
      } else {
          error(peek(), "Trailing closure applied to invalid expression type");
          return nullptr;
      }
    } else {
      break;
    }
  }

  return expr;
}

std::unique_ptr<Expr> Parser::parseUnsafeExpr() {
  Token tok = previous(); // assume KwUnsafe matched
  auto expr = parseExpr();
  auto node = std::make_unique<UnsafeExpr>(std::move(expr));
  node->setLocation(tok, m_CurrentFile);
  return node;
}

std::unique_ptr<Expr> Parser::parseAllocExpr() {
  Token tok = previous(); // assume KwAlloc matched
  bool isArray = false;
  std::unique_ptr<Expr> arraySize = nullptr;

  if (match(TokenType::LBracket)) {
    isArray = true;
    arraySize = parseExpr();
    consume(TokenType::RBracket, "Expected ']'");
  }

  Token typeTok =
      consume(TokenType::Identifier, "Expected type name after 'alloc'");
  std::string typeName = typeTok.Text;
  if (!typeName.empty() && typeName[0] == '\'') {
      // typeName = typeName.substr(1);
  }

  // [NEW] Handle Generics for Alloc Type: RcWrapper<T>
  if (check(TokenType::GenericLT)) {
    typeName += advance().Text; // <
    int balance = 1;
    while (balance > 0 && !check(TokenType::EndOfFile)) {
      if (check(TokenType::GenericLT))
        balance++;
      else if (check(TokenType::Greater))
        balance--;
      typeName += advance().Text;
    }
  }

  std::unique_ptr<Expr> init = nullptr;
  if (match(TokenType::LParen)) {
    // Check if it's named field initialization: Hero(id = 1, hp = 2)
    if (check(TokenType::Identifier) && checkAt(1, TokenType::Equal)) {
      std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields;
      while (!check(TokenType::RParen) && !check(TokenType::EndOfFile)) {
        std::string prefix = "";
        if (match(TokenType::Star))
          prefix = "*";
        else if (match(TokenType::Caret))
          prefix = "^";
        else if (match(TokenType::Tilde))
          prefix = "~";
        else if (match(TokenType::Ampersand))
          prefix = "&";

        Token fieldName = consume(TokenType::Identifier, "Expected field name");
        consume(TokenType::Equal, "Expected '=' after field name");
        fields.push_back({prefix + fieldName.Text, parseExpr()});
        match(TokenType::Comma);
      }
      init = std::make_unique<InitStructExpr>(typeName, std::move(fields));
    } else if (!check(TokenType::RParen)) {
      // Positional args
      std::vector<std::unique_ptr<Expr>> args;
      do {
        args.push_back(parseExpr());
      } while (match(TokenType::Comma));
      init = std::make_unique<CallExpr>(typeName, std::move(args));
    }
    consume(TokenType::RParen, "Expected ')'");
  }

  auto node = std::make_unique<AllocExpr>(typeName, std::move(init), isArray,
                                          std::move(arraySize));
  node->setLocation(tok, m_CurrentFile);
  return node;
}

std::unique_ptr<Expr> Parser::parseIf() {
  Token tok = previous(); // consumed by match(KwIf)
  if (tok.Kind != TokenType::KwIf)
    tok = consume(TokenType::KwIf, "Expected 'if'");
  bool hasParen = match(TokenType::LParen);
  auto cond = parseExpr(0, false);
  if (hasParen)
    consume(TokenType::RParen, "Expected ')'");
  auto thenStmt = parseStmt();
  std::unique_ptr<Stmt> elseStmt;
  if (match(TokenType::KwElse)) {
    elseStmt = parseStmt();
  }
  auto node = std::make_unique<IfExpr>(std::move(cond), std::move(thenStmt),
                                       std::move(elseStmt));
  node->setLocation(tok, m_CurrentFile);
  return node;
}

std::unique_ptr<Expr> Parser::parseGuard() {
  Token tok = previous(); // consumed by match(KwGuard)
  if (tok.Kind != TokenType::KwGuard)
    tok = consume(TokenType::KwGuard, "Expected 'guard'");
  auto cond = parseExpr(0, false);
  
  auto thenStmt = parseBlock();
  std::unique_ptr<Stmt> elseStmt = nullptr;
  
  if (match(TokenType::KwElse)) {
    if (check(TokenType::LBrace)) {
      elseStmt = parseBlock();
    } else {
      elseStmt = parseStmt();
    }
  }
  
  auto node = std::make_unique<GuardExpr>(std::move(cond), std::move(thenStmt),
                                          std::move(elseStmt));
  node->setLocation(tok, m_CurrentFile);
  return node;
}

std::unique_ptr<Expr> Parser::parseWhile() {
  Token tok = previous();
  if (tok.Kind != TokenType::KwWhile)
    tok = consume(TokenType::KwWhile, "Expected 'while'");
  bool hasParen = match(TokenType::LParen);
  auto cond = parseExpr(0, false);
  if (hasParen)
    consume(TokenType::RParen, "Expected ')'");
  auto body = parseStmt();
  std::unique_ptr<Stmt> elseBody;
  if (match(TokenType::KwOr)) {
    elseBody = parseBlock();
  }
  auto node = std::make_unique<WhileExpr>(std::move(cond), std::move(body),
                                          std::move(elseBody));
  node->setLocation(tok, m_CurrentFile);
  return node;
}

std::unique_ptr<Expr> Parser::parseLoop() {
  Token tok = previous();
  if (tok.Kind != TokenType::KwLoop)
    tok = consume(TokenType::KwLoop, "Expected 'loop'");
  auto body = parseStmt();
  auto node = std::make_unique<LoopExpr>(std::move(body));
  node->setLocation(tok, m_CurrentFile);
  return node;
}

std::unique_ptr<Expr> Parser::parseForExpr() {
  Token tok = previous();
  if (tok.Kind != TokenType::KwFor)
    tok = consume(TokenType::KwFor, "Expected 'for'");

  bool isMut = match(TokenType::KwLet);
  if (!isMut)
    match(TokenType::KwAuto); // Allow optional auto
  std::string morphologyPrefix = "";
  bool isRef = false;
  while (true) {
    if (match(TokenType::Ampersand)) {
      isRef = true;
      morphologyPrefix += "&";
    } else if (match(TokenType::And)) {
      isRef = true;
      morphologyPrefix += "&&";
    } else if (match(TokenType::Caret)) {
      morphologyPrefix += "^";
    } else if (match(TokenType::Star)) {
      morphologyPrefix += "*";
    } else if (match(TokenType::Tilde)) {
      morphologyPrefix += "~";
    } else {
      break;
    }
  }
  Token varName =
      consume(TokenType::Identifier, "Expected variable name in for");
  consume(TokenType::KwIn, "Expected 'in' in for loop");
  auto collection = parseExpr(0, false);
  auto body = parseStmt();
  std::unique_ptr<Stmt> elseBody;
  if (match(TokenType::KwOr)) {
    elseBody = parseBlock();
  }
  auto node = std::make_unique<ForExpr>(varName.Text, isRef, isMut,
                                        std::move(collection), std::move(body),
                                        std::move(elseBody));
  node->MorphologyPrefix = morphologyPrefix;
  node->setLocation(tok, m_CurrentFile);
  return node;
}

std::unique_ptr<Expr> Parser::parseBreak() {
  Token tok = previous();
  std::string label = "";
  if (match(TokenType::KwTo)) {
    label = consume(TokenType::Identifier, "Expected label after 'to'").Text;
  }
  std::unique_ptr<Expr> val;
  if (!isEndOfStatement() && !check(TokenType::RBrace)) {
    val = parseExpr();
  }
  auto node = std::make_unique<BreakExpr>(label, std::move(val));
  node->setLocation(tok, m_CurrentFile);
  return node;
}

std::unique_ptr<Expr> Parser::parseContinue() {
  Token tok = previous();
  std::string label = "";
  if (match(TokenType::KwTo)) {
    label = consume(TokenType::Identifier, "Expected label after 'to'").Text;
  }
  auto node = std::make_unique<ContinueExpr>(label);
  node->setLocation(tok, m_CurrentFile);
  return node;
}

std::unique_ptr<Expr> Parser::parsePass() {
  Token tok = previous();
  auto val = parseExpr();
  auto node = std::make_unique<PassExpr>(std::move(val));
  node->setLocation(tok, m_CurrentFile);
  return node;
}

bool Parser::isClosureExpression() {
  if (check(TokenType::LBrace)) return true;
  
  return false;
}

std::unique_ptr<Expr> Parser::parseClosureExpr() {
  auto expr = std::make_unique<ClosureExpr>();
  expr->setLocation(peek(), m_CurrentFile);

  Token braceTok = consume(TokenType::LBrace, "Expected '{' for closure body");
  expr->Body = std::make_unique<BlockStmt>();
  expr->Body->setLocation(braceTok, m_CurrentFile);
  expr->ReturnType = "unknown";

  // Lookahead to find '=>'
  int lookahead = 0;
  bool hasArrow = false;
  
  if (peekAt(lookahead).Kind == TokenType::LBracket && (peekAt(lookahead + 1).Kind == TokenType::KwCede || peekAt(lookahead + 1).Kind == TokenType::KwCopy)) {
      lookahead++; // '['
      while (peekAt(lookahead).Kind != TokenType::RBracket && peekAt(lookahead).Kind != TokenType::EndOfFile) {
          lookahead++;
      }
      if (peekAt(lookahead).Kind == TokenType::RBracket) lookahead++;
  }

  while (true) {
    TokenType t = peekAt(lookahead).Kind;
    if (t == TokenType::FatArrow) {
      hasArrow = true;
      break;
    }
    if (t == TokenType::EndOfFile) break;
    if (t != TokenType::Identifier && t != TokenType::Comma && t != TokenType::Ampersand && t != TokenType::Caret && t != TokenType::Tilde && t != TokenType::Star && t != TokenType::TokenNull && t != TokenType::TokenWrite) {
      break;
    }
    lookahead++;
  }

  if (hasArrow) {
    // 1. Optional Captures inside the braces
    if (match(TokenType::LBracket)) {
      while (!check(TokenType::RBracket) && !check(TokenType::EndOfFile)) {
         CaptureItem cap;
         cap.Loc = peek().Loc;
         if (match(TokenType::KwCede)) cap.Mode = CaptureMode::ExplicitCede;
         else if (match(TokenType::KwCopy)) cap.Mode = CaptureMode::ExplicitCopy;
         else { error(peek(), "Expected 'cede' or 'copy' modifier in closure capture list. Implicit variables do not need to be declared."); return nullptr; }
         
         std::string prefix = "";
         if (match(TokenType::Tilde)) prefix = "~";
         else if (match(TokenType::Caret)) prefix = "^";
         else if (match(TokenType::Star)) prefix = "*";
         else if (match(TokenType::Ampersand)) prefix = "&";

         if (match(TokenType::TokenNull)) prefix += "?";
         if (match(TokenType::TokenWrite)) prefix += "#";

         cap.Name = prefix + consume(TokenType::Identifier, "Expected variable name to capture").Text;
         
         expr->ExplicitCaptures.push_back(cap);
         if (!check(TokenType::RBracket)) consume(TokenType::Comma, "Expected ',' in capture list");
      }
      consume(TokenType::RBracket, "Expected ']' to end capture list");
    }

    // 2. Explicit typed parameters
    while (!check(TokenType::FatArrow) && !check(TokenType::EndOfFile)) {
      // skip basic sigils if user puts them
      match(TokenType::Caret); match(TokenType::Tilde); match(TokenType::Star); match(TokenType::Ampersand);
      match(TokenType::TokenNull); match(TokenType::TokenWrite);
      if (check(TokenType::FatArrow)) break; // handle zero explicit args `{ [cede x] => ... }`
      Token name = consume(TokenType::Identifier, "Expected parameter name");
      expr->ArgNames.push_back(name.Text);
      if (!check(TokenType::FatArrow)) {
        consume(TokenType::Comma, "Expected ',' between parameter names");
      }
    }
    consume(TokenType::FatArrow, "Expected '=>' after closure parameters");
  }

  if (!hasArrow && check(TokenType::LBracket) && (checkAt(1, TokenType::KwCede) || checkAt(1, TokenType::KwCopy))) {
      error(peek(), "Expected '=>' after closure capture list");
      return nullptr;
  }

  int oldMax = m_CurrentClosureMaxImplicitArg;
  m_CurrentClosureMaxImplicitArg = -1;

  // Parse the rest of the block
  while (!check(TokenType::RBrace) && !check(TokenType::EndOfFile)) {
    auto stmt = parseStmt();
    if (stmt) {
      expr->Body->Statements.push_back(std::move(stmt));
    } else {
      advance();
    }
  }
  consume(TokenType::RBrace, "Expected '}'");

  if (!hasArrow && m_CurrentClosureMaxImplicitArg == -1) {
      error(braceTok, "Zero-argument closures must use '{ => ... }' to disambiguate from code blocks");
  }

  if (!expr->ArgNames.empty()) {
      expr->HasExplicitArgs = true;
  } else if (hasArrow && m_CurrentClosureMaxImplicitArg == -1) {
      expr->HasExplicitArgs = true;
  } else {
      expr->HasExplicitArgs = false;
  }

  expr->MaxImplicitArgIndex = m_CurrentClosureMaxImplicitArg;
  m_CurrentClosureMaxImplicitArg = oldMax;

  // Implicit Return Transformation:
  // If the last statement is an ExprStmt, convert it into a ReturnStmt.
  if (!expr->Body->Statements.empty()) {
      auto* lastStmt = expr->Body->Statements.back().get();
      if (auto* exprStmt = dynamic_cast<ExprStmt*>(lastStmt)) {
          auto retStmt = std::make_unique<ReturnStmt>(std::move(exprStmt->Expression));
          retStmt->Loc = exprStmt->Loc;
          expr->Body->Statements.back() = std::move(retStmt);
      }
  }

  return expr;
}

} // namespace toka
