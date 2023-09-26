/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file src/runtime/contrib/arm_compute_lib/acl_runtime.cc
 * \brief A simple JSON runtime for Arm Compute Library.
 */

#include <tvm/runtime/ndarray.h>
#include <tvm/runtime/registry.h>

#include "../json/json_node.h"
#include "../json/json_runtime.h"

#include "ncnn/net.h"
#include "stdio.h"

namespace tvm {
namespace runtime {
namespace contrib {

using namespace tvm::runtime::json;

class NCNNRuntime : public JSONRuntimeBase {
public: 
  /*!
   * \brief The NCNN runtime module. Deserialize the provided functions 
   * on creation and store in the layer cache.
   * 
   * \param symbol_name The name of the function.
   * \param graph_json serialized JSON representation of a sub-graph.
   * \param const_names The names of each constant in the sub-graph.
   */ 
  explicit NCNNRuntime(const std::string& symbol_name, const std::string& graph_json,
      const Array<String>& const_names) : JSONRuntimeBase(symbol_name, graph_json, const_names) {}
  
  /*!
   * \brief The type key of the mdoule.
   * 
   * \ return module type key
   */ 
  const char* type_key() const override { return "ncnn"; }

  /*!
   * \brief Initialize runtime. Create ncnn layer from JSON representation.
   *
   * \param consts The constant params from compiled model.
   */ 
  void Init(const Array<NDArray>& consts) override {
    ICHECK_EQ(consts.size(), const_idx_.size()) 
      << "The number of input constants must match the number of required.";
    LOG(INFO) << "Initilize ncnn runtime engine";
    SetupConstants(consts);
    BuildEngine();
  }
  
  void Run() override {
    LOG(INFO) << "******************************************";
    LOG(INFO) << "Run ncnn runtime engine";
    LOG(INFO) << "num inputs when running " << input_nodes_.size();
    // TODO PRINT ncnn input shape first
    for (size_t nid_idx = 0; nid_idx < input_nodes_.size(); ++nid_idx) {
      auto nid = input_nodes_[nid_idx];
      if (nodes_[nid].GetOpType() == "input") {
        //LOG(INFO) << "num output for " << nid_idx << "th node is "
        //  << nodes_[nid].GetNumOutput();
        for (uint32_t eid_idx = 0; eid_idx < nodes_[nid].GetNumOutput(); eid_idx++) {
          uint32_t eid = EntryID(nid, eid_idx);
          int ndim = data_entry_[eid]->ndim;
          if (ndim == 2) { // TODO dense layer for now, remove later
            layer_.in.create(
                (int)*(data_entry_[eid]->shape+1),
                (int)*(data_entry_[eid]->shape) 
            );
            for (size_t h = 0; h < layer_.in.h; h++) {
              for (size_t w = 0; w < layer_.in.w; w++) {
                layer_.in[h, w] = 
                  static_cast<float *>(data_entry_[eid]->data)[h * layer_.in.w + w];
              }
            }
          } else if (ndim == 4) { // TODO reshae layer for now, remove later
            layer_.in.create(
                (int)*(data_entry_[eid]->shape+3), // 64
                (int)*(data_entry_[eid]->shape+2), // 64
                (int)*(data_entry_[eid]->shape+1)  // 16
                // (int)*(data_entry_[eid]->shape)
            );
            //LOG(INFO) << "Input from tvm...";
            //for (size_t k = 0; k < 60; k++) {
            //  LOG(INFO) << " " << static_cast<float *>(data_entry_[eid]->data)[k];
            //}
            for (size_t c = 0; c < layer_.in.c; c++) {
              for (size_t d = 0; d < layer_.in.d; d++) {
                for (size_t h = 0; h < layer_.in.h; h++) {
                  for (size_t w = 0; w < layer_.in.w; w++) {
                    layer_.in.channel(c)[h * layer_.in.w + w] = 
                      static_cast<float *>(data_entry_[eid]->data)[
                        c * layer_.in.d * layer_.in.h * layer_.in.w + 
                        d * layer_.in.h * layer_.in.w + 
                        h * layer_.in.w + 
                        w];
                  }
                }
              }
            }
          }
        }
      }
    }
    //LOG(INFO) << "ncnn input data...";
    //pretty_print(layer_.in); 
    layer_.op->forward(layer_.in, layer_.out, layer_.opt);
    // LOG(INFO) << "ncnn output data...";
    //pretty_print(layer_.out);
    //LOG(INFO) << "ncnn output shape: " << 
    //  " w " << layer_.out.w << 
    //  " h " << layer_.out.h << 
    //  " d " << layer_.out.d << 
    //  " c " << layer_.out.c;

    for (size_t i = 0 ; i < outputs_.size(); i++) {
      uint32_t eid = EntryID(outputs_[i]);
      void* data = data_entry_[eid]->data;
      int dim = data_entry_[eid]->ndim;
      for (size_t ii = 0; ii < dim; ii++) {
        int output_shape = *(data_entry_[eid]->shape+ii);
        LOG(INFO) << "output shape along dim " << ii << " is: " << output_shape;
      }
      float* temp_ptr = static_cast<float *>(data);
      if (dim == 2) {
        size_t wdim = *(data_entry_[eid]->shape+1);
        for (size_t ii = 0; ii < wdim; ii++) {
          temp_ptr[ii] = layer_.out.channel(0)[ii]; 
        }
      } else if (dim == 4) {
        // TODO: FIXBUG here
        //LOG(INFO) << "ncnn mat input shape info "
        //  << " c: " << layer_.in.c
        //  << " d: " << layer_.in.d 
        //  << " h: " << layer_.in.h 
        //  << " w: " << layer_.in.w
        //  << " cstep: " << layer_.in.cstep;
        //LOG(INFO) << "ncnn mat output shape info "
        //  << " c: " << layer_.out.c
        //  << " d: " << layer_.out.d 
        //  << " h: " << layer_.out.h 
        //  << " w: " << layer_.out.w
        //  << " cstep: " << layer_.out.cstep;
        size_t wdim = *(data_entry_[eid]->shape+3);
        size_t hdim = *(data_entry_[eid]->shape+2);
        size_t cdim = *(data_entry_[eid]->shape+1); 
        LOG(INFO) << "wdim: " << wdim << " hdim: " << hdim;
        ncnn::Mat layer_out_unpacked;
        ncnn::convert_packing(layer_.out, layer_out_unpacked, 1);
        for (size_t c = 0; c < layer_out_unpacked.c; c++) {
          for (size_t h = 0; h < layer_out_unpacked.h; h++) {
            for (size_t w = 0; w < layer_out_unpacked.w; w++) {
              temp_ptr[c * hdim * wdim + h * wdim + w] = 
                layer_out_unpacked.channel(c)[h * wdim + w];
            }
          } 
        }
      } else {
        LOG(FATAL) << "Unknow dim option";
      }
    }
  }

private:
  /*!
   * \brief Build ncnn layer from JSON representation and cache
   * \note For the time being only one layer or operator is supported
   * per engine
   */
  void BuildEngine() {
    bool found_kernel_node = false;
    for (size_t nid = 0; nid < nodes_.size(); ++nid) {
      const auto& node = nodes_[nid];
      if (found_kernel_node) {
        LOG(FATAL) 
          << "ncnn runtime module only supports one kernel node per function.";
      }
      if (node.GetOpType() == "kernel") {
        found_kernel_node = true;
        auto op_name = node.GetOpName();
        if (op_name == "nn.dense") {
          LOG(INFO) << "Create InnerProduct layer...";
          CreateInnerProductLayer(&layer_, node);
        } else if (op_name == "nn.conv2d") {
          LOG(INFO) << "Create Conv2d layer...";
          CreateConv2dLayer(&layer_,node);
        }
        else if (op_name == "reshape") {
          LOG(INFO) << "Create Reshape layer...";
          CreateReshapeLayer(&layer_, node);
        } else {
          LOG(FATAL) << "Unsupported op: " << op_name;
        }
      }
    }
  }
  
  /*!
   * \brief ncnn objects that we cache in order to avoid needing to construct 
   * a new layer each time.
   */
  struct CachedLayer {
    ncnn::Layer* op;
    ncnn::Option opt;
    ncnn::Mat in;
    ncnn::Mat out;
  };
  
  /*!
   * \brief Helper class used to parse information from JSONGraphNode 
   */
  void ParseInfoFromJSONGraphNode(const JSONGraphNode& node) {
    LOG(INFO) << "------------------------------------";
    auto op_name = node.GetOpName();
    LOG(INFO) << "op name is " << op_name;
    LOG(INFO) << "parse inputs info...";
    std::vector<JSONGraphNodeEntry> inputs = node.GetInputs();
    size_t num_inputs = inputs.size();
    LOG(INFO) << "num inputs for " << op_name << " is " << num_inputs;
    for (size_t i = 0; i < num_inputs; i++) {
      auto tensor = inputs[i];
      JSONGraphNode node = nodes_[tensor.id_];
      LOG(INFO) << i + 1 << "th input is " << node.GetOpType();
      // input or weight
      if (node.GetOpType() == "input" || node.GetOpType() == "const") {
        auto shape = node.GetOpShape()[0];
        LOG(INFO) << "ndim of input is " << shape.size();
        for (size_t ii = 0; ii < shape.size(); ii++) {
          LOG(INFO) << "shape of " << i + 1 << "th input along dim " 
            << ii << " is " << shape[ii]; 
        }
      }
    }
    LOG(INFO) << "parse outputs info...";
    size_t num_outputs = node.GetNumOutput();
    LOG(INFO) <<  "num ouputs for " << op_name << " is " << num_outputs;
    for (size_t i = 0; i < num_outputs; i++) {
      LOG(INFO) << i + 1 << "th output is " << node.GetOpType();
      auto shape = node.GetOpShape()[i];
      LOG(INFO) << "ndim of output is " << shape.size();
      for (size_t ii = 0; ii < shape.size(); ii++) {
        LOG(INFO) << "shape of " << i + 1 << "th output along dim " 
          << ii << " is " << shape[ii]; 
      }
    }
    LOG(INFO) << "======================================";
  }

  void CreateConv2dLayer(CachedLayer* layer, const JSONGraphNode& node) {
    ParseInfoFromJSONGraphNode(node);
    std::vector<JSONGraphNodeEntry> inputs = node.GetInputs();
    ICHECK(inputs.size() == 2) << "Currently only support conv2d without bias_add";
    ncnn::Layer* op = ncnn::create_layer("Convolution");
    auto channels = node.GetAttr<std::vector<std::string>>("channels");
    auto kernel_sizes = node.GetAttr<std::vector<std::string>>("kernel_size");
    auto padding = node.GetAttr<std::vector<std::string>>("padding");
    auto strides = node.GetAttr<std::vector<std::string>>("strides");
    auto dilation = node.GetAttr<std::vector<std::string>>("dilation");
    
    ncnn::Option opt;
    opt.num_threads = 2;
    opt.use_packing_layout = false;
    ncnn::ParamDict pd;
    ncnn::Mat weights[1];
    // int weight_size = std::stoi(channels[0]) * std::stoi(kernel_sizes[0]) * std::stoi(kernel_sizes[1]);
    // weights[0].create(weight_size);
    // TODO deal with edge cases with various sizes in different direction
    pd.set(0, std::stoi(channels[0]));
    pd.set(1, std::stoi(kernel_sizes[0])); // set kernel size
    pd.set(2, std::stoi(dilation[0])); // set dilation 
    pd.set(3, std::stoi(strides[0])); // set strides
    pd.set(4, std::stoi(padding[0])); // set padding size
    pd.set(5, 0); // set bias term
    // LOG(INFO) << "channels size: " << channels.size() << "channels: " << channels[0];
//    LOG(INFO) << "padding size: " << padding.size() << "padding[0]" << padding[0];
//    LOG(INFO) << "kernel_size size: " << kernel_sizes.size();

    // Load weights from tvm into ncnn 
    for (size_t i = 0; i < inputs.size(); i++) {
      auto tensor = inputs[i];
      JSONGraphNode node = nodes_[tensor.id_];
      if (node.GetOpType() == "const") {
        void* node_data = nullptr;
        node_data = data_entry_[EntryID(tensor)]->data;
        auto dim = data_entry_[EntryID(tensor)]->ndim;
        LOG(INFO) << "ndim of data is " << dim;
        int64_t data_shape[dim];
        for (size_t ii = 0; ii < dim; ii++) {
          data_shape[ii] = *(data_entry_[EntryID(tensor)]->shape+ii);
          LOG(INFO) << "shape of weight along dim "
            << ii << " is " << data_shape[ii];
        }
        int weight_size = 1;
        for (size_t ii = 0; ii < dim; ii++) {
          weight_size *= data_shape[ii];
        }
        pd.set(6, weight_size);
        weights[0].create(weight_size);

        float* temp_ptr = static_cast<float *>(node_data);
        for (size_t ii = 0; ii < weight_size; ii++) {
          weights[0][ii] = temp_ptr[ii];
        }
      }
    }
    op->load_param(pd);
    op->load_model(ncnn::ModelBinFromMatArray(weights));
    op->create_pipeline(opt);
    layer->op = op;
    layer->opt = opt;
  }

  void CreateInnerProductLayer(CachedLayer* layer, const JSONGraphNode& node) {
    ParseInfoFromJSONGraphNode(node);
    ncnn::Layer* op = ncnn::create_layer("InnerProduct");
    // collect inputs from json representation 
    std::vector<JSONGraphNodeEntry> inputs = node.GetInputs();
    size_t num_inputs= inputs.size();
    LOG(INFO) << "num_inputs for InnerProduct layer parsed from ncnn json: " << num_inputs;
    bool has_bias;
    ICHECK(num_inputs >= 2U && num_inputs <= 3U)
      << "InnerProduct(dense) layer requires 3 inputs with a bias, 2 inputs without.";
    has_bias = num_inputs == 3;
    ncnn::Option opt;
    opt.num_threads = 2; // TODO: how to get num threads to use from tvm
    ncnn::ParamDict pd;
    ncnn::Mat *weights; // TODO: remember to release memory after use here!!!

    if (has_bias) {
      pd.set(1, 1); // has bias
      weights = new ncnn::Mat[2];
    } else {
      pd.set(1, 0); // has no bias
      weights = new ncnn::Mat[1];
    }
    
    if (node.HasAttr("activation_type")) {
      std::string activation_type = node.GetAttr<std::vector<std::string>>("activation_type")[0];
      if (activation_type == std::string("relu")) {
        // 0=none 1=relu 2=leakyrelu 3=clip 4=sigmoid
        pd.set(9, 1); // set activation type to relu
      }
    }
    // TODO map inputs to weights
    for (size_t i = 0; i < inputs.size(); i++) {
      auto tensor = inputs[i];
      JSONGraphNode node = nodes_[tensor.id_];
      if (node.GetOpType() == "const") {
        if (i == 1) {
          LOG(INFO) << i + 1 << "th node is " << "fc weight node";
          void* node_data = nullptr;
          node_data = data_entry_[EntryID(tensor)]->data;
          auto dim = data_entry_[EntryID(tensor)]->ndim;
          LOG(INFO) << "ndim of data is " << dim;
          int64_t data_shape[dim];
          int64_t data_size = 1;
          for (size_t i = 0; i < dim; i++) {
            data_shape[i] = *(data_entry_[EntryID(tensor)]->shape+i);
            LOG(INFO) << "shape of weight along dim "
              << i << " is " << data_shape[i];
            data_size *= data_shape[i];
          }
          // *node_shape is 10
          pd.set(0, (int)data_shape[0]); // set output shape
          pd.set(2, (int)data_size); // set weight size here
          weights[0].create((int)data_size);
          float* temp_array = static_cast<float *>(node_data);
          for (size_t ii = 0; ii < data_size; ii++) {
            weights[0][ii] = temp_array[ii];
          }
          //auto stride = data_entry_[EntryID(tensor)]->strides;
          //LOG(INFO) << "stride of weight is " << stride;
          //auto dtype = data_entry_[EntryID(tensor)]->dtype;
          //LOG(INFO) << "dtype of weight is " << dtype;
          //auto b = data_entry_[EntryID(tensor)]->byte_offset;
          //LOG(INFO) << "byte offset of weight is " << b;
        } else if(i == 2) { // nn.dense with bias_add
          LOG(INFO) << i + 1 << "th node is " << "fc bias node";
          void* node_data = nullptr;
          node_data = data_entry_[EntryID(tensor)]->data;
          auto dim = data_entry_[EntryID(tensor)]->ndim;
          LOG(INFO) << "ndim of data is " << dim;
          int64_t data_shape[dim];
          int64_t data_size = 1;
          for (size_t i = 0; i < dim; i++) {
            data_shape[i] = *(data_entry_[EntryID(tensor)]->shape+i);
            LOG(INFO) << "shape of weight along dim "
              << i << " is " << data_shape[i];
            data_size *= data_shape[i];
          }
          weights[1].create((int)data_size);
          float* temp_array = static_cast<float *>(node_data);
          for (size_t ii = 0; ii < data_size; ii++) {
            weights[1][ii] = temp_array[ii];
          }
        }
      }
    }
    op->load_param(pd); // load param/model structure
    op->load_model(ncnn::ModelBinFromMatArray(weights));
    op->create_pipeline(opt);
    layer->op = op;
    layer->opt = opt;
  }
  
  void CreateReshapeLayer(CachedLayer* layer, const JSONGraphNode& node) {
    ParseInfoFromJSONGraphNode(node);
    ncnn::Layer *op = ncnn::create_layer("Reshape");
    ncnn::Option opt;
    opt.num_threads = 2;
    ncnn::ParamDict pd;
    // TODO Replace hardcode with info from JSONGraphNode
    pd.set(0, -1);
    pd.set(1, 1);
    op->load_param(pd);
    op->create_pipeline(opt);
    layer->op = op;
    layer->opt = opt;
  }
  void pretty_print(const ncnn::Mat& m)
  {
      for (int q=0; q<m.c; q++)
      {
          const float* ptr = m.channel(q);
          for (int y=0; y<m.h; y++)
          {
              for (int x=0; x<m.w; x++)
              {
                  printf("%f ", ptr[x]);
              }
              ptr += m.w;
              printf("\n");
          }
          printf("------------------------\n");
      }
  }

  CachedLayer layer_;
};

runtime::Module NCNNRuntimeCreate(const String& symbol_name, const String& graph_json, 
    const Array<String>& const_names) {
  auto n = make_object<NCNNRuntime>(symbol_name, graph_json, const_names);
  return runtime::Module(n);
}

TVM_REGISTER_GLOBAL("runtime.NCNNRuntimeCreate").set_body_typed(NCNNRuntimeCreate);
TVM_REGISTER_GLOBAL("runtime.module.loadbinary_ncnn")
  .set_body_typed(JSONRuntimeBase::LoadFromBinary<NCNNRuntime>);
}
}
}
