//
// static_array.h --- Created at 2013-12-06
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

#ifndef SRC_MILKCAT_STATIC_ARRAY_H_
#define SRC_MILKCAT_STATIC_ARRAY_H_

#include <assert.h>
#include <stdio.h>
#include "utils/utils.h"
#include "utils/readable_file.h"

template<class T>
class StaticArray {
 public:
  static StaticArray *New(const char *file_path, Status *status) {
    int type_size = sizeof(T);
    StaticArray *self = new StaticArray();
    ReadableFile *fd = ReadableFile::New(file_path, status);

    if (status->ok()) {
      if (fd->Size() % type_size != 0) *status = Status::Corruption(file_path);
    }

    if (status->ok()) {
      self->data_ = new T[fd->Size() / type_size];
      self->size_ = fd->Size() / type_size;
    }

    if (status->ok()) fd->Read(self->data_, fd->Size(), status);

    if (fd != NULL) delete fd;
    if (status->ok()) {
      return self;
    } else {
      delete self;
      return NULL;
    }
  }

  // Create a StaticArray<T> from an array specified by ptr, this function
  // will copy the data of ptr into this->data_
  static StaticArray *NewFromArray(T *ptr, int size) {
    StaticArray *self = new StaticArray();
    self->size_ = size;
    self->data_ = new T[size];
    memcpy(self->data_, ptr, sizeof(T) * size);

    return self;
  }

  StaticArray(): data_(NULL), size_(0) {}

  ~StaticArray() {
    delete[] data_;
    data_ = NULL;
  }

  T get(int position) const {
    assert(position < size_);
    return data_[position];
  }

  int size() const { return size_; }

 private:
  T *data_;
  int size_;

  DISALLOW_COPY_AND_ASSIGN(StaticArray);
};

#endif  // SRC_MILKCAT_STATIC_ARRAY_H_
