// Copyright (c) 2026 Toka Project. All rights reserved.
// lib/sys/llvm_shim.h
// Flat C interface wrapping LLVM 20 C++ APIs for the self-hosted compiler.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// Opaque handles
typedef void* TokaLLVMContextRef;
typedef void* TokaLLVMModuleRef;
typedef void* TokaLLVMBuilderRef;
typedef void* TokaLLVMTypeRef;
typedef void* TokaLLVMValueRef;
typedef void* TokaLLVMBasicBlockRef;

// 1. Context & Module
TokaLLVMContextRef toka_llvm_create_context(void);
TokaLLVMModuleRef toka_llvm_create_module(const char* name, TokaLLVMContextRef ctx);
void toka_llvm_dispose_module(TokaLLVMModuleRef mod);
void toka_llvm_dispose_context(TokaLLVMContextRef ctx);

// 2. Types
TokaLLVMTypeRef toka_llvm_type_void(TokaLLVMContextRef ctx);
TokaLLVMTypeRef toka_llvm_type_int(int bits, TokaLLVMContextRef ctx);
TokaLLVMTypeRef toka_llvm_type_double(TokaLLVMContextRef ctx);
TokaLLVMTypeRef toka_llvm_type_pointer(TokaLLVMContextRef ctx);
TokaLLVMTypeRef toka_llvm_type_function(TokaLLVMTypeRef retTy, TokaLLVMTypeRef* paramTys, int paramCount, bool isVarArg);

// 3. Builder
TokaLLVMBuilderRef toka_llvm_create_builder(TokaLLVMContextRef ctx);
void toka_llvm_dispose_builder(TokaLLVMBuilderRef builder);
void toka_llvm_position_builder_at_end(TokaLLVMBuilderRef builder, TokaLLVMBasicBlockRef block);

// 4. Instructions
TokaLLVMValueRef toka_llvm_build_add(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name);
TokaLLVMValueRef toka_llvm_build_sub(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name);
TokaLLVMValueRef toka_llvm_build_mul(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name);
TokaLLVMValueRef toka_llvm_build_ret(TokaLLVMBuilderRef builder, TokaLLVMValueRef val);
TokaLLVMValueRef toka_llvm_build_ret_void(TokaLLVMBuilderRef builder);
TokaLLVMValueRef toka_llvm_build_call(TokaLLVMBuilderRef builder, TokaLLVMValueRef fn, TokaLLVMValueRef* args, int argCount, const char* name);

// 5. Functions & Basic Blocks
TokaLLVMValueRef toka_llvm_add_function(TokaLLVMModuleRef mod, const char* name, TokaLLVMTypeRef fnTy);
TokaLLVMBasicBlockRef toka_llvm_append_basic_block(TokaLLVMContextRef ctx, TokaLLVMValueRef fn, const char* name);
TokaLLVMValueRef toka_llvm_get_param(TokaLLVMValueRef fn, int index);

// 6. Validation & Utilities
bool toka_llvm_verify_module(TokaLLVMModuleRef mod, char** outError);
char* toka_llvm_print_module_to_string(TokaLLVMModuleRef mod);
void toka_llvm_dispose_string(char* str);

#ifdef __cplusplus
}
#endif
