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

    // Create a virtual file in SourceManager
    toka::SourceLocation startLoc = sm.addFile("playground.tk", code_cstr);

    if (!startLoc.isInvalid()) {
        toka::Lexer lexer(code_cstr, startLoc);
        auto tokens = lexer.tokenize();

        toka::Parser parser(tokens, "playground.tk");
        auto module = parser.parseModule();

        if (!toka::DiagnosticEngine::hasErrors() && module) {
            toka::Sema sema;
            sema.setBorrowCheckEnabled(true);
            sema.checkModule(*module);
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
