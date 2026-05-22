#include <emscripten.h>
#include "toka/Lexer.h"
#include "toka/Parser.h"
#include "toka/Sema.h"
#include "toka/SourceManager.h"
#include "toka/DiagnosticEngine.h"
#include <string>
#include <sstream>
#include <iostream>

bool g_JsonDiagnostics = false;

// Intercept std::cout to capture JSON output
class StringbufStream : public std::stringbuf {
public:
    std::string getString() {
        std::string s = this->str();
        this->str(""); // clear
        return s;
    }
};


#include <fstream>
#include <vector>
#include <set>
#include <memory>

void parseSource(const std::string &filename,
                 std::vector<std::unique_ptr<toka::Module>> &astModules,
                 std::set<std::string> &visited,
                 std::vector<std::string> &recursionStack,
                 toka::SourceManager &sm,
                 const std::vector<std::string> &searchPaths,
                 const std::string &code_cstr = "") {
  if (visited.count(filename)) return;
  visited.insert(filename);
  recursionStack.push_back(filename);

  std::string resolvedPath = filename;
  bool found = false;

  auto getBasename = [](const std::string &path) -> std::string {
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash == std::string::npos) return path;
    return path.substr(lastSlash + 1);
  };

  bool isStdOrCore = (filename.rfind("std/", 0) == 0) || (filename.rfind("std\\", 0) == 0) ||
                     (filename.rfind("core/", 0) == 0) || (filename.rfind("core\\", 0) == 0);

  bool isCompilingBuildSystem = false;
  if (!recursionStack.empty()) {
    std::string rootBasename = getBasename(recursionStack[0]);
    if (rootBasename == "build.tk" || rootBasename == "Project.tk") {
      isCompilingBuildSystem = true;
    }
  } else {
    std::string rootBasename = getBasename(filename);
    if (rootBasename == "build.tk" || rootBasename == "Project.tk") {
      isCompilingBuildSystem = true;
    }
  }

  if (!code_cstr.empty()) {
      found = true;
  } else {
      bool tryExact = std::ifstream(filename).good();
      bool tryTk = (filename.find(".tk") == std::string::npos && std::ifstream(filename + ".tk").good());
      
      if (isStdOrCore) {
          tryExact = false;
          tryTk = false;
      }
      if (isCompilingBuildSystem && recursionStack.size() > 1) {
          if (getBasename(filename) == "build.tk" || getBasename(filename) == "Project.tk") {
              tryExact = false;
          }
          if (getBasename(filename + ".tk") == "build.tk" || getBasename(filename + ".tk") == "Project.tk") {
              tryTk = false;
          }
      }
      
      if (tryExact) {
          found = true;
      } else if (tryTk) {
          resolvedPath = filename + ".tk";
          found = true;
      }
  }

  if (!found) {
      std::vector<std::string> pathsToTry = {"lib/", "../lib/"};
      for (const auto &p : searchPaths) {
          pathsToTry.push_back(p.back() == '/' ? p : p + "/");
      }
      for (const auto &p : pathsToTry) {
          std::string libPath = p + filename;
          if (std::ifstream(libPath).good()) {
              resolvedPath = libPath; found = true; break;
          }
          if (filename.find(".tk") == std::string::npos) {
              libPath += ".tk";
              if (std::ifstream(libPath).good()) {
                  resolvedPath = libPath; found = true; break;
              }
          }
      }
  }

  if (!found) {
      toka::DiagnosticEngine::report(toka::DiagLoc{}, toka::DiagID::ERR_FILE_IO, "Could not open file: " + filename);
      return;
  }

  toka::SourceLocation startLoc = code_cstr.empty() ? sm.loadFile(resolvedPath) : sm.addFile(resolvedPath, code_cstr);
  if (startLoc.isInvalid()) return;
  std::string code(sm.getBufferData(startLoc));

  toka::Lexer lexer(code.c_str(), startLoc);
  auto tokens = lexer.tokenize();

  toka::Parser parser(tokens, resolvedPath);
  auto module = parser.parseModule();

  if (!module) return;

  for (const auto &imp : module->Imports) {
      std::string importPath = imp->PhysicalPath;
      if (importPath.rfind("./", 0) == 0 || importPath.rfind("../", 0) == 0) {
          size_t lastSlash = resolvedPath.find_last_of('/');
          std::string parentDir = (lastSlash == std::string::npos) ? "." : resolvedPath.substr(0, lastSlash);
          importPath = parentDir + "/" + importPath;
      }
      parseSource(importPath, astModules, visited, recursionStack, sm, searchPaths);
  }

  astModules.push_back(std::move(module));
  recursionStack.pop_back();
}

extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* check_toka_code(const char* code_cstr) {
    static std::string lastResult;
    
    // Enable JSON output
    g_JsonDiagnostics = true;
    
    // Reset errors
    toka::DiagnosticEngine::ErrorCount = 0;
    
    // Capture stdout
    std::streambuf* oldCout = std::cout.rdbuf();
    StringbufStream captureBuf;
    std::cout.rdbuf(&captureBuf);

    toka::SourceManager sm;
    toka::DiagnosticEngine::init(sm);

    std::vector<std::unique_ptr<toka::Module>> astModules;
    std::set<std::string> visited;
    std::vector<std::string> recursionStack;
    std::vector<std::string> searchPaths; // Emscripten will have access to virtual /lib

    parseSource("playground.tk", astModules, visited, recursionStack, sm, searchPaths, code_cstr);

    if (!toka::DiagnosticEngine::hasErrors()) {
        toka::Sema sema;
        sema.setBorrowCheckEnabled(true);
        for (auto &mod : astModules) {
            sema.checkModule(*mod);
        }
    }

    // Restore stdout
    std::cout.rdbuf(oldCout);

    std::string errors = captureBuf.getString();
    
    if (errors.empty()) {
        lastResult = "{\"status\": \"ok\"}";
    } else {
        // Output might contain multiple JSON objects separated by newlines
        // We wrap them in a JSON array
        std::stringstream jsonArr;
        jsonArr << "{\"status\": \"error\", \"diagnostics\": [";
        
        bool first = true;
        std::istringstream iss(errors);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            if (!first) jsonArr << ",";
            jsonArr << line;
            first = false;
        }
        jsonArr << "]}";
        
        lastResult = jsonArr.str();
    }

    return lastResult.c_str();
}

} // extern "C"
