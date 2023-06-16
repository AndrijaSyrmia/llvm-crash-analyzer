//===- MemoryWrapper.cpp Track down changed memory locations --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


#include "Analysis/MemoryWrapper.h"
#include <sstream>

using namespace llvm;

#define DEBUG_TYPE "mem-wrapper"

//promeni StrRef u std::string

crash_analyzer::MemoryWrapper::MemoryWrapper()
{
}

std::string crash_analyzer::MemoryWrapper::ReadFromMemory(uint64_t addr, uint32_t byte_size, lldb::SBError& error)
{
    std::string StrVal;
    std::stringstream SS;
    if(this->ChangedMemoryAdresses.count(addr))
    {
        SS << std::hex << this->ChangedMemoryAdresses[addr].str();
        SS >> StrVal;
        LLVM_DEBUG(llvm::dbgs() << "Addressing changed location: " << addr <<  ":" << StrVal <<"\n";);
    }
    else if(this->Dec != nullptr)
    {

        uint64_t Val = this->Dec->getTarget()->GetProcess().ReadUnsignedFromMemory(addr, byte_size, error);
        SS << std::hex << Val;
        SS >> StrVal;
        LLVM_DEBUG(llvm::dbgs() << "Addressing unchanged location: " << addr <<  ":" << StrVal <<"\n";);
    }
    return StrVal;
}



void crash_analyzer::MemoryWrapper::setDecompiler(crash_analyzer::Decompiler* Dec)
{
    this->Dec = Dec;
}

void crash_analyzer::MemoryWrapper::changeValue(uint64_t addr, StringRef val)
{
    LLVM_DEBUG(llvm::dbgs() << "Changed the value of address " << addr << " to " << val << "\n";);
    this->ChangedMemoryAdresses[addr] = val;
}

void crash_analyzer::MemoryWrapper::invalidateAddress(uint64_t addr)
{
    LLVM_DEBUG(llvm::dbgs() << "Invalidated address " << addr << "\n");
    this->ChangedMemoryAdresses[addr] = "";
}