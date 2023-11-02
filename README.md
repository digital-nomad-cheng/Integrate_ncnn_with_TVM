
In this branch, I try to explore how to use TVM's BYOC to offload computation to a highly optimized inference engine designed for mobile device - [ncnn](https://github.com/tencent/ncnn).
The implementation is based on TVM v0.13.0. 

## Current Progress
- [x] Core codegen and runtime logic, relay pattern matching done. Successfully parse nn.dense layer from relay and get layer information in runtime. Dispatch the computation to ncnn.
- [ ] Types of layers support progresss...
  - [x] Merge nn.dense + nn.bias_add composites
  - [x] reshape layer
  - [x] Merge activation function with nn.dense, support nn.dense + bias_add + relu for now - 25/Sep/2023
  - [x] nn.conv2d - 26/Sep/2023
  - [x] Merge nn.conv2d + nn.bias_add + nn.relu - 26/Sep/2023
  - [ ] nn.depthwise_conv2d
  - [ ] ...
- [x] Reduce memory traffic by allocating input and output tensor at initializing engine time instead of per run. Increase the speed by 20% for AlexNet. 5/Oct
- [x] Set thread number based on hardware
- [ ] Fallback to layout packing
- [ ] Support dispath subgraph instead of per layer
- [ ] Reduce memory traffic when copying weights and tensors from tvm to ncnn, perhaps using tvm as ncnn::Mat's allocator
- [x] Performance benchmark: For AlexNet, on raspberry pi 4B the performance of arm compute lib is 12.455 seconds for image size 227x227, 100 runs, while for ncnn is 8.536 seconds - 31.46% speedup. 6/Oct/2023

## How to Use
1. download repo with prepared Dockerfile: `git clone --recursive https://github.com/digital-nomad-cheng/tvm/ && cd tvm`
2. build docker container: `docker build . -t ncnn_codegen`
3. run docker container: `docker run -it ncnn_codegen:latest`
4. test: `cd ../tvm_project_course/byoc && python alexnet_ncnn_codegen.py`
5. 
   ncnn can support x86 CPU. To benchmark with Arm Compute Lib, you need a ARM device for example Raspberry Pi.

<!--- Licensed to the Apache Software Foundation (ASF) under one -->
<!--- or more contributor license agreements.  See the NOTICE file -->
<!--- distributed with this work for additional information -->
<!--- regarding copyright ownership.  The ASF licenses this file -->
<!--- to you under the Apache License, Version 2.0 (the -->
<!--- "License"); you may not use this file except in compliance -->
<!--- with the License.  You may obtain a copy of the License at -->

<!---   http://www.apache.org/licenses/LICENSE-2.0 -->

<!--- Unless required by applicable law or agreed to in writing, -->
<!--- software distributed under the License is distributed on an -->
<!--- "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY -->
<!--- KIND, either express or implied.  See the License for the -->
<!--- specific language governing permissions and limitations -->
<!--- under the License. -->

<img src=https://raw.githubusercontent.com/apache/tvm-site/main/images/logo/tvm-logo-small.png width=128/> Open Deep Learning Compiler Stack
==============================================


[Documentation](https://tvm.apache.org/docs) |
[Contributors](CONTRIBUTORS.md) |
[Community](https://tvm.apache.org/community) |
[Release Notes](NEWS.md)

[![Build Status](https://ci.tlcpack.ai/buildStatus/icon?job=tvm/main)](https://ci.tlcpack.ai/job/tvm/job/main/)
[![WinMacBuild](https://github.com/apache/tvm/workflows/WinMacBuild/badge.svg)](https://github.com/apache/tvm/actions?query=workflow%3AWinMacBuild)

Apache TVM is a compiler stack for deep learning systems. It is designed to close the gap between the
productivity-focused deep learning frameworks, and the performance- and efficiency-focused hardware backends.
TVM works with deep learning frameworks to provide end to end compilation to different backends.

License
-------
TVM is licensed under the [Apache-2.0](LICENSE) license.

Getting Started
---------------
Check out the [TVM Documentation](https://tvm.apache.org/docs/) site for installation instructions, tutorials, examples, and more.
The [Getting Started with TVM](https://tvm.apache.org/docs/tutorial/introduction.html) tutorial is a great
place to start.

Contribute to TVM
-----------------
TVM adopts apache committer model, we aim to create an open source project that is maintained and owned by the community.
Check out the [Contributor Guide](https://tvm.apache.org/docs/contribute/).

Acknowledgement
---------------
We learned a lot from the following projects when building TVM.
- [Halide](https://github.com/halide/Halide): Part of TVM's TIR and arithmetic simplification module
  originates from Halide. We also learned and adapted some part of lowering pipeline from Halide.
- [Loopy](https://github.com/inducer/loopy): use of integer set analysis and its loop transformation primitives.
- [Theano](https://github.com/Theano/Theano): the design inspiration of symbolic scan operator for recurrence.
