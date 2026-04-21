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
#include "toka/DiagnosticEngine.h"
#include "toka/SourceManager.h"
#include <iostream>

namespace toka {

SourceManager *DiagnosticEngine::SrcMgr = nullptr;
int DiagnosticEngine::ErrorCount = 0;

const char *DiagnosticEngine::getFormatString(DiagID id) {
  switch (id) {
#define DIAG(ID, Level, Code, Msg)                                             \
  case DiagID::ID:                                                             \
    return Msg;
#include "toka/DiagnosticDefs.def"
#undef DIAG
  case DiagID::NUM_DIAGNOSTICS:
    return "Unknown Error";
  default:
    return "Unknown Error";
  }
}

DiagLevel DiagnosticEngine::getLevel(DiagID id) {
  switch (id) {
#define DIAG(ID, Level, Code, Msg)                                             \
  case DiagID::ID:                                                             \
    return DiagLevel::Level;
#include "toka/DiagnosticDefs.def"
#undef DIAG
  case DiagID::NUM_DIAGNOSTICS:
    return DiagLevel::Error;
  default:
    return DiagLevel::Error;
  }
}

const char *DiagnosticEngine::getCode(DiagID id) {
  switch (id) {
#define DIAG(ID, Level, Code, Msg)                                             \
  case DiagID::ID:                                                             \
    return Code;
#include "toka/DiagnosticDefs.def"
#undef DIAG
  case DiagID::NUM_DIAGNOSTICS:
    return "E0000";
  default:
    return "E0000";
  }
}

void DiagnosticEngine::reportImpl(DiagLoc loc, DiagID id,
                                  const std::string &message) {
  DiagLevel level = getLevel(id);

  if (level == DiagLevel::Error) {
    ErrorCount++;
  }

  const char *color = "";
  const char *reset = "\033[0m";

  if (level == DiagLevel::Error) {
    color = "\033[1;31m"; // Red Bold
    std::cerr << color << "error[" << getCode(id) << "]" << reset << ": "
              << message << "\n";
  } else if (level == DiagLevel::Warning) {
    color = "\033[1;33m"; // Yellow Bold
    std::cerr << color << "warning[" << getCode(id) << "]" << reset << ": "
              << message << "\n";
  } else {
    color = "\033[1;36m"; // Cyan Bold
    std::cerr << color << "note" << reset << ": " << message << "\n";
  }

  std::cerr << " --> " << loc.File << ":" << loc.Line << ":" << loc.Col << "\n";

  if (ErrorCount > 20) {
    std::cerr
        << "\033[1;31mfatal:\033[0m too many errors emitted, stopping now.\n";
    exit(1);
  }
}

void DiagnosticEngine::reportImpl(SourceLocation loc, DiagID id,
                                  const std::string &message) {
  if (SrcMgr) {
    FullSourceLoc Full = SrcMgr->getFullSourceLoc(loc);
    DiagLoc DL{Full.FileName, (int)Full.Line, (int)Full.Column};
    
    // Print the basic header first
    reportImpl(DL, id, message);
    
    // Print rich snippet if available
    std::string lineData = SrcMgr->getLineData(Full);
    if (!lineData.empty()) {
      std::string lineNumStr = std::to_string(Full.Line);
      std::string padding(lineNumStr.length() + 1, ' ');
      
      const char *blue = "\033[1;34m";
      const char *reset = "\033[0m";
      const char *caretColor = getLevel(id) == DiagLevel::Warning ? "\033[1;33m" : "\033[1;31m";
      if (getLevel(id) == DiagLevel::Note) caretColor = "\033[1;36m";
      
      std::cerr << padding << blue << "|" << reset << "\n";
      std::cerr << " " << lineNumStr << " " << blue << "|" << reset << " " << lineData << "\n";
      std::cerr << padding << blue << "|" << reset << " ";
      if (Full.Column > 0) {
        std::cerr << std::string(Full.Column - 1, ' ');
      }
      std::cerr << caretColor << "^" << reset << "\n\n";
    }
    
  } else {
    DiagLoc DL{"<unknown>", 0, 0};
    reportImpl(DL, id,
               message + " [RawLoc: " + std::to_string(loc.getRawEncoding()) +
                   "]");
  }
}

} // namespace toka
