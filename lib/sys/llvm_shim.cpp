// Copyright (c) 2026 Toka Project. All rights reserved.
// lib/sys/llvm_shim.cpp
// C++ implementation of the LLVM 20 flat C wrapper.

#include "llvm_shim.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>

extern "C" {

// 1. Context & Module
TokaLLVMContextRef toka_llvm_create_context(void) {
    return reinterpret_cast<TokaLLVMContextRef>(new llvm::LLVMContext());
}

TokaLLVMModuleRef toka_llvm_create_module(const char* name, TokaLLVMContextRef ctx) {
    auto* context = reinterpret_cast<llvm::LLVMContext*>(ctx);
    return reinterpret_cast<TokaLLVMModuleRef>(new llvm::Module(name, *context));
}

void toka_llvm_dispose_module(TokaLLVMModuleRef mod) {
    delete reinterpret_cast<llvm::Module*>(mod);
}

void toka_llvm_dispose_context(TokaLLVMContextRef ctx) {
    delete reinterpret_cast<llvm::LLVMContext*>(ctx);
}

// 2. Types
TokaLLVMTypeRef toka_llvm_type_void(TokaLLVMContextRef ctx) {
    auto* context = reinterpret_cast<llvm::LLVMContext*>(ctx);
    return reinterpret_cast<TokaLLVMTypeRef>(llvm::Type::getVoidTy(*context));
}

TokaLLVMTypeRef toka_llvm_type_int(int bits, TokaLLVMContextRef ctx) {
    auto* context = reinterpret_cast<llvm::LLVMContext*>(ctx);
    return reinterpret_cast<TokaLLVMTypeRef>(llvm::Type::getIntNTy(*context, bits));
}

TokaLLVMTypeRef toka_llvm_type_double(TokaLLVMContextRef ctx) {
    auto* context = reinterpret_cast<llvm::LLVMContext*>(ctx);
    return reinterpret_cast<TokaLLVMTypeRef>(llvm::Type::getDoubleTy(*context));
}

TokaLLVMTypeRef toka_llvm_type_pointer(TokaLLVMContextRef ctx) {
    auto* context = reinterpret_cast<llvm::LLVMContext*>(ctx);
    return reinterpret_cast<TokaLLVMTypeRef>(llvm::PointerType::getUnqual(*context));
}

TokaLLVMTypeRef toka_llvm_type_function(TokaLLVMTypeRef retTy, TokaLLVMTypeRef* paramTys, int paramCount, bool isVarArg) {
    auto* returnType = reinterpret_cast<llvm::Type*>(retTy);
    std::vector<llvm::Type*> params;
    params.reserve(paramCount);
    for (int i = 0; i < paramCount; ++i) {
        params.push_back(reinterpret_cast<llvm::Type*>(paramTys[i]));
    }
    auto* funcTy = llvm::FunctionType::get(returnType, params, isVarArg);
    return reinterpret_cast<TokaLLVMTypeRef>(funcTy);
}

// 3. Builder
TokaLLVMBuilderRef toka_llvm_create_builder(TokaLLVMContextRef ctx) {
    auto* context = reinterpret_cast<llvm::LLVMContext*>(ctx);
    return reinterpret_cast<TokaLLVMBuilderRef>(new llvm::IRBuilder<>(*context));
}

void toka_llvm_dispose_builder(TokaLLVMBuilderRef builder) {
    delete reinterpret_cast<llvm::IRBuilder<>*>(builder);
}

void toka_llvm_position_builder_at_end(TokaLLVMBuilderRef builder, TokaLLVMBasicBlockRef block) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* basicBlock = reinterpret_cast<llvm::BasicBlock*>(block);
    builderPtr->SetInsertPoint(basicBlock);
}

// 4. Instructions
TokaLLVMValueRef toka_llvm_build_add(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateAdd(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_sub(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateSub(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_mul(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateMul(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_ret(TokaLLVMBuilderRef builder, TokaLLVMValueRef val) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* retVal = reinterpret_cast<llvm::Value*>(val);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateRet(retVal));
}

TokaLLVMValueRef toka_llvm_build_ret_void(TokaLLVMBuilderRef builder) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateRetVoid());
}

TokaLLVMValueRef toka_llvm_build_call(TokaLLVMBuilderRef builder, TokaLLVMValueRef fn, TokaLLVMValueRef* args, int argCount, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* val = reinterpret_cast<llvm::Value*>(fn);
    std::vector<llvm::Value*> values;
    values.reserve(argCount);
    for (int i = 0; i < argCount; ++i) {
        values.push_back(reinterpret_cast<llvm::Value*>(args[i]));
    }
    if (auto* func = llvm::dyn_cast<llvm::Function>(val)) {
        return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateCall(func->getFunctionType(), func, values, name));
    }
    // Fallback if not direct function call: assume it's casted or pointer and try to get dynamic type
    auto* dummyFunc = reinterpret_cast<llvm::Function*>(val);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateCall(dummyFunc->getFunctionType(), val, values, name));
}

// 5. Functions & Basic Blocks
TokaLLVMValueRef toka_llvm_add_function(TokaLLVMModuleRef mod, const char* name, TokaLLVMTypeRef fnTy) {
    auto* module = reinterpret_cast<llvm::Module*>(mod);
    auto* funcTy = reinterpret_cast<llvm::FunctionType*>(fnTy);
    llvm::FunctionCallee callee = module->getOrInsertFunction(name, funcTy);
    return reinterpret_cast<TokaLLVMValueRef>(callee.getCallee());
}

TokaLLVMBasicBlockRef toka_llvm_append_basic_block(TokaLLVMContextRef ctx, TokaLLVMValueRef fn, const char* name) {
    auto* context = reinterpret_cast<llvm::LLVMContext*>(ctx);
    auto* func = reinterpret_cast<llvm::Function*>(fn);
    return reinterpret_cast<TokaLLVMBasicBlockRef>(llvm::BasicBlock::Create(*context, name, func));
}

TokaLLVMValueRef toka_llvm_get_param(TokaLLVMValueRef fn, int index) {
    auto* func = reinterpret_cast<llvm::Function*>(fn);
    return reinterpret_cast<TokaLLVMValueRef>(func->getArg(index));
}

// 6. Validation & Utilities
bool toka_llvm_verify_module(TokaLLVMModuleRef mod, char** outError) {
    auto* module = reinterpret_cast<llvm::Module*>(mod);
    std::string err;
    llvm::raw_string_ostream os(err);
    bool failed = llvm::verifyModule(*module, &os);
    if (failed) {
        os.flush();
        if (outError) {
            *outError = strdup(err.c_str());
        }
        return false;
    }
    return true;
}

char* toka_llvm_print_module_to_string(TokaLLVMModuleRef mod) {
    auto* module = reinterpret_cast<llvm::Module*>(mod);
    std::string str;
    llvm::raw_string_ostream os(str);
    module->print(os, nullptr);
    os.flush();
    return strdup(str.c_str());
}

void toka_llvm_dispose_string(char* str) {
    free(str);
}

} // extern "C"
