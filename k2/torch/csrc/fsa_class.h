/**
 * @brief python wrapper for Ragged<Arc>
 *
 * @copyright
 * Copyright      2021  Xiaomi Corp.  (authors: Wei Kang, Fangjun Kuang)
 *
 * @copyright
 * See LICENSE for clarification regarding multiple authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef K2_TORCH_CSRC_FSA_CLASS_H_
#define K2_TORCH_CSRC_FSA_CLASS_H_

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "k2/csrc/fsa.h"
#include "k2/csrc/ragged.h"
#include "torch/script.h"

namespace k2 {

// It is a wrapper of Ragged<Arc> to support attributes propagation
struct FsaClass {
  Ragged<Arc> fsa;
  int32_t properties = 0;

  /// It contains all tensor attributes of this FSA
  std::unordered_map<std::string, torch::Tensor> tensor_attrs;

  /// It contains all ragged tensor attributes of this FSA
  std::unordered_map<std::string, Ragged<int32_t>> ragged_tensor_attrs;

  /// The name of all attributes of this FSA
  std::unordered_set<std::string> all_attr_names;

  // The default constructor initializes an invalid FSA.
  FsaClass() = default;

  explicit FsaClass(const Ragged<Arc> &fsa) : fsa(fsa) {}
  FsaClass(const Ragged<Arc> &fsa, torch::Tensor aux_labels);
  FsaClass(const Ragged<Arc> &fsa, Ragged<int32_t> &aux_labels);

  // TODO: support more options, e.g.,
  /* Construct a FsaClass from a string.
     @param s  The input string that can be passed to FsaFromString
     @param extra_label_names A list of strings specifying the names of
                extra labels. If it is empty, then the string represents
                an acceptor.
   */
  FsaClass(const std::string &s,
           const std::vector<std::string> &extra_label_names = {});

  FsaClass(const FsaClass &other) = default;

  FsaClass &operator=(const FsaClass &other) = default;

  FsaClass(FsaClass &&other) = default;

  FsaClass &operator=(FsaClass &&other) = default;

  /**
    Create an Fsa object, including propagating properties from the source FSA.
    This is intended to be called from unary functions on FSAs where the arc_map
    is a Tensor of int32 (i.e. not ragged).
    @param src The source Fsa, i.e. the arg to the unary function.
    @param arcs The raw output of the unary function, as output by whatever C++
                algorithm we used.
    @param arc_map A map from arcs in `arcs` to the corresponding arc-index in
                   `src`, or -1 if the arc had no source arc
                   (e.g. added epsilon self-loops).
   */
  static FsaClass FromUnaryFunctionTensor(FsaClass &src,
                                          const Ragged<Arc> &arcs,
                                          torch::Tensor arc_map);

  /**
    Create an Fsa object, including propagating properties from the source FSA.
    This is intended to be called from unary functions on FSAs where the arc_map
    is an instance of k2.RaggedTensor (with dtype torch.int32).
    @param src  The source Fsa, i.e. the arg to the unary function.
    @param arcs The raw output of the unary function, as output by whatever C++
                 algorithm we used.
    @param arc_map A map from arcs in `arcs` to the corresponding arc-index in
                   `src`, or -1 if the arc had no source arc
                   (e.g. :func:`remove_epsilon`).
   */
  static FsaClass FromUnaryFunctionRagged(FsaClass &src,
                                          const Ragged<Arc> &arcs,
                                          Ragged<int32_t> &arc_map);

  /**
    Create an Fsa object, including propagating properties from the source FSAs.
    This is intended to be called from binary functions on FSAs where the
    arc_map is a Tensor of int32 (i.e. not ragged).
    Caution: Only the attributes with dtype `torch.float32` will be merged,
             other kinds of attributes with the same name are discarded.
    @param a_src  The source Fsa, i.e. the arg to the binary function.
    @param b_src  The other source Fsa.
    @param arcs The raw output of the binary function, as output by whatever C++
                algorithm we used.
    @param a_arc_map A map from arcs in `arcs` to the corresponding
                     arc-index in `a_fsa` or -1 if the arc had no source arc
                     (e.g. added epsilon self-loops).
    @param a_arc_map A map from arcs in `dest_arcs` to the corresponding
                     arc-index in `b_fsa` or -1 if the arc had no source arc
                     (e.g. added epsilon self-loops).
   */
  static FsaClass FromBinaryFunctionTensor(FsaClass &a_src, FsaClass &b_src,
                                           const Ragged<Arc> &arcs,
                                           torch::Tensor a_arc_map,
                                           torch::Tensor b_arc_map);

  /* Return a 1-D float32 torch tensor.
      @caution You should not modify the returned tensor since it shares
      the underlying memory with this FSA.
     */
  torch::Tensor Scores();
  // Set scores, will modify scores in fsa.arcs
  void SetScores(torch::Tensor scores);

  /* Return a 1-D int32 torch tensor.
      @caution You should not modify the returned tensor since it shares
      the underlying memory with this FSA.
     */
  torch::Tensor Labels() /*const*/;
  // Set labels, will modify labels in fsa.arcs
  void SetLabels(torch::Tensor labels);

  /* Return a 2-D int32 torch tensor.
     Each row represents an arc, where:
      - column 0 is the source state
      - column 1 is the dest state
      - column 2 is the label
      - column 3 is the score, reinterpreted cast from a float.
    @caution You should not modify the returned tensor since it shares
    the underlying memory with this FSA.
   */
  torch::Tensor Arcs() /*const*/;

  // Get fsa properties.
  int32_t Properties();
  // Get fsa properties as string format.
  std::string PropertiesStr() /*const*/;

  // Transfer current fsa to another device.
  FsaClass To(torch::Device device) const;
  FsaClass To(const std::string &device) const;

  static FsaClass CreateFsaVec(std::vector<FsaClass> &fsas);

  /** Associate an attribute with a value.
    If there is no attribute with the given `name`,
      - If `value` is an instance of `torch::Tensor`, add it to `tensor_attrs`
      - If `value` is an instance of `Ragged<int32_t>`, add it to
        `ragged_tensor_attrs`
    If there is already an attribute with the given `name`, we first
    remove this attribute and then add it using the above logic.
    @param name  The attribute name.
    @param value  The attribute value.
   */
  void SetAttr(const std::string &name, torch::IValue value);

  /** Get an attribute by its name.
    Raise a RuntimeError exception if there is no such attribute.
    @param name The attribute name.
    @return Return the value of the attribute.
   */
  torch::IValue GetAttr(const std::string &name) /*const*/;

  /** Delete an attribute by its name.
    Raise a RuntimeError exception if there is no such attribute.
    @param name The attribute name.
   */
  void DeleteAttr(const std::string &name);

  /** Query if an attribute exists.
    @param name The attribute name.
    @return Return `true` if the given attribute exists.
            Return `false` otherwise.
   */
  bool HasAttr(const std::string &name) const;

 private:
  void SetTensorAttr(const std::string &name, torch::Tensor value) {
    K2_CHECK_EQ(value.size(0), fsa.NumElements())
        << "shape[0] of the tensor MUST be equal to number of arcs";
    all_attr_names.insert(name);
    tensor_attrs[name] = value;
  }
  void SetRaggedTensorAttr(const std::string &name,
                           const Ragged<int32_t> &value) {
    K2_CHECK_EQ(value.Dim0(), fsa.NumElements())
        << "dim0 of the tensor MUST be equal to number of arcs";
    all_attr_names.insert(name);
    ragged_tensor_attrs[name] = value;
  }

  FsaClass ToOtherContext(const ContextPtr &context) const;

  // Propagate tensor attributes from src.
  void CopyTensorAttrs(FsaClass &src, torch::Tensor arc_map);

  // Propagate ragged attributes from src.
  void CopyRaggedTensorAttrs(FsaClass &src, torch::Tensor arc_map);
  void CopyRaggedTensorAttrs(FsaClass &src, Ragged<int32_t> &arc_map);
};

}  // namespace k2
#endif  // K2_TORCH_CSRC_FSA_CLASS_H_
