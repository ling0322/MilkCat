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
// hmm_part_of_speech_tagger.h --- Created at 2013-11-08
//

#ifndef SRC_MILKCAT_HMM_PART_OF_SPEECH_TAGGER_H_
#define SRC_MILKCAT_HMM_PART_OF_SPEECH_TAGGER_H_

#include "milkcat/beam.h"
#include "milkcat/crf_part_of_speech_tagger.h"
#include "milkcat/darts.h"
#include "milkcat/hmm_model.h"
#include "milkcat/libmilkcat.h"
#include "milkcat/milkcat_config.h"
#include "milkcat/part_of_speech_tagger.h"
#include "milkcat/trie_tree.h"
#include "utils/utils.h"

namespace milkcat {

class PartOfSpeechTagInstance;
class TermInstance;
class Configuration;

class CRFEmitGetter {
 public:
  static CRFEmitGetter *New(ModelFactory *model_factory, Status *status);
  ~CRFEmitGetter();

  // Get the emit link list of term specified by position in term_instance
  // using CRF model. The emit nodes alloced by this function should release by
  // calling CRFEmitGetter::ReleaseAllEmits()
  HMMModel::Emit *GetEmits(TermInstance *term_instance, int position);

  // Release all emit nodes alloced by GetEmits
  void ReleaseAllEmits() { pool_top_ = 0; }

 private:
  std::vector<HMMModel::Emit *> emit_pool_;
  int pool_top_;
  PartOfSpeechFeatureExtractor *feature_extractor_;

  CRFPartOfSpeechTagger *crf_part_of_speech_tagger_;
  CRFTagger *crf_tagger_;

  double *probabilities_;
  int *crf_to_hmm_tag_;
  int crf_tag_num_;

  CRFEmitGetter();

  HMMModel::Emit *AllocEmit() {
    if (pool_top_ < emit_pool_.size()) {
      return emit_pool_[pool_top_++];
    } else {
      HMMModel::Emit *emit = new HMMModel::Emit(0, 0.0, nullptr);
      pool_top_++;
      emit_pool_.push_back(emit);
      return emit;
    }
  }
};

class HMMPartOfSpeechTagger: public PartOfSpeechTagger {
 public:
  struct Node;

  // Beam size, two position for BOS node
  static const int kMaxBeams = kTokenMax + 2;
  static const int kBeamSize = 3;

  ~HMMPartOfSpeechTagger();
  void Tag(PartOfSpeechTagInstance *part_of_speech_tag_instance,
           TermInstance *term_instance);

  static HMMPartOfSpeechTagger *New(ModelFactory *model_factory,
                                    Status *status);

 private:
  Beam<Node> *beams_[kMaxBeams];
  NodePool<Node> *node_pool_;

  const HMMModel *model_;
  const TrieTree *index_;
  CRFEmitGetter *crf_emit_getter_;

  HMMModel::Emit *PU_emit_;
  HMMModel::Emit *DT_emit_;

  int BOS_tagid_;
  int NN_tagid_;

  TermInstance *term_instance_;

  HMMPartOfSpeechTagger();

  // Build the beam
  void BuildBeam(int position);

  void AddBOSNodeToBeam();

  void GetBestPOSTagFromBeam(
      PartOfSpeechTagInstance *part_of_speech_tag_instance);

  HMMModel::Emit *GetEmitAtPosition(int position);

  void GuessTag(const Node *leftleft_node, 
                const Node *left_node, 
                Beam<Node> *beam);

  DISALLOW_COPY_AND_ASSIGN(HMMPartOfSpeechTagger);
};

}  // namespace milkcat

#endif  // SRC_MILKCAT_HMM_PART_OF_SPEECH_TAGGER_H_
