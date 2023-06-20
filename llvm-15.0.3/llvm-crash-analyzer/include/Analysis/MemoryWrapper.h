
//===- MemoryWrapper.h - Track down changed memory locations ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


#ifndef MEM_WRAPPER_H
#define MEM_WRAPPER_H

#include "Decompiler/Decompiler.h"

#include <unordered_map>

namespace llvm {
    namespace crash_analyzer {
        using MemoryAdressMap = std::unordered_map<uint64_t, std::string>;

        class MemoryWrapper{
            private:
                Decompiler* Dec = nullptr;
                MemoryAdressMap ChangedMemoryAdresses; 

            public:
                MemoryWrapper();
                std::string ReadFromMemory(uint64_t addr, uint32_t byte_size, lldb::SBError& error);
                void setDecompiler(Decompiler* Dec);
                void changeValue(uint64_t addr, std::string val);
                void invalidateAddress(uint64_t addr);
                void dumpChangedMemory();
        };
    }
}

#endif