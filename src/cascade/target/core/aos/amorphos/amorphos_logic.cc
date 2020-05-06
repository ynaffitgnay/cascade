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

#include "target/core/aos/amorphos/amorphos_logic.h"

namespace cascade::aos {

AmorphosLogic::AmorphosLogic(Interface* interface, ModuleDeclaration* md, size_t slot, syncbuf* reqs, syncbuf* resps) : AosLogic<uint64_t>(interface, md) {
  get_table()->set_read([slot, reqs, resps](size_t index) {
    uint8_t bytes[8];
    const uint8_t packed = (1 << 7) | slot;
    const uint16_t vid = index;
    bytes[0] = packed;
    bytes[1] = vid >> 0;
    bytes[2] = vid >> 8;
    reqs->sputn(reinterpret_cast<const char*>(bytes), 3);
    resps->waitforn(reinterpret_cast<char*>(bytes), 8);
    uint64_t result = 0;
    result = (result << 8) | bytes[7];
    result = (result << 8) | bytes[6];
    result = (result << 8) | bytes[5];
    result = (result << 8) | bytes[4];
    result = (result << 8) | bytes[3];
    result = (result << 8) | bytes[2];
    result = (result << 8) | bytes[1];
    result = (result << 8) | bytes[0];
    //result = *reinterpret_cast<uint64_t*>(&bytes[0]);
    //std::cout << "r " << index << " " << result << std::endl;
    return result;
  });
  get_table()->set_write([slot, reqs](size_t index, uint64_t val) {
    uint8_t bytes[11];
    const uint8_t packed = (1 << 7) | (1 << 6) | slot;
    const uint16_t vid = index;
    const uint64_t data = val;
    bytes[0] = packed;
    bytes[1] = vid >> 0;
    bytes[2] = vid >> 8;
    //*reinterpret_cast<uint16_t*>(&bytes[1]) = vid;
    bytes[3] = data >> 0;
    bytes[4] = data >> 8;
    bytes[5] = data >> 16;
    bytes[6] = data >> 24;
    bytes[7] = data >> 32;
    bytes[8] = data >> 40;
    bytes[9] = data >> 48;
    bytes[10] = data >> 56;
    //*reinterpret_cast<uint64_t*>(&bytes[3]) = data;
    //std::cout << "w " << index << " " << val << std::endl;
    reqs->sputn(reinterpret_cast<const char*>(bytes), 11);
  });
}

} // namespace cascade::aos
