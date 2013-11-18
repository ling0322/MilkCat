/*
 * crf_segmenter.h
 *
 * by ling0322 at 2013-08-17
 *
 */

#ifndef CRF_SEGMENTER_H
#define CRF_SEGMENTER_H

#include "crf_tagger.h"
#include "utils.h"
#include "token_instance.h"

class SegmentFeatureExtractor;
class TermInstance;

class CRFSegmenter {
 public:
  static CRFSegmenter *Create(const char *model_path);
  ~CRFSegmenter();

  // Segment a range [begin, end) of token 
  void SegmentRange(TermInstance *term_instance, const TokenInstance *token_instance, int begin, int end);

  void Segment(TermInstance *term_instance, const TokenInstance *token_instance) {
  	SegmentRange(term_instance, token_instance, 0, token_instance->size());
  }
 
 private:
  CRFTagger *crf_tagger_;
  CRFModel *crf_model_;

  SegmentFeatureExtractor *feature_extractor_;

  int S, B, B1, B2, M, E;

  CRFSegmenter();

  DISALLOW_COPY_AND_ASSIGN(CRFSegmenter);
};

#endif