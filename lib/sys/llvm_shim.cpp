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

TokaLLVMValueRef toka_llvm_build_alloca(TokaLLVMBuilderRef builder, TokaLLVMTypeRef type, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* ty = reinterpret_cast<llvm::Type*>(type);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateAlloca(ty, nullptr, name));
}

TokaLLVMValueRef toka_llvm_build_load(TokaLLVMBuilderRef builder, TokaLLVMTypeRef type, TokaLLVMValueRef ptr, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* ty = reinterpret_cast<llvm::Type*>(type);
    auto* ptrVal = reinterpret_cast<llvm::Value*>(ptr);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateLoad(ty, ptrVal, name));
}

TokaLLVMValueRef toka_llvm_build_store(TokaLLVMBuilderRef builder, TokaLLVMValueRef val, TokaLLVMValueRef ptr) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* value = reinterpret_cast<llvm::Value*>(val);
    auto* ptrVal = reinterpret_cast<llvm::Value*>(ptr);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateStore(value, ptrVal));
}

TokaLLVMValueRef toka_llvm_build_gep(TokaLLVMBuilderRef builder, TokaLLVMTypeRef type, TokaLLVMValueRef ptr, TokaLLVMValueRef* indices, int indexCount, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* ty = reinterpret_cast<llvm::Type*>(type);
    auto* ptrVal = reinterpret_cast<llvm::Value*>(ptr);
    std::vector<llvm::Value*> idxList;
    idxList.reserve(indexCount);
    for (int i = 0; i < indexCount; ++i) {
        idxList.push_back(reinterpret_cast<llvm::Value*>(indices[i]));
    }
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateGEP(ty, ptrVal, idxList, name));
}

TokaLLVMValueRef toka_llvm_build_br(TokaLLVMBuilderRef builder, TokaLLVMBasicBlockRef destBlock) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* block = reinterpret_cast<llvm::BasicBlock*>(destBlock);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateBr(block));
}

TokaLLVMValueRef toka_llvm_build_cond_br(TokaLLVMBuilderRef builder, TokaLLVMValueRef cond, TokaLLVMBasicBlockRef thenBlock, TokaLLVMBasicBlockRef elseBlock) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* condVal = reinterpret_cast<llvm::Value*>(cond);
    auto* tBlock = reinterpret_cast<llvm::BasicBlock*>(thenBlock);
    auto* fBlock = reinterpret_cast<llvm::BasicBlock*>(elseBlock);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateCondBr(condVal, tBlock, fBlock));
}

TokaLLVMValueRef toka_llvm_build_icmp(TokaLLVMBuilderRef builder, int op, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    llvm::CmpInst::Predicate pred;
    switch (op) {
        case 0: pred = llvm::CmpInst::ICMP_EQ; break;
        case 1: pred = llvm::CmpInst::ICMP_NE; break;
        case 2: pred = llvm::CmpInst::ICMP_UGT; break;
        case 3: pred = llvm::CmpInst::ICMP_UGE; break;
        case 4: pred = llvm::CmpInst::ICMP_ULT; break;
        case 5: pred = llvm::CmpInst::ICMP_ULE; break;
        case 6: pred = llvm::CmpInst::ICMP_SGT; break;
        case 7: pred = llvm::CmpInst::ICMP_SGE; break;
        case 8: pred = llvm::CmpInst::ICMP_SLT; break;
        case 9: pred = llvm::CmpInst::ICMP_SLE; break;
        default: pred = llvm::CmpInst::ICMP_EQ; break;
    }
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateICmp(pred, left, right, name));
}

TokaLLVMValueRef toka_llvm_const_int(TokaLLVMTypeRef type, long long val, bool signExtend) {
    auto* ty = reinterpret_cast<llvm::Type*>(type);
    return reinterpret_cast<TokaLLVMValueRef>(llvm::ConstantInt::get(ty, val, signExtend));
}

TokaLLVMValueRef toka_llvm_const_null(TokaLLVMTypeRef type) {
    auto* ty = reinterpret_cast<llvm::Type*>(type);
    return reinterpret_cast<TokaLLVMValueRef>(llvm::Constant::getNullValue(ty));
}

TokaLLVMValueRef toka_llvm_build_bitcast(TokaLLVMBuilderRef builder, TokaLLVMValueRef val, TokaLLVMTypeRef destTy, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* value = reinterpret_cast<llvm::Value*>(val);
    auto* ty = reinterpret_cast<llvm::Type*>(destTy);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateBitCast(value, ty, name));
}

TokaLLVMValueRef toka_llvm_build_inttoptr(TokaLLVMBuilderRef builder, TokaLLVMValueRef val, TokaLLVMTypeRef destTy, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* value = reinterpret_cast<llvm::Value*>(val);
    auto* ty = reinterpret_cast<llvm::Type*>(destTy);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateIntToPtr(value, ty, name));
}

TokaLLVMValueRef toka_llvm_build_ptrtoint(TokaLLVMBuilderRef builder, TokaLLVMValueRef val, TokaLLVMTypeRef destTy, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* value = reinterpret_cast<llvm::Value*>(val);
    auto* ty = reinterpret_cast<llvm::Type*>(destTy);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreatePtrToInt(value, ty, name));
}

TokaLLVMValueRef toka_llvm_build_sext(TokaLLVMBuilderRef builder, TokaLLVMValueRef val, TokaLLVMTypeRef destTy, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* value = reinterpret_cast<llvm::Value*>(val);
    auto* ty = reinterpret_cast<llvm::Type*>(destTy);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateSExt(value, ty, name));
}

TokaLLVMValueRef toka_llvm_build_zext(TokaLLVMBuilderRef builder, TokaLLVMValueRef val, TokaLLVMTypeRef destTy, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* value = reinterpret_cast<llvm::Value*>(val);
    auto* ty = reinterpret_cast<llvm::Type*>(destTy);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateZExt(value, ty, name));
}

TokaLLVMValueRef toka_llvm_build_trunc(TokaLLVMBuilderRef builder, TokaLLVMValueRef val, TokaLLVMTypeRef destTy, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* value = reinterpret_cast<llvm::Value*>(val);
    auto* ty = reinterpret_cast<llvm::Type*>(destTy);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateTrunc(value, ty, name));
}

// 4b. Newly Added Advanced Core Instructions
TokaLLVMValueRef toka_llvm_build_sdiv(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateSDiv(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_udiv(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateUDiv(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_srem(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateSRem(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_urem(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateURem(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_and(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateAnd(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_or(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateOr(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_xor(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateXor(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_shl(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateShl(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_lshr(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateLShr(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_ashr(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateAShr(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_fadd(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateFAdd(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_fsub(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateFSub(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_fmul(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateFMul(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_fdiv(TokaLLVMBuilderRef builder, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateFDiv(left, right, name));
}

TokaLLVMValueRef toka_llvm_build_fcmp(TokaLLVMBuilderRef builder, int op, TokaLLVMValueRef lhs, TokaLLVMValueRef rhs, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* left = reinterpret_cast<llvm::Value*>(lhs);
    auto* right = reinterpret_cast<llvm::Value*>(rhs);
    llvm::FCmpInst::Predicate pred;
    switch (op) {
        case 0: pred = llvm::FCmpInst::FCMP_OEQ; break;
        case 1: pred = llvm::FCmpInst::FCMP_ONE; break;
        case 2: pred = llvm::FCmpInst::FCMP_OGT; break;
        case 3: pred = llvm::FCmpInst::FCMP_OGE; break;
        case 4: pred = llvm::FCmpInst::FCMP_OLT; break;
        case 5: pred = llvm::FCmpInst::FCMP_OLE; break;
        case 6: pred = llvm::FCmpInst::FCMP_UEQ; break;
        case 7: pred = llvm::FCmpInst::FCMP_UNE; break;
        case 8: pred = llvm::FCmpInst::FCMP_UGT; break;
        case 9: pred = llvm::FCmpInst::FCMP_UGE; break;
        case 10: pred = llvm::FCmpInst::FCMP_ULT; break;
        case 11: pred = llvm::FCmpInst::FCMP_ULE; break;
        case 12: pred = llvm::FCmpInst::FCMP_TRUE; break;
        case 13: pred = llvm::FCmpInst::FCMP_FALSE; break;
        default: pred = llvm::FCmpInst::FCMP_OEQ; break;
    }
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateFCmp(pred, left, right, name));
}

TokaLLVMValueRef toka_llvm_build_sitofp(TokaLLVMBuilderRef builder, TokaLLVMValueRef val, TokaLLVMTypeRef destTy, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* value = reinterpret_cast<llvm::Value*>(val);
    auto* ty = reinterpret_cast<llvm::Type*>(destTy);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateSIToFP(value, ty, name));
}

TokaLLVMValueRef toka_llvm_build_fptosi(TokaLLVMBuilderRef builder, TokaLLVMValueRef val, TokaLLVMTypeRef destTy, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* value = reinterpret_cast<llvm::Value*>(val);
    auto* ty = reinterpret_cast<llvm::Type*>(destTy);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateFPToSI(value, ty, name));
}

TokaLLVMValueRef toka_llvm_build_phi(TokaLLVMBuilderRef builder, TokaLLVMTypeRef type, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* ty = reinterpret_cast<llvm::Type*>(type);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreatePHI(ty, 0, name));
}

void toka_llvm_add_incoming(TokaLLVMValueRef phi, TokaLLVMValueRef* vals, TokaLLVMBasicBlockRef* blocks, int count) {
    auto* phiNode = reinterpret_cast<llvm::PHINode*>(phi);
    for (int i = 0; i < count; ++i) {
        phiNode->addIncoming(reinterpret_cast<llvm::Value*>(vals[i]), reinterpret_cast<llvm::BasicBlock*>(blocks[i]));
    }
}

TokaLLVMValueRef toka_llvm_build_select(TokaLLVMBuilderRef builder, TokaLLVMValueRef cond, TokaLLVMValueRef trueVal, TokaLLVMValueRef falseVal, const char* name) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* condV = reinterpret_cast<llvm::Value*>(cond);
    auto* tVal = reinterpret_cast<llvm::Value*>(trueVal);
    auto* fVal = reinterpret_cast<llvm::Value*>(falseVal);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateSelect(condV, tVal, fVal, name));
}

TokaLLVMValueRef toka_llvm_build_switch(TokaLLVMBuilderRef builder, TokaLLVMValueRef val, TokaLLVMBasicBlockRef defaultBlock, int numCases) {
    auto* builderPtr = reinterpret_cast<llvm::IRBuilder<>*>(builder);
    auto* value = reinterpret_cast<llvm::Value*>(val);
    auto* defBlock = reinterpret_cast<llvm::BasicBlock*>(defaultBlock);
    return reinterpret_cast<TokaLLVMValueRef>(builderPtr->CreateSwitch(value, defBlock, numCases));
}

void toka_llvm_add_case(TokaLLVMValueRef switchVal, TokaLLVMValueRef onVal, TokaLLVMBasicBlockRef destBlock) {
    auto* switchInst = reinterpret_cast<llvm::SwitchInst*>(switchVal);
    auto* caseVal = llvm::cast<llvm::ConstantInt>(reinterpret_cast<llvm::Value*>(onVal));
    switchInst->addCase(caseVal, reinterpret_cast<llvm::BasicBlock*>(destBlock));
}

TokaLLVMValueRef toka_llvm_add_global(TokaLLVMModuleRef mod, TokaLLVMTypeRef type, const char* name) {
    auto* module = reinterpret_cast<llvm::Module*>(mod);
    auto* ty = reinterpret_cast<llvm::Type*>(type);
    // Use Constant::getNullValue as a placeholder initializer so it's not a pure external declaration, or set to null
    auto* gv = new llvm::GlobalVariable(*module, ty, false, llvm::GlobalValue::ExternalLinkage, nullptr, name);
    return reinterpret_cast<TokaLLVMValueRef>(gv);
}

void toka_llvm_set_initializer(TokaLLVMValueRef globalVal, TokaLLVMValueRef constVal) {
    auto* gv = reinterpret_cast<llvm::GlobalVariable*>(globalVal);
    auto* init = llvm::cast<llvm::Constant>(reinterpret_cast<llvm::Value*>(constVal));
    gv->setInitializer(init);
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
