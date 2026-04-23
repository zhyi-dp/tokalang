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
#include "toka/CodeGen.h"
#include "toka/DiagnosticEngine.h"
#include "toka/Lexer.h"
#include "toka/Parser.h"
#include "toka/Sema.h"
#include "toka/SourceLocation.h"
#include "toka/SourceManager.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Transforms/Coroutines/CoroCleanup.h"
#include "llvm/Transforms/Coroutines/CoroEarly.h"
#include "llvm/Transforms/Coroutines/CoroElide.h"
#include "llvm/Transforms/Coroutines/CoroSplit.h"

#include "toka/Version.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include <sstream>

#include "llvm/Support/FileSystem.h"

bool verboseMode = false;

void parseSource(const std::string &filename,
                 std::vector<std::unique_ptr<toka::Module>> &astModules,
                 std::set<std::string> &visited,
                 std::vector<std::string> &recursionStack,
                 toka::SourceManager &sm,
                 const std::vector<std::string> &searchPaths,
                 const std::map<std::string, std::string> &pkgMap) {
  // Check recursion stack for circular dependency
  for (const auto &f : recursionStack) {
    if (f == filename) {
      std::string chain;
      for (const auto &s : recursionStack)
        chain += s + " -> ";
      chain += filename;
      toka::DiagnosticEngine::report(toka::DiagLoc{}, toka::DiagID::ERR_FILE_IO,
                                     "Circular dependency detected: " + chain);
      return;
    }
  }

  if (visited.count(filename))
    return;
  visited.insert(filename);
  recursionStack.push_back(filename);

  std::string resolvedPath = filename;
  bool found = false;

  // 0. Check package aliases
  auto pkgIt = pkgMap.find(filename);
  if (pkgIt != pkgMap.end()) {
    resolvedPath = pkgIt->second;
    found = true;
  }
  // 1. Try exact filename
  else if (std::ifstream(filename).good()) {
    found = true;
  }
  // 2. Try adding .tk
  else if (filename.find(".tk") == std::string::npos &&
           std::ifstream(filename + ".tk").good()) {
    resolvedPath = filename + ".tk";
    found = true;
  }
  // 3. Try user-provided search paths and default lib/ paths
  else {
    std::vector<std::string> pathsToTry;
    for (const auto &p : searchPaths) {
      if (!p.empty() && p.back() != '/') {
        pathsToTry.push_back(p + "/");
      } else {
        pathsToTry.push_back(p);
      }
    }
    pathsToTry.push_back("lib/");
    pathsToTry.push_back("../lib/");

    for (const auto &p : pathsToTry) {
      std::string libPath = p + filename;
      if (std::ifstream(libPath).good()) {
        resolvedPath = libPath;
        found = true;
        break;
      }
      if (filename.find(".tk") == std::string::npos) {
        libPath += ".tk";
        if (std::ifstream(libPath).good()) {
          resolvedPath = libPath;
          found = true;
          break;
        }
      }
    }
  }

  if (!found) {
    toka::DiagnosticEngine::report(toka::DiagLoc{}, toka::DiagID::ERR_FILE_IO,
                                   "Could not open file: " + filename);
    return;
  }

  if (verboseMode) llvm::errs() << "Parsing " << resolvedPath << "...\n";

  toka::SourceLocation startLoc = sm.loadFile(resolvedPath);
  if (startLoc.isInvalid()) {
    toka::DiagnosticEngine::report(toka::DiagLoc{}, toka::DiagID::ERR_FILE_IO,
                                   "Failed to load file via SourceManager: " +
                                       resolvedPath);
    return;
  }
  std::string code(sm.getBufferData(startLoc));

  toka::Lexer lexer(code.c_str(), startLoc);
  auto tokens = lexer.tokenize();

  toka::Parser parser(tokens, resolvedPath);
  auto module = parser.parseModule();

  // Recursively parse imports
  for (const auto &imp : module->Imports) {
    if (!imp->Items.empty()) {
      // TODO: Handle logic import symbol filtering if we add per-module symbol
      // tables. For now, we just parse the file to register its globals.
    }
    parseSource(imp->PhysicalPath, astModules, visited, recursionStack, sm, searchPaths, pkgMap);
  }

  astModules.push_back(std::move(module));
  recursionStack.pop_back(); // Pop after finishing processing logic for this
                             // module's imports
}

int main(int argc, char **argv) {
  std::vector<std::string> searchPaths;
  if (const char* env_p = std::getenv("TOKA_LIB")) {
    std::string envStr(env_p);
    std::stringstream ss(envStr);
    std::string item;
    while (std::getline(ss, item, ':')) {
        if (!item.empty()) {
            searchPaths.push_back(item);
        }
    }
  }

  std::vector<std::string> inputFiles;
  std::map<std::string, std::string> pkgMap;
  bool disableBorrowCheck = false;
  bool emitObj = false;
  std::string outputFile = "";
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-I") {
      if (i + 1 < argc) {
        searchPaths.push_back(argv[++i]);
      } else {
        llvm::errs() << "-I requires an argument\n";
        return 1;
      }
    } else if (arg.rfind("-I", 0) == 0 && arg.length() > 2) {
      searchPaths.push_back(arg.substr(2));
    } else if (arg == "--version" || arg == "-V") {
      llvm::outs() << "toka version 0.8.0-beta (Built: " << __DATE__ << " " << __TIME__ << ")\n";
      return 0;
    } else if (arg == "--disable-borrow-check") {
      disableBorrowCheck = true;
    } else if (arg == "--pkg" || arg == "-P") {
      if (i + 1 < argc) {
        std::string mapping = argv[++i];
        size_t eqPos = mapping.find('=');
        if (eqPos != std::string::npos) {
          pkgMap[mapping.substr(0, eqPos)] = mapping.substr(eqPos + 1);
        } else {
          llvm::errs() << "--pkg requires format name=path\n";
          return 1;
        }
      } else {
        llvm::errs() << "--pkg requires an argument\n";
        return 1;
      }
    } else if (arg == "-v" || arg == "--verbose") {
      verboseMode = true;
    } else if (arg == "-o") {
      if (i + 1 < argc) {
        outputFile = argv[++i];
        if (outputFile.length() > 2 && outputFile.substr(outputFile.length() - 2) == ".o") {
          emitObj = true;
        }
      } else {
        llvm::errs() << "-o requires an argument\n";
        return 1;
      }
    } else if (arg == "--emit-obj") {
      emitObj = true;
    } else if (arg == "--emit-llvm") {
      emitObj = false;
    } else if (arg.rfind("-", 0) == 0) {
      // Ignore other flags for now or report error
    } else {
      inputFiles.push_back(arg);
    }
  }

  if (inputFiles.empty()) {
    llvm::errs() << "Usage: tokac [options] <filename>\n";
    return 1;
  }

  toka::SourceManager sm;
  toka::DiagnosticEngine::init(sm);

  std::vector<std::unique_ptr<toka::Module>> astModules;
  std::set<std::string> visited;

  std::vector<std::string> recursionStack;
  for (const auto &file : inputFiles) {
    parseSource(file, astModules, visited, recursionStack, sm, searchPaths, pkgMap);
  }

  if (astModules.empty())
    return 1;

  if (verboseMode) llvm::errs() << "Parse Successful. Running Semantic Analysis...\n";

  toka::Sema sema;
  sema.setBorrowCheckEnabled(!disableBorrowCheck);
  for (const auto &ast : astModules) {
    if (!sema.checkModule(*ast)) {
      return 1;
    }
  }

  if (verboseMode) fprintf(stderr, "Sema Successful. Merging and Generating IR...\n");
  fflush(stderr);

  if (verboseMode) fprintf(stderr, "Initializing LLVM Context...\n");
  fflush(stderr);
  llvm::LLVMContext context;
  if (verboseMode) fprintf(stderr, "Instantiating CodeGen for module: %s\n", argv[1]);
  fflush(stderr);
  toka::CodeGen codegen(context, argv[1]);
  if (verboseMode) fprintf(stderr, "CodeGen instantiated.\n");
  fflush(stderr);

  std::unique_ptr<toka::Module> genericModule = sema.extractGenericRegistry();
  
  if (verboseMode) fprintf(stderr, "Pass 1: Discovery (Registration)...\n");
  fflush(stderr);
  for (const auto &ast : astModules) {
    codegen.discover(*ast);
  }
  if (genericModule) codegen.discover(*genericModule);

  if (verboseMode) fprintf(stderr, "Pass 2: Resolution (Signatures)...\n");
  fflush(stderr);
  for (const auto &ast : astModules) {
    codegen.resolveSignatures(*ast);
  }
  if (genericModule) codegen.resolveSignatures(*genericModule);

  if (verboseMode) fprintf(stderr, "Pass 3: Generation (Emission)...\n");
  fflush(stderr);
  for (const auto &ast : astModules) {
    codegen.generate(*ast);
  }
  if (genericModule) codegen.generate(*genericModule);
  
  codegen.finalizeGlobals();

  if (codegen.hasErrors()) {
    return 1;
  }

  if (verboseMode) fprintf(stderr, "Pass 4: Optimization (Coroutines & O2)...\n");
  fflush(stderr);

  llvm::LoopAnalysisManager LAM;
  llvm::FunctionAnalysisManager FAM;
  llvm::CGSCCAnalysisManager CGAM;
  llvm::ModuleAnalysisManager MAM;

  llvm::PassBuilder PB;

  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
  
  PB.registerPipelineStartEPCallback(
      [](llvm::ModulePassManager &MPM, llvm::OptimizationLevel Level) {
        MPM.addPass(llvm::CoroEarlyPass());
        llvm::CGSCCPassManager CGPM;
        CGPM.addPass(llvm::CoroSplitPass());
        MPM.addPass(llvm::createModuleToPostOrderCGSCCPassAdaptor(std::move(CGPM)));
        llvm::FunctionPassManager FPM;
        FPM.addPass(llvm::CoroElidePass());
        MPM.addPass(llvm::createModuleToFunctionPassAdaptor(std::move(FPM)));
        MPM.addPass(llvm::CoroCleanupPass());
      });

  llvm::ModulePassManager MPM = PB.buildO0DefaultPipeline(llvm::OptimizationLevel::O0);
  MPM.run(*codegen.getModule(), MAM);

  
  if (outputFile.empty() && emitObj) {
    llvm::errs() << "Error: --emit-obj requires an output file specified with -o\n";
    return 1;
  }

  if (emitObj) {
    if (verboseMode) fprintf(stderr, "Initializing TargetMachine for native emission...\n");
    fflush(stderr);

    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    auto TargetTriple = llvm::sys::getDefaultTargetTriple();
    std::string Error;
    auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);

    if (!Target) {
      llvm::errs() << "Target lookup error: " << Error;
      return 1;
    }

    auto CPU = "generic";
    auto Features = "";
    llvm::TargetOptions opt;
    std::optional<llvm::Reloc::Model> RM = llvm::Reloc::PIC_;
    auto TargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);

    codegen.getModule()->setDataLayout(TargetMachine->createDataLayout());
    codegen.getModule()->setTargetTriple(TargetTriple);

    std::error_code EC;
    llvm::raw_fd_ostream dest(outputFile, EC, llvm::sys::fs::OF_None);
    if (EC) {
      llvm::errs() << "Could not open file: " << EC.message() << "\n";
      return 1;
    }

    llvm::legacy::PassManager pass;
    if (TargetMachine->addPassesToEmitFile(pass, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
      llvm::errs() << "TargetMachine can't emit a file of this type\n";
      return 1;
    }

    pass.run(*codegen.getModule());
    dest.flush();
    if (verboseMode) fprintf(stderr, "Object file emitted successfully.\n");
  } else {
    if (outputFile.empty()) {
      codegen.print(llvm::outs());
    } else {
      std::error_code EC;
      llvm::raw_fd_ostream dest(outputFile, EC, llvm::sys::fs::OF_None);
      if (EC) {
        llvm::errs() << "Could not open file: " << EC.message() << "\n";
        return 1;
      }
      codegen.print(dest);
    }
  }


  return 0;
}
