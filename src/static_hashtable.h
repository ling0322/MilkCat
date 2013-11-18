//
// static_hashtable.h --- Created at 2013-11-14
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


#ifndef STATIC_HASHTABLE_H
#define STATIC_HASHTABLE_H

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <exception>
#include <stdexcept>
#include <iostream>
#include <string>

template <class K, class V>
class StaticHashTable {
 public:
  static const StaticHashTable *Build(const K *keys, const V *values, int size, double load_factor = 0.5) {
    StaticHashTable *self = new StaticHashTable();

    self->bucket_size_ = static_cast<int>(size / load_factor);
    self->buckets_ = new Bucket[self->bucket_size_];
    for (int i = 0; i < self->bucket_size_; ++i) {
      self->buckets_[i].empty = true;
    }

    int position;
    for (int i = 0; i < size; ++i) {
      position = self->FindPosition(keys[i]);
      self->buckets_[position].key = keys[i];
      self->buckets_[position].value = values[i];
      self->buckets_[position].empty = false;
    }

    self->data_size_ = size;
    return self;
  }

  // Load hash table from file
  static const StaticHashTable *Load(const char *file_path) {
    StaticHashTable *self = new StaticHashTable();
    char *buffer = NULL; 
    FILE *fd;

    try {
      fd = fopen(file_path, "rb");
      if (fd == NULL) throw std::runtime_error(std::string("unable to open file ") + file_path);

      fseek(fd, 0, SEEK_END);
      int file_size = ftell(fd);
      fseek(fd, 0, SEEK_SET);

      int32_t magic_number;
      if (1 != fread(&magic_number, sizeof(int32_t), 1, fd)) 
        throw std::runtime_error(std::string("unable to read from file ") + file_path);
      if (magic_number != 0x3321) throw std::runtime_error(std::string("invaild hash table file ") + file_path);

      int32_t bucket_size;
      if (1 != fread(&bucket_size, sizeof(int32_t), 1, fd)) 
        throw std::runtime_error(std::string("unable to read from file ") + file_path);

      int32_t data_size;
      if (1 != fread(&data_size, sizeof(int32_t), 1, fd)) 
        throw std::runtime_error(std::string("unable to read from file ") + file_path);

      int32_t serialize_size;
      if (1 != fread(&serialize_size, sizeof(int32_t), 1, fd))
        throw std::runtime_error(std::string("unable to read from file ") + file_path);

      if (kSerializeBukcetSize != serialize_size)
        throw std::runtime_error(std::string("invaild hash table file ") + file_path);

      buffer = new char[kSerializeBukcetSize * data_size];
      if (data_size != fread(buffer, kSerializeBukcetSize, data_size, fd)) 
        throw std::runtime_error(std::string("invaild hash table file ") + file_path);
      
      // Get a char to reach EOF
      fgetc(fd);
      if (!feof(fd)) throw std::runtime_error(std::string("invaild hash table file ") + file_path);

      self->buckets_ = new Bucket[bucket_size];
      self->bucket_size_ = bucket_size;
      self->data_size_ = data_size;

      for (int i = 0; i < self->bucket_size_; ++i) {
        self->buckets_[i].empty = true;
      }

      char *p = buffer, *p_end = buffer + kSerializeBukcetSize * data_size;
      int32_t position;
      K key;
      V value;
      for (int i = 0; i < data_size; ++i) {
        self->Desericalize(position, key, value, p, p_end - p);
        self->buckets_[position].key = key;
        self->buckets_[position].value = value;
        self->buckets_[position].empty = false;
        p += kSerializeBukcetSize;
      }

      delete[] buffer;
      buffer = NULL;
      fclose(fd);
      return self;

    } catch (std::exception &ex) {
      puts(ex.what());

      if (buffer != NULL) delete[] buffer;
      if (fd != NULL) fclose(fd);
      delete self;
      return NULL;
    } 

  }

  // Save the hash table into file
  bool Save(const char *file_path) const {
    FILE *fd;
    char buffer[kSerializeBukcetSize];

    try {
      fd = fopen(file_path, "w");
      if (fd == NULL) throw std::runtime_error(std::string("unable to write to file ") + file_path);

      int32_t magic_number = 0x3321;
      if (1 != fwrite(&magic_number, sizeof(int32_t), 1, fd)) 
        throw std::runtime_error(std::string("unable to write to file ") + file_path);

      int32_t bucket_size = bucket_size_;
      if (1 != fwrite(&bucket_size, sizeof(int32_t), 1, fd)) 
        throw std::runtime_error(std::string("unable to write to file ") + file_path);

      int32_t data_size = data_size_;
      if (1 != fwrite(&data_size, sizeof(int32_t), 1, fd)) 
        throw std::runtime_error(std::string("unable to write to file ") + file_path);

      int32_t serialize_size = kSerializeBukcetSize;
      if (1 != fwrite(&serialize_size, sizeof(int32_t), 1, fd)) 
        throw std::runtime_error(std::string("unable to write to file ") + file_path);

      for (int i = 0; i < bucket_size_; ++i) {
        if (buckets_[i].empty == false) {
          Serialize(i, buckets_[i].key, buckets_[i].value, buffer, sizeof(buffer));
          if (1 != fwrite(buffer, kSerializeBukcetSize, 1, fd)) 
            throw std::runtime_error(std::string("unable to write to file ") + file_path);
        }
      }

      fclose(fd);
      return true;

    } catch (std::exception &ex) {
      puts(ex.what());

      if (fd != NULL) fclose(fd);
      return false;
    }
  }

  // Find the value by key in hash table if exist return a const pointer to the value
  // else return NULL
  const V *Find(const K &key) const {
    int position = FindPosition(key);
    if (buckets_[position].empty) {
      return NULL;
    } else {
      return &(buckets_[position].value);
    }
  }

  ~StaticHashTable() {
    if (buckets_ != NULL) {
      delete buckets_;
      buckets_ = NULL;
    }
  }

 private:
  struct Bucket {
    K key;
    V value;
    bool empty;
  };

  Bucket *buckets_;

  // How many key-value pairs in hash table
  int data_size_;
  int bucket_size_;

  StaticHashTable(): buckets_(NULL), bucket_size_(0), data_size_(0) {}

  // Serialize size of each bucket
  static const int kSerializeBukcetSize = sizeof(int32_t) + sizeof(K) + sizeof(V);

  // Serialize a Bucket into buffer
  void Serialize(int32_t position, const K &key, const V &value, void *buffer, int buffer_size) const {
    assert(kSerializeBukcetSize <= buffer_size);
    
    char *p = reinterpret_cast<char *>(buffer);
    *reinterpret_cast<int32_t *>(p) = position;

    p += sizeof(int32_t);
    *reinterpret_cast<K *>(p) = key;

    p += sizeof(K);
    *reinterpret_cast<V *>(p) = value;
  }

  void Desericalize(int32_t &position, K &key, V &value, const void *buffer, int buffer_size) const {
    assert(kSerializeBukcetSize <= buffer_size);

    const char *p = reinterpret_cast<const char *>(buffer);
    position = *reinterpret_cast<const int32_t *>(p);

    p += sizeof(int32_t);
    key = *reinterpret_cast<const K *>(p);

    p += sizeof(K);
    value = *reinterpret_cast<const V *>(p);
  }

  int FindPosition(const K &key) const {
    int original = JSHash(key) % bucket_size_,
        position = original;
    for (int i = 1; ; i++) {
      if (buckets_[position].empty) {
        
        // key not exists in hash table
        return position;
      } else if (buckets_[position].key == key) {

        // OK, find the key
        return position;
      } else {
        position = (position + i) % bucket_size_;
        assert(i < bucket_size_);
      }
    }
  }
  
  // The JS hash function
  int JSHash(const K &key) const {
    const char *p = reinterpret_cast<const char *>(&key);
    uint32_t b = 378551;
    uint32_t a = 63689;
    uint32_t hash = 0;

    for (int i = 0; i < sizeof(K); ++i) {
      hash = hash * a + (*p++);
      a *= b;
    }

    hash = hash & 0x7FFFFFFF;

    return static_cast<int>(hash);
  }
};


#endif