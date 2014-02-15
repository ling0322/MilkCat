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
// configuration.h --- Created at 2013-11-08
//

#ifndef SRC_MILKCAT_CONFIGURATION_H_
#define SRC_MILKCAT_CONFIGURATION_H_

#include <string>
#include <map>
#include "utils/utils.h"

namespace milkcat {

class Configuration {
 public:
  Configuration();

  // Load the configuration file of path
  static Configuration *New(const char *path, Status *status);

  // Get integer value by key, default is 0
  int GetInteger(const char *key) const;

  // Get string value by key, default is ""
  const char *GetString(const char *key) const;

  // Check if the key exists in configuration file
  bool HasKey(const char *key) const;

  // Set the integer value of key
  void SetInteger(const char *key, int value);

  // Set the string value of key
  void SetString(const char *key, const char * value);

  // Save the configuration into file
  bool SaveToPath(const char *path);

 private:
  std::map<std::string, std::string> data_;
};

}  // namespace milkcat

#endif  // SRC_MILKCAT_CONFIGURATION_H_
