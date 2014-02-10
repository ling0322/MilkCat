//
// crf_segmenet_vocabulary.cc --- Created at 2014-01-28
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

#include <stdio.h>
#include <stdint.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include "milkcat/milkcat.h"
#include "utils/readable_file.h"
#include "utils/status.h"


// To analyze the corpus in multi-threading
void CrfSegmentThread(milkcat_model_t *model,
                      ReadableFile *fd, 
                      std::mutex &fd_mutex, 
                      std::unordered_map<std::string, int> &vocab,
                      int &total_count,
                      std::mutex &vocab_mutex,
                      Status *status) {

  int buf_size = 1024 * 1024;
  char *buf = new char[buf_size];

  milkcat_t *analyzer = milkcat_new(model, CRF_SEGMENTER);
  milkcat_item_t item;
  milkcat_cursor_t cursor;
  if (analyzer == nullptr) *status = Status::Corruption(milkcat_last_error());

  bool eof = false;
  std::vector<std::string> words;
  while (status->ok() && !eof) {

    // Read a line from corpus
    fd_mutex.lock();
    eof = fd->Eof();
    if (!eof) fd->ReadLine(buf, buf_size, status);
    fd_mutex.unlock();

    // Segment the line and store the results into words
    if (status->ok() && !eof) {
      words.clear();
      cursor = milkcat_analyze(analyzer, buf);
      while (milkcat_cursor_get_next(&cursor, &item)) {
        if (item.word_type == MC_CHINESE_WORD)
          words.push_back(item.word);
        else
          words.push_back("-NOT-CJK-");
      }
    }

    // Update vocab and total_count with words
    if (status->ok() && !eof) {
      vocab_mutex.lock();
      for (auto &word: words) {
        vocab[word] += 1;
      }
      total_count += words.size();
      vocab_mutex.unlock();
    }
  }

  milkcat_destroy(analyzer);
  delete[] buf;
}

// Thread to update progress information via calling callback function progress
void ProgressUpdateThread(
    ReadableFile *fd, 
    std::mutex &fd_mutex,
    std::atomic_bool &task_finished,
    void (* progress)(int64_t bytes_processed, 
                      int64_t file_size, 
                      int64_t bytes_per_second)) {

  int64_t file_size = fd->Size();     
  int64_t last_bytes_processed = 0,
          bytes_processed = 0;

  while (task_finished.load() == false) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    last_bytes_processed = bytes_processed;
    fd_mutex.lock();
    bytes_processed = fd->Tell();
    fd_mutex.unlock();
    progress(bytes_processed, 
             file_size, 
             bytes_processed - last_bytes_processed);
  }  
}

// Segment the corpus from path and return the vocabulary of chinese words.
// If any errors occured, status is not Status::OK()
std::unordered_map<std::string, int> GetCrfVocabulary(
    const char *path, 
    int &total_count,
    void (* progress)(int64_t bytes_processed, 
                      int64_t file_size, 
                      int64_t bytes_per_second), 
    Status *status) {
  std::unordered_map<std::string, int> vocab;
  milkcat_model_t *model = milkcat_model_new(NULL);
  total_count = 0;

  ReadableFile *fd;
  if (status->ok()) fd = ReadableFile::New(path, status);

  if (status->ok()) {
    int64_t file_size = fd->Size();  

    // Thread number = CPU core number
    int n_threads = std::thread::hardware_concurrency();
    std::vector<Status> status_vec(n_threads);
    std::vector<std::thread> threads;
    std::thread progress_thread;
    std::mutex fd_mutex, vocab_mutex;
    std::atomic_bool task_finished;

    for (int i = 0; i < n_threads; ++i) {
      threads.push_back(std::thread(CrfSegmentThread, 
                                    model, 
                                    fd, 
                                    std::ref(fd_mutex), 
                                    std::ref(vocab), 
                                    std::ref(total_count), 
                                    std::ref(vocab_mutex), 
                                    &status_vec[i]));
    }

    // Synchronizing all threads, call progress function per second

    if (progress) {
      progress_thread = std::thread(ProgressUpdateThread, 
                                    fd, 
                                    std::ref(fd_mutex), 
                                    std::ref(task_finished), 
                                    progress);
    }
    
    for (auto &th: threads) th.join();
    task_finished.store(true);
    if (progress) progress_thread.join();

    // Set the status
    for (auto &st: status_vec) {
      if (!st.ok()) *status = st;
    }
  }

  delete fd;
  milkcat_model_destroy(model);

  return vocab;
}
