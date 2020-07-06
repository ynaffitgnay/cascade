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

#include <cstring>
#include <signal.h>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include "include/cascade.h"
#include "cl/cl.h"
#include "common/system.h"

using namespace cascade;
using namespace cascade::cl;
using namespace std;

namespace cascade::cascade_tiff {

// Allocate cascade on the heap so we can guarantee that it's torn down before
// stack or static variables.
  Cascade* cascade_ = nullptr;

} // namespace

int main(int argc, char** argv) {

  // Create a new cascade
  cascade::cascade_tiff::cascade_ = new Cascade();

  // Set command line flags
  cascade::cascade_tiff::cascade_->set_fopen_dirs(System::src_root());
  cascade::cascade_tiff::cascade_->set_include_dirs("share/cascade/test/benchmark/mips32/");
  //cascade::cascade_tiff::cascade_->set_enable_inlining(!::disable_inlining.value());
  //cascade::cascade_tiff::cascade_->set_open_loop_target(::open_loop_target.value());
  //cascade::cascade_tiff::cascade_->set_quartus_server(::compiler_host.value(), ::compiler_port.value());
  cascade::cascade_tiff::cascade_->set_vivado_server("localhost", 9920, 0);
  cascade::cascade_tiff::cascade_->set_profile_interval(1);
  cascade::cascade_tiff::cascade_->set_stdout(std::cout.rdbuf());
  cascade::cascade_tiff::cascade_->set_stderr(std::cout.rdbuf());
  cascade::cascade_tiff::cascade_->set_stdwarn(std::cout.rdbuf());
  cascade::cascade_tiff::cascade_->set_stdinfo(std::cout.rdbuf());
  

  //// Map standard streams to colored outbufs
  cascade::cascade_tiff::cascade_->set_stdin(cin.rdbuf());
  auto* fb = new filebuf();
  fb->open("tiff_cascade_noyield.log", ios::app | ios::out);
  cascade::cascade_tiff::cascade_->set_stdlog(fb);

  //// Print the initial prompt
  //cout << ">>> ";

  // Start cascade, and read the march file and -e file (if provided)
  cascade::cascade_tiff::cascade_->run();
  *cascade::cascade_tiff::cascade_ << "`include \"share/cascade/march/regression/f1_minimal_tif.v\"\n"
	      << "`include \"share/cascade/test/benchmark/mips32/run_bubble_128_1024.v\"" << endl;

  // Block until execution is complete
  cascade::cascade_tiff::cascade_->stop_now();
  
  cascade::cascade_tiff::cascade_->flush();

  //// Don't retarget yet
  //// Sleep for 15 seconds
  //usleep(15000000);
  //// Retarget
  //*cascade::cascade_tiff::cascade_ << "initial $retarget(\"regression/f1_minimal_tif\");\n";
  //cout << "Retargeted" << endl;
  //
  //cascade::cascade_tiff::cascade_->flush();

  // Block until execution is complete
  cascade::cascade_tiff::cascade_->stop_now();

  return 0;
}
