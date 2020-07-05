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

#ifndef CASCADE_SRC_TARGET_CORE_COMMON_SYNCBUF_H
#define CASCADE_SRC_TARGET_CORE_COMMON_SYNCBUF_H

#include <streambuf>
#include <mutex>
#include <condition_variable>

namespace cascade {

// FIFO with atomic puts and gets Peeking and put-backs are not supported

class syncbuf : public std::streambuf {
  public:
    // Typedefs:
    typedef std::streambuf::char_type char_type;
    typedef std::streambuf::traits_type traits_type;
    typedef std::streambuf::int_type int_type;
    typedef std::streambuf::pos_type pos_type;
    typedef std::streambuf::off_type off_type;
   
    // Constructors:
    syncbuf();
    ~syncbuf() override;
    
    // Blocking Read:
    void waitforn(char_type* s, std::streamsize count);

  private:
    // Shared data buffer:
    char_type* data_;
    size_t data_cap_;
    size_t goff_, poff_;

    // Synchronization:
    std::mutex mut_;
    std::condition_variable cv_;
    
    // Get Area:
    int_type uflow() override;
    std::streamsize xsgetn(char_type* s, std::streamsize count) override;

    // Put Area:
    std::streamsize xsputn(const char_type* s, std::streamsize count) override;
};

inline syncbuf::syncbuf() {
  data_ = new char_type[64];
  data_cap_ = 64;
  goff_ = 0;
  poff_ = 0;
  setg(nullptr, nullptr, nullptr);
  setp(nullptr, nullptr);
}

inline syncbuf::~syncbuf() {
  delete[] data_;
  //std::cout << "Final capacity: " << data_cap_ << std::endl;
}

inline void syncbuf::waitforn(char_type* s, std::streamsize count) {
  std::unique_lock<std::mutex> ul(mut_);

  // Wait on condition variable until enough data is available
  while ((poff_-goff_) < count) {
    cv_.wait(ul);
  }
  
  const auto next_goff = goff_ + count;
  std::copy(&data_[goff_], &data_[next_goff], s);
  goff_ = next_goff;
  
  // If no data is left, reset pointers to recycle space
  if (goff_ == poff_) {
    goff_ = 0;
    poff_ = 0;
  }
}

inline syncbuf::int_type syncbuf::uflow() {
  std::lock_guard<std::mutex> lg(mut_);
  if (goff_ == poff_) {
    return traits_type::eof();
  } else {
    const auto ret_val = traits_type::to_int_type(data_[goff_]);
    ++goff_;
    if (goff_ == poff_) {
      goff_ = 0;
      poff_ = 0;
    }
    return ret_val;
  }
}

inline std::streamsize syncbuf::xsputn(const char_type* s, std::streamsize count) {
  std::lock_guard<std::mutex> lg(mut_);
  size_t next_poff = poff_ + count;
  
  // If current capacity is insufficient, grow by power of 2
  if (next_poff > data_cap_) {
    while (next_poff > data_cap_) {
      data_cap_ *= 2;
    }
    auto next_data = new char_type[data_cap_];
    // Only copy valid entries
    std::copy(&data_[goff_], &data_[poff_], next_data);
    delete data_;
    data_ = next_data;
    poff_ = poff_ - goff_;
    goff_ = 0;
    next_poff = poff_ + count;
  }
  
  std::copy(s, s+count, &data_[poff_]);
  poff_ = next_poff;
  cv_.notify_all();
  
  return count;
}

inline std::streamsize syncbuf::xsgetn(char_type* s, std::streamsize count) {
  std::lock_guard<std::mutex> lg(mut_);

  const auto true_count = std::min((size_t)count, (poff_-goff_));
  if (true_count == 0) {
    return 0;
  }
  
  const auto next_goff = goff_ + true_count;
  std::copy(&data_[goff_], &data_[next_goff], s);
  goff_ = next_goff;
  
  // If no data is left, reset pointers to recycle space
  if (goff_ == poff_) {
    goff_ = 0;
    poff_ = 0;
  }
  
  return true_count;
}

} // namespace cascade

#endif
