//
// The MIT License (MIT)
//
// Copyright 2013-2014 The MilkCat Project Developers
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// crf_vocab.h --- Created at 2014-02-02
//

#ifndef SRC_NEKO_CRF_VOCAB_H_
#define SRC_NEKO_CRF_VOCAB_H_

#include <unordered_map>
#include <string>
#include "utils/status.h"

namespace milkcat {

// Segment the corpus from path and return the vocabulary of chinese words.
// If any errors occured, status is not Status::OK()
std::unordered_map<std::string, int> GetCrfVocabulary(
    const char *path,
    int *total_count,
    void (* progress)(int64_t bytes_processed,
                      int64_t file_size,
                      int64_t bytes_per_second),
    Status *status);

}  // namespace milkcat

#endif  // SRC_NEKO_CRF_VOCAB_H_
