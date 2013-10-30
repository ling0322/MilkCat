/*
 * crf_pos_tagger.h
 *
 * by ling0322 at 2013-08-20
 *
 */


#ifndef CRF_POS_TAGGER_H
#define CRF_POS_TAGGER_H

#include "term_instance.h"
#include "pos_tag_set.h"
#include "part_of_speech_tag_instance.h"
#include "crf_tagger.h"
#include "milkcat_config.h" 

class FeatureExtractor;
class PartOfSpeechFeatureExtractor;

class CRFPOSTagger {
 public:
  static CRFPOSTagger *Create(const char *model_path);
  ~CRFPOSTagger();

  void Tag(PartOfSpeechTagInstance *part_of_speech_tag_instance, const TermInstance *term_instance);

 private:
  POSTagSet *pos_tag_set_;
  CRFTagger *crf_tagger_;
  TagSequence *tag_sequence_;
  PartOfSpeechFeatureExtractor *feature_extractor_;

  CRFPOSTagger();

  DISALLOW_COPY_AND_ASSIGN(CRFPOSTagger);
};



#endif