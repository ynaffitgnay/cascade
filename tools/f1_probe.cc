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

#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include "cl/cl.h"
#include "common/system.h"
#include "target/core/aos/f1/aos.h"

using namespace cascade;
using namespace cascade::cl;
using namespace cascade::aos;
using namespace std;

__attribute__((unused)) auto& g1 = Group::create("Configuration Options");
auto& fpga = StrArg<size_t>::create("--fpga")
  .usage("<FId>")
  .description("FPGA ID")
  .initial(0);
auto& mid = StrArg<size_t>::create("--mid")
  .usage("<MId>")
  .description("Module ID")
  .initial(0);
auto& read_cmd = StrArg<string>::create("-r")
  .usage("<VId>")
  .description("Variable ID")
  .initial("-1");
auto& write_cmd = StrArg<string>::create("-w")
  .usage("<VId>:<Val>")
  .description("Variable ID and 64-bit value to write")
  .initial("-1:0");

aos_client aos_;

uint64_t fpga_read(const uint64_t addr) {
  // Read into temporary variable
  uint64_t temp;
  // Read using AmorphOS API
  aos_errcode err;
  err = aos_.aos_cntrlreg_read(addr << 3, temp);
  assert(err == aos_errcode::SUCCESS);
  // Silence unused variable warnings
  (void)err;
  //printf("Read value 0x%lX from var %ld\n", temp, addr);
  return temp;
}

void fpga_write(const uint64_t addr, const uint64_t val) {
  // Write using AmorphOS API
  aos_errcode err;
  err = aos_.aos_cntrlreg_write(addr << 3, val);
  assert(err == aos_errcode::SUCCESS);
  // Silence unused variable warnings
  (void)err;
  //printf("Wrote value 0x%lX to var %ld\n", val, addr);
}

int done(int code) {
  aos_.disconnect();
  return code;
}

int main(int argc, char** argv) {
  Simple::read(argc, argv);
  
  aos_.set_slot_id(fpga.value());
  aos_.set_app_id(mid.value());
  if (!aos_.connect()) {
  	if (System::execute(System::src_root() + "/src/target/core/aos/f1/device/daemon/start.sh >/dev/null 2>&1") != 0) {
  		cerr << "Could not start AOS daemon" << endl;
  		return 1;
  	}
  }
  
  if (!aos_.connect()) {
    cout << "Could not connect to AOS daemon" << endl;
    return 1;
  }

  stringstream ss1(read_cmd.value());
  int r;
  ss1 >> r;
  
  if (r >= 0) {
    cout << "VID[" << r << "] = " << fpga_read(r) << endl;
    return done(0);
  }

  stringstream ss2(write_cmd.value());
  string s;
  getline(ss2, s, ':');

  stringstream ss3(s);
  int w;
  ss3 >> w;
  uint64_t val;
  ss2 >> val;

  if (w >= 0) {
    fpga_write(w, val);
    cout << "VID[" << w << "] = " << val << endl;
    return done(0);
  }

  cout << "No commands provided" << endl;
  return done(0);
}
