//
// instance_data.cc --- Created at 2013-10-20
//
// The MIT License (MIT)
//
// Copyright (c) 2013 ling0322 <ling032x@gmail.com>
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


#include "instance_data.h"

InstanceData::InstanceData(int string_number, int integer_number, int capability): 
    size_(0), 
    string_data_(NULL), 
    integer_data_(NULL) {

  if (string_number != 0) {
    string_data_ = new char **[string_number];
    for (int i = 0; i < string_number; ++i) {
      string_data_[i] = new char *[capability];
      for (size_t j = 0; j < capability; ++j) {
        string_data_[i][j] = new char[kFeatureLengthMax];
      }
    }
  }
  
  if (integer_number != 0) {
    integer_data_ = new int *[integer_number];
    for (int i = 0; i < integer_number; ++i) {
      integer_data_[i] = new int[capability];
    }
  }

  string_number_ = string_number;
  integer_number_ = integer_number;
  capability_ = capability;
}

InstanceData::~InstanceData() {
  if (string_data_ != NULL) {
    for (int i = 0; i < string_number_; ++i) {
      for (size_t j = 0; j < capability_; ++j) {
        delete[] string_data_[i][j];
      }
      delete[] string_data_[i];
    } 
    delete[] string_data_;
  }

  if (integer_data_ != NULL) {
    for (int i = 0; i < integer_number_; ++i) {
      delete[] integer_data_[i];
    }
    delete[] integer_data_;
  }
}