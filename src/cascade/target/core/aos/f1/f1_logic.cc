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

#include "target/core/aos/f1/f1_logic.h"

namespace cascade::aos {

F1Logic::F1Logic(Interface* interface, ModuleDeclaration* md, uint32_t fpga, uint32_t slot) : AosLogic<uint64_t>(interface, md) {
  aos_ = new aos_client();
  aos_->set_slot_id(fpga);
  aos_->set_app_id(slot);
  aos_->connect();
  
  get_table()->set_read([this](size_t index) {
    // Read into temporary variable
    uint64_t temp;
    // Read using AmorphOS API
    aos_errcode err;
    err = aos_->aos_cntrlreg_read(index << 3, temp);
    assert(err == aos_errcode::SUCCESS);
    // Silence unused variable warnings
    (void)err;
    //printf("Read value 0x%lX from var %zu\n", temp, index);
    return temp;
  });
  get_table()->set_write([this](size_t index, uint64_t val) {
    // Write using AmorphOS API
    aos_errcode err;
    err = aos_->aos_cntrlreg_write(index << 3, val);
    assert(err == aos_errcode::SUCCESS);
    // Silence unused variable warnings
    (void)err;
    //printf("Wrote value 0x%lX to var %zu\n", val, index);
  });
}

bool F1Logic::connected() {
  if (!aos_->connected()) {
    aos_->connect();
  }
  return aos_->connected();
}

F1Logic::~F1Logic() {
  aos_->disconnect();
  delete aos_;
}

} // namespace cascade::aos
