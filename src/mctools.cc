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
// mkgram.cc --- Created at 2013-10-21
// (with) mkdict.cc --- Created at 2013-06-08
// mk_model.cc -- Created at 2013-11-08
// mctools.cc -- Created at 2014-02-21
//

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <map>
#include <string>
#include <algorithm>
#include <thread>
#include <set>
#include "common/get_vocabulary.h"
#include "utils/utils.h"
#include "utils/readable_file.h"
#include "utils/writable_file.h"
#include "milkcat/hmm_part_of_speech_tagger.h"
#include "milkcat/milkcat.h"
#include "milkcat/static_hashtable.h"
#include "milkcat/darts.h"
#include "neko/maxent_classifier.h"

namespace milkcat {

#pragma pack(1)
struct BigramRecord {
  int32_t word_left;
  int32_t word_right;
  float weight;
};

struct HMMEmitRecord {
  int32_t term_id;
  int32_t tag_id;
  float weight;
};
#pragma pack(0)

#define UNIGRAM_INDEX_FILE "unigram.idx"
#define UNIGRAM_DATA_FILE "unigram.bin"
#define BIGRAM_FILE "bigram.bin"
#define HMM_MODEL_FILE "hmm_model.bin"

// Load unigram data from unigram_file, if an error occured set status !=
// Status::OK()
std::map<std::string, double>
LoadUnigramFile(const char *unigram_file, Status *status) {
  ReadableFile *fd = ReadableFile::New(unigram_file, status);
  std::map<std::string, double> unigram_data;
  char word[1024], line[1024];
  double count, sum = 0;

  while (status->ok() && !fd->Eof()) {
    fd->ReadLine(line, sizeof(line), status);
    if (status->ok()) {
      sscanf(line, "%s %lf", word, &count);
      unigram_data[std::string(word)] += count;
      sum += count;
    }
  }

  // Calculate the weight = -log(freq / total)
  if (status->ok()) {
    for (auto it = unigram_data.begin(); it != unigram_data.end(); ++it) {
      it->second = -log(it->second / sum);
    }
  }

  delete fd;
  return unigram_data;
}

// Load bigram data from bigram_file, if an error occured set status !=
// Status::OK()
std::map<std::pair<std::string, std::string>, int>
LoadBigramFile(const char *bigram_file, int *total_count, Status *status) {
  std::map<std::pair<std::string, std::string>, int> bigram_data;
  char left[100], right[100], line[1024];
  int count;

  ReadableFile *fd = ReadableFile::New(bigram_file, status);
  *total_count = 0;

  while (status->ok() && !fd->Eof()) {
    fd->ReadLine(line, sizeof(line), status);
    if (status->ok()) {
      sscanf(line, "%s %s %d", left, right, &count);
      bigram_data[std::pair<std::string, std::string>(left, right)] += count;
      *total_count += count;
    }
  }

  delete fd;
  return bigram_data;
}

// Build Double-Array TrieTree index from unigram, and save the index and the
// unigram data file
void BuildAndSaveUnigramData(const std::map<std::string, double> &unigram_data,
                             Darts::DoubleArray *double_array,
                             Status *status) {
  std::vector<const char *> key;
  std::vector<Darts::DoubleArray::value_type> term_id;
  std::vector<float> weight;

  // term_id = 0 is reserved for out-of-vocabulary word
  int i = 1;
  weight.push_back(0.0);

  for (auto &x : unigram_data) {
    key.push_back(x.first.c_str());
    term_id.push_back(i++);
    weight.push_back(x.second);
  }

  int result = double_array->build(key.size(), &key[0], 0, &term_id[0]);
  if (result != 0) *status = Status::RuntimeError("unable to build trie-tree");

  WritableFile *fd = nullptr;
  if (status->ok()) fd = WritableFile::New(UNIGRAM_DATA_FILE, status);
  if (status->ok())
    fd->Write(weight.data(), sizeof(float) * weight.size(), status);


  if (status->ok()) {
    if (0 != double_array->save(UNIGRAM_INDEX_FILE)) {
      std::string message = "unable to save index file ";
      message += UNIGRAM_INDEX_FILE;
      *status = Status::RuntimeError(message.c_str());
    }
  }

  delete fd;
}

// Save unigram data into binary file UNIGRAM_FILE. On success, return the
// number of bigram word pairs successfully writed. On failed, set status !=
// Status::OK()
int SaveBigramBinFile(
    const std::map<std::pair<std::string, std::string>, int> &bigram_data,
    int total_count,
    const Darts::DoubleArray &double_array,
    Status *status) {
  const char *left_word, *right_word;
  int32_t left_id, right_id;
  int count;
  std::vector<int64_t> keys;
  std::vector<float> values;

  for (auto &x : bigram_data) {
    left_word = x.first.first.c_str();
    right_word = x.first.second.c_str();
    count = x.second;
    left_id = double_array.exactMatchSearch<int>(left_word);
    right_id = double_array.exactMatchSearch<int>(right_word);
    if (left_id > 0 && right_id > 0) {
      keys.push_back((static_cast<int64_t>(left_id) << 32) + right_id);
      values.push_back(-log(static_cast<double>(count) / total_count));
    }
  }

  auto hashtable = StaticHashTable<int64_t, float>::Build(
      keys.data(),
      values.data(),
      keys.size());
  hashtable->Save(BIGRAM_FILE, status);

  delete hashtable;
  return keys.size();
}

int MakeGramModel(int argc, char **argv) {
  Darts::DoubleArray double_array;
  std::map<std::string, double> unigram_data;
  std::map<std::pair<std::string, std::string>, int> bigram_data;
  Status status;

  if (argc != 4)
    status = Status::Info("Usage: mc_model gram [UNIGRAM FILE] [BIGRAM FILE]");

  const char *unigram_file = argv[argc - 2];
  const char *bigram_file = argv[argc - 1];

  if (status.ok()) {
    printf("Loading unigram data ...");
    fflush(stdout);
    unigram_data = LoadUnigramFile(unigram_file, &status);
  }

  int total_count = 0;
  if (status.ok()) {
    printf(" OK, %d entries loaded.\n", static_cast<int>(unigram_data.size()));
    printf("Loading bigram data ...");
    fflush(stdout);
    bigram_data = LoadBigramFile(bigram_file, &total_count, &status);
  }

  if (status.ok()) {
    printf(" OK, %d entries loaded.\n", static_cast<int>(bigram_data.size()));
    printf("Saveing unigram index and data file ...");
    fflush(stdout);
    BuildAndSaveUnigramData(unigram_data, &double_array, &status);
  }

  int count = 0;
  if (status.ok()) {
    printf(" OK\n");
    printf("Saving Bigram Binary File ...");
    count = SaveBigramBinFile(bigram_data, total_count, double_array, &status);
  }

  if (status.ok()) {
    printf(" OK, %d entries saved.\n", count);
    printf("Success!");
    return 0;
  } else {
    puts(status.what());
    return -1;
  }
}

int MakeIndexFile(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "Usage: mc_model dict [INPUT-FILE] [OUTPUT-FILE]\n");
    return 1;
  }

  const char *input_path = argv[argc - 2];
  const char *output_path = argv[argc - 1];

  Darts::DoubleArray double_array;

  FILE *fd = fopen(input_path, "r");
  if (fd == NULL) {
    fprintf(stderr, "error: unable to open input file %s\n", input_path);
    return 1;
  }

  char key_text[1024];
  int value;
  std::vector<std::pair<std::string, int> > key_value;
  while (fscanf(fd, "%s %d", key_text, &value) != EOF) {
    key_value.push_back(std::pair<std::string, int>(key_text, value));
  }
  sort(key_value.begin(), key_value.end());
  printf("read %ld words.\n", key_value.size());

  std::vector<const char *> keys;
  std::vector<Darts::DoubleArray::value_type> values;

  for (auto &x : key_value) {
    keys.push_back(x.first.c_str());
    values.push_back(x.second);
  }

  if (double_array.build(keys.size(), &keys[0], 0, &values[0]) != 0) {
    fprintf(stderr,
            "error: unable to build double array from file %s\n",
            input_path);
    return 1;
  }

  if (double_array.save(output_path) != 0) {
    fprintf(stderr,
            "error: unable to save double array to file %s\n",
            output_path);
    return 1;
  }

  fclose(fd);
  return 0;
}

int MakeHMMTaggerModel(int argc, char **argv) {
  Status status;
  if (argc != 7)
    status = Status::Info("Usage: mctools hmm index-file tag-set-file "
                          "trans-file emit-file binary-model-file");

  const char *index_file = argv[2];
  const char *yset_file = argv[3];
  const char *trans_file = argv[4];
  const char *emit_file = argv[5];
  const char *model_file = argv[6];

  HMMModel *hmm_model = nullptr;
  if (status.ok()) {
    hmm_model = HMMModel::NewFromText(trans_file,
                                      emit_file,
                                      yset_file,
                                      index_file,
                                      &status);
  }
  if (status.ok()) hmm_model->Save(model_file, &status);

  delete hmm_model;
  if (status.ok()) {
    return 0;
  } else {
    puts(status.what());
    return -1;
  }
}

int MakeMaxentFile(int argc, char **argv) {
  Status status;

  if (argc != 4)
    status = Status::Info("Usage: mc_model maxent "
                          "text-model-file binary-model-file");

  printf("Load text formatted model: %s \n", argv[argc - 2]);
  MaxentModel *maxent = MaxentModel::NewFromText(argv[argc - 2], &status);

  if (status.ok()) {
    printf("Save binary formatted model: %s \n", argv[argc - 1]);
    maxent->Save(argv[argc - 1], &status);
  }

  delete maxent;
  if (status.ok()) {
    return 0;
  } else {
    puts(status.what());
    return -1;
  }
}

void DisplayProgress(int64_t bytes_processed,
                     int64_t file_size,
                     int64_t bytes_per_second) {
  fprintf(stderr,
          "\rprogress %dMB/%dMB -- %2.1f%% %.3fMB/s",
          static_cast<int>(bytes_processed / (1024 * 1024)),
          static_cast<int>(file_size / (1024 * 1024)),
          100.0 * bytes_processed / file_size,
          bytes_per_second / static_cast<double>(1024 * 1024));
}

int CorpusVocabulary(int argc, char **argv) {
  Status status;
  char message[1024], 
       output_file[1024] = "",
       model_path[1024] = "",
       user_dict[1024] = "";
  int c = '\0';
  int analyzer_type = BIGRAM_SEGMENTER;

  while ((c = getopt(argc, argv, "u:d:m:o:")) != -1 && status.ok()) {
    switch (c) {
      case 'd':
        strcpy(model_path, optarg);
        if (model_path[strlen(model_path) - 1] != '/') 
          strcat(model_path, "/");
        break;

      case 'u':
        strcpy(user_dict, optarg);
        break;

      case 'o':
        strcpy(output_file, optarg);
        break;

      case 'm':
        if (strcmp(optarg, "crf_seg") == 0) {
          analyzer_type = CRF_SEGMENTER;
        } else if (strcmp(optarg, "bigram_seg") == 0) {
          analyzer_type = BIGRAM_SEGMENTER;
        } else {
          status = Status::Info("Option -m: invalid method");
        }
        break;

      case ':':
        sprintf(message, "Option -%c: requires an operand\n", optopt);
        status = Status::Info(message);
        break;

      case '?':
        sprintf(message, "Unrecognized option: -%c\n", optopt);
        status = Status::Info(message);
        break;
    }
  }

  if (status.ok() && argc - optind != 1) {
    status = Status::Info("");
  }

  if (!status.ok()) {
    if (*status.what()) puts(status.what());
    puts("Usage: mctools vocab [-m crf_seg|bigram_seg] [-d model_dir] "
         "[-u userdict] -o output_file corpus_file");
  }

  milkcat_model_t *model = nullptr;
  std::unordered_map<std::string, int> vocab;
  if (status.ok()) {
    const char *corpus_path = argv[optind];
    int total_count = 0;
    model = milkcat_model_new(*model_path == '\0'? NULL: model_path);
    if (*user_dict) milkcat_model_set_userdict(model, user_dict);
    int n_threads = std::thread::hardware_concurrency();
    vocab = GetVocabularyFromFile(corpus_path,
                                  model,
                                  analyzer_type,
                                  n_threads,
                                  &total_count,
                                  DisplayProgress,
                                  &status);
  }

  WritableFile *fd = nullptr;
  if (status.ok()) {
    fd = WritableFile::New(output_file, &status);
  }

  if (status.ok()) {
    for (auto &x: vocab) {
      sprintf(message, "%s %d", x.first.c_str(), x.second);
      fd->WriteLine(message, &status);
      if (!status.ok()) break;
    }
  }

  delete fd;
  milkcat_model_destroy(model);

  if (status.ok()) {
    puts("Success!");
    return 0;
  } else {
    if (*status.what()) puts(status.what());
    return -1;
  }
}

}  // namespace milkcat

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: mc_model [dict|gram|hmm|maxent]\n");
    return 1;
  }

  char *tool = argv[1];

  if (strcmp(tool, "dict") == 0) {
    return milkcat::MakeIndexFile(argc, argv);
  } else if (strcmp(tool, "gram") == 0) {
    return milkcat::MakeGramModel(argc, argv);
  } else if (strcmp(tool, "hmm") == 0) {
    return milkcat::MakeHMMTaggerModel(argc, argv);
  } else if (strcmp(tool, "maxent") == 0) {
    return milkcat::MakeMaxentFile(argc, argv);
  } else if (strcmp(tool, "vocab") == 0) {
    return milkcat::CorpusVocabulary(argc - 1, argv + 1);
  } else {
    fprintf(stderr, "Usage: mc_model [dict|gram|hmm|maxent|vocab]\n");
    return 1;
  }

  return 0;
}

