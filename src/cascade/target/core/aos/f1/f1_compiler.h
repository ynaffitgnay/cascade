// Copyright 2017-2019 VMware, Inc.
// SPDX-License-Identifier: BSD-2-Clause
//
// The BSD-2 license (the License) set forth below applies to all parts of the
// Cascade project.  You may not use this file except in compliance with the
// License.
//
// BSD-2 License
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CASCADE_SRC_TARGET_CORE_AOS_F1_F1_COMPILER_H
#define CASCADE_SRC_TARGET_CORE_AOS_F1_F1_COMPILER_H

#include "target/core/aos/aos_compiler.h"
#include "target/core/aos/f1/f1_logic.h"

namespace cascade::aos {

class F1Compiler : public AosCompiler<uint64_t> {
  public:
    F1Compiler();
    ~F1Compiler() override;

    F1Compiler& set_host(const std::string& host);
    F1Compiler& set_port(uint32_t port);
    F1Compiler& set_fpga(uint32_t fpga);

  private:
    // Configuration State:
    std::string host_;
    uint32_t port_;
    uint32_t fpga_;

    // Aos Compiler Interface:
    F1Logic* build(Interface* interface, ModuleDeclaration* md, size_t slot) override;
    bool compile(const std::string& text, std::mutex& lock) override;
    void stop_compile() override;

    // Compilation Helpers:
    void compile(sockstream* sock, const std::string& text);
    bool block_on_compile(sockstream* sock);
    bool reprogram(sockstream* sock);
};

} // namespace cascade::aos

#endif
