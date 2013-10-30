/*
 * named_entity_recognitioin.cc
 * out_of_vocabulary_word_recognitioin.cc (2013-09-03)
 *
 * by ling0322 at 2013-08-27
 *
 */

#include <stdio.h>
#include <string.h>
#include "darts.h"
#include "utils.h"
#include "out_of_vocabulary_word_recognitioin.h"
#include "token_instance.h"
#include "crf_segmenter.h"
#include "crf_pos_tagger.h"

OutOfVocabularyWordRecognition *OutOfVocabularyWordRecognition::Create(const char *crf_segment_model_path, 
                                                                       const char *ner_filter_word_path) {
  char error_message[2048];
  OutOfVocabularyWordRecognition *self = new OutOfVocabularyWordRecognition();

  self->crf_segmenter_ = CRFSegmenter::Create(crf_segment_model_path);
  if (self->crf_segmenter_ == NULL) {
    delete self;
    return NULL;
  }

  self->offset_instance_ = new OffsetTokenInstance();
  self->term_instance_ = new TermInstance();
  self->double_array_ = new Darts::DoubleArray();
  
  if (-1 == self->double_array_->open(ner_filter_word_path)) {
    delete self;
    sprintf(error_message, "unable to open OOV filter word double array file %s", ner_filter_word_path);
    set_error_message(error_message);
    return NULL;
  }

  return self;
}

OutOfVocabularyWordRecognition::~OutOfVocabularyWordRecognition() {
  if (crf_segmenter_ != NULL) {
    delete crf_segmenter_;
    crf_segmenter_ = NULL;
  }

  if (offset_instance_ != NULL) {
    delete offset_instance_;
    offset_instance_ = NULL;
  }

  if (term_instance_ != NULL) {
    delete term_instance_;
    term_instance_ = NULL;
  }

  if (double_array_ != NULL) {
    delete double_array_;
    double_array_ = NULL;
  }
}

OutOfVocabularyWordRecognition::OutOfVocabularyWordRecognition(): offset_instance_(NULL),
                                                                  term_instance_(NULL),
                                                                  crf_segmenter_(NULL),
                                                                  double_array_(NULL) {
}

void OutOfVocabularyWordRecognition::Process(TermInstance *term_instance,
                                             const TermInstance *in_term_instance, 
                                             const TokenInstance *in_token_instance) {
  int ner_begin_token = 0;
  int current_token = 0;
  int current_term = 0;
  int term_token_number;
  int current_token_type;
  int ner_term_number = 0;
  const char *term_str;
  const char *pos_tag;

  for (size_t i = 0; i < in_term_instance->size(); ++i) {
    term_token_number = in_term_instance->token_number_at(i);
    current_token_type = in_token_instance->token_type_at(current_token);
    term_str = in_term_instance->term_text_at(i);
    // printf("%d\n", ner_term_number);

    if (term_token_number > 1 || 
        current_token_type != TokenInstance::kChineseChar ||
        IsFiltered(term_str) == true) {

      // Recognize token from ner_begin_token to current_token
      if (ner_term_number > 1) {
        // printf("ner_term_number %d\n", ner_term_number);
        RecognizeRange(in_token_instance, ner_begin_token, current_token);
        
        for (size_t j = 0; j < term_instance_->size(); ++j) {
          CopyTermValue(term_instance, current_term, term_instance_, j);
          current_term++;
        }
      } else if (ner_term_number == 1) {
        CopyTermValue(term_instance, current_term, in_term_instance, i - 1);
        current_term++;
      }

      // printf("%d\n", i);

      CopyTermValue(term_instance, current_term, in_term_instance, i);
      ner_begin_token = current_token + term_token_number;
      current_term++;
      ner_term_number = 0;

    } else {
      ner_term_number++;
    }

    current_token += term_token_number;
  } 
  
  // Recognize remained tokens
  if (ner_term_number > 1) {
    RecognizeRange(in_token_instance, ner_begin_token, current_token);
    for (size_t j = 0; j < term_instance_->size(); ++j) {
      CopyTermValue(term_instance, current_term, term_instance_, j);
      current_term++;
    }
    ner_begin_token = current_token + term_token_number;
  } else if (ner_term_number == 1) {
    CopyTermValue(term_instance, current_term, in_term_instance, in_term_instance->size() - 1);
    current_term++;
  }

  term_instance->set_size(current_term);
}

void OutOfVocabularyWordRecognition::CopyTermValue(TermInstance *dest_term_instance, 
                                                   int dest_postion, 
                                                   const TermInstance *src_term_instance, 
                                                   int src_position)  {

  dest_term_instance->set_value_at(dest_postion,
                                   src_term_instance->term_text_at(src_position),
                                   src_term_instance->token_number_at(src_position),
                                   src_term_instance->term_type_at(src_position));
}

void OutOfVocabularyWordRecognition::RecognizeRange(const TokenInstance *token_instance, int begin, int end) {
  printf("rgn %d %d\n", begin, end);
  offset_instance_->Update(token_instance, begin, end);
  crf_segmenter_->Segment(term_instance_, offset_instance_);
  printf("s %d\n", term_instance_->size());
  for (int i = 0; i < term_instance_->size(); ++i) {
    puts(term_instance_->term_text_at(i));
  }
}