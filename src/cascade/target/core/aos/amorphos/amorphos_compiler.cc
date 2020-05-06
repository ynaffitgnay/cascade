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

//#include <cstdlib>
//#include <type_traits>
#include <fstream>
#include "target/core/aos/amorphos/amorphos_compiler.h"

namespace cascade::aos {

AmorphosCompiler::AmorphosCompiler() : AosCompiler<uint64_t>() {
  cascade_ = nullptr;
}

AmorphosCompiler::~AmorphosCompiler() {
  if (cascade_ != nullptr) {
    delete cascade_;
  }
}

AmorphosLogic* AmorphosCompiler::build(Interface* interface, ModuleDeclaration* md, size_t slot) {
  return new AmorphosLogic(interface, md, slot, &reqs_, &resps_);
}

bool AmorphosCompiler::compile(const string& text, mutex& lock) {
  (void) lock;
  
  get_compiler()->schedule_state_safe_interrupt([this, &text]{
    System::execute("mkdir -p /tmp/amorphos/");
    char path[] = "/tmp/amorphos/program_logic_XXXXXX.v";
    const auto fd = mkstemps(path, 2);
    close(fd);
    
    std::ofstream ofs(path);
    ofs << text << std::endl;
    ofs.close();
    
    // Determine num apps based on text
    const char* prefix = "app_num == ";
    const int plen = strlen(prefix);
    const char* sp = strstr(text.c_str(), prefix);
    int num_apps = 0;
    while (sp != nullptr) {
      sp += plen;
      num_apps = max(num_apps, atoi(sp)+1);
      sp = strstr(sp, prefix);
    }
    assert(num_apps > 0);
    assert(num_apps <= 32);
    //cout << "Detected " << num_apps << " application slot(s)" << endl;
  
    if (cascade_ != nullptr) {
      delete cascade_;
    }
    cascade_ = new Cascade();
    cascade_->set_stdout(cout.rdbuf());
    cascade_->set_stderr(cout.rdbuf());

    const auto ifd = cascade_->open(&reqs_);
    const auto ofd = cascade_->open(&resps_);

    cascade_->run();
    *cascade_ << "`include \"share/cascade/march/regression/minimal.v\"\n";
    *cascade_ << "`include \"" << path << "\"\n";
    *cascade_ << "localparam NUM_APPS = " << num_apps << ";\n";
    *cascade_ << "integer ifd = " << ifd << ";\n";
    *cascade_ << "integer ofd = " << ofd << ";\n";
    *cascade_ << "`include \"share/cascade/amorphos/amorphos_wrapper.v\"\n";
    cascade_->flush();
    if (cascade_->bad()) stop_compile();
  });

  return true;
}

void AmorphosCompiler::stop_compile() {
  if (cascade_ != nullptr) {
    delete cascade_;
    cascade_ = nullptr;
  }
}

} // namespace cascade::aos
