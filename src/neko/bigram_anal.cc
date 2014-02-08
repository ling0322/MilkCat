//
// bigram_anal.cc --- Created at 2014-01-30
//
// The MIT License (MIT)
//
// Copyright 2014 ling0322 <ling032x@gmail.com>
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

#include <unordered_map>
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include <utility>
#include <math.h>
#include <stdio.h>
#include "utils/readable_file.h"
#include "utils/status.h"
#include "milkcat/milkcat.h"

struct Adjacent {
  std::map<int, int> left;
  std::map<int, int> right;
};

int GetOrInsert(std::unordered_map<std::string, int> &dict, const std::string &word) {
  auto it = dict.find(word);
  if (it == dict.end()) {
    dict.insert(std::pair<std::string, int>(word, dict.size()));
    return dict.size();
  } else {
    return it->second;
  }
}

// Segment a string specified by text and return as a vector.
std::vector<std::string> SegmentText(
      const char *text, 
      milkcat_t *analyzer) {

  std::vector<std::string> line_words;

  milkcat_cursor_t cursor = milkcat_analyze(analyzer, text);
  milkcat_item_t item;

  while (milkcat_cursor_get_next(&cursor, &item) == MC_OK) {
    if (item.word_type == MC_CHINESE_WORD) {
      line_words.push_back(item.word);
    } else {
      line_words.push_back("-NOT-CJK-");
    }
  }

  return line_words;
} 

// Update the word_adjacent data from words specified by line_words
void UpdateAdjacent(const std::vector<int> &line_words,
                    int candidate_size,
                    std::vector<Adjacent> &word_adjacent) {
  int word_id;
  for (auto it = line_words.begin(); it != line_words.end(); ++it) {
    word_id = *it;
    if (word_id < candidate_size) {
      // OK, find a candidate word

      if (it != line_words.begin()) 
        word_adjacent[word_id].left[*(it - 1)] += 1;

      if (it != line_words.end() - 1) 
        word_adjacent[word_id].right[*(it + 1)] += 1;
    }
  }
}

// Calculates the adjacent entropy from word's adjacent word data
// specified by adjacent
double CalculateAdjacentEntropy(const std::map<int, int> &adjacent) {
  double entropy = 0, probability;
  int total_count = 0;
  for (auto &x: adjacent) {
    total_count += x.second;
  }

  for (auto &x: adjacent) {
    probability = static_cast<double>(x.second) / total_count;
    entropy += -probability * log(probability);
  }

  return entropy;
}

// Its a thread that using bigram to segment the text from fd, and update the
// adjacent_entropy and the vocab data
void BigramAnalyzeThread(milkcat_model_t *model,
                         ReadableFile *fd,
                         int candidate_size,
                         std::mutex &fd_mutex,
                         std::vector<Adjacent> &word_adjacent,
                         std::unordered_map<std::string, int> &vocab,
                         std::unordered_map<std::string, int> &dict,
                         std::mutex &update_mutex,
                         Status &status) {

  milkcat_t *analyzer = milkcat_new(model, BIGRAM_SEGMENTER);
  if (analyzer == nullptr) status = Status::Corruption(milkcat_last_error());

  int buf_size = 1024 * 1024;
  char *buf = new char[buf_size];

  bool eof = false;
  std::vector<std::string> words;
  std::vector<int> word_ids;
  while (status.ok() && !eof) {
    fd_mutex.lock();
    eof = fd->Eof();
    if (!eof) fd->ReadLine(buf, buf_size, status);
    fd_mutex.unlock();

    if (status.ok() && !eof) {

      // Using bigram model to segment the corpus
      words = SegmentText(buf, analyzer);

      // Now start to update the data
      word_ids.clear();
      update_mutex.lock();
      for (auto &word: words) {
        vocab[word] += 1;
        word_ids.push_back(GetOrInsert(dict, word));
      }
      UpdateAdjacent(word_ids, candidate_size, word_adjacent);
      update_mutex.unlock();
    }
  }

  milkcat_destroy(analyzer);
  delete buf;
}

// Use bigram segmentation to analyze a corpus. Candidate to analyze is specified by 
// candidate, and the corpus is specified by corpus_path. It would use a temporary file
// called 'candidate_cost.txt' as user dictionary file for MilkCat.
// On success, stores the adjecent entropy in adjacent_entropy, and stores the vocabulary
// of segmentation's result in vocab. On failed set status != Status::OK()
void BigramAnalyze(const std::unordered_map<std::string, float> &candidate,
                   const char *corpus_path,
                   std::unordered_map<std::string, double> &adjacent_entropy,
                   std::unordered_map<std::string, int> &vocab,
                   Status &status) {

  std::unordered_map<std::string, int> dict;
  dict.reserve(500000);

  int candidate_size = candidate.size();
  std::vector<Adjacent> word_adjacent(candidate_size);

  // Put each candidate word into dictionary and assign its word-id
  for (auto &x: candidate) {
    dict.insert(std::pair<std::string, int>(x.first, dict.size()));
  }

  // The model file
  milkcat_model_t *model = milkcat_model_new(nullptr);
  milkcat_model_set_userdict(model, "candidate_cost.txt");
  
  ReadableFile *fd = nullptr;
  if (status.ok()) {
    fd = ReadableFile::New(corpus_path, status);
  }

  if (status.ok()) {
    int n_threads = std::thread::hardware_concurrency();
    std::vector<Status> status_vec(n_threads);
    std::vector<std::thread> threads;
    std::mutex update_mutex, fd_mutex;

    for (int i = 0; i < n_threads; ++i) {
      threads.push_back(std::thread(BigramAnalyzeThread,
                                    model,
                                    fd,
                                    candidate_size,
                                    std::ref(fd_mutex),
                                    std::ref(word_adjacent),
                                    std::ref(vocab),
                                    std::ref(dict),
                                    std::ref(update_mutex),
                                    std::ref(status_vec[i])));
    }

    // Synchronizing all threads
    for (auto &th: threads) th.join();

    // Set the status
    for (auto &st: status_vec) {
      if (!st.ok()) status = st;
    }
  }

  if (status.ok()) {
    std::vector<std::string> id_to_str(dict.size());
    for (auto &x: dict) id_to_str[x.second] = x.first;

    // Calculate the candidates' adjacent entropy and store in adjacent_entropy
    for (auto it = word_adjacent.begin(); it != word_adjacent.end(); ++it) {
      double left_entropy = CalculateAdjacentEntropy(it->left);
      double right_entropy = CalculateAdjacentEntropy(it->right);
      adjacent_entropy[id_to_str[it - word_adjacent.begin()]] = std::min(left_entropy, right_entropy);
    }
  }

  delete fd;
  milkcat_model_destroy(model);
}
