#include "noether/Network.h"
#include "noether/Image.h"
#include "noether/Nodes.h"
#include "noether/Tensor.h"

#include <iostream>
#include <unordered_set>

using namespace noether;

Network::Network() {}

Network::~Network() {
  for (auto *node : networkNodes_) {
    delete node;
  }
}

ConvNode *Network::createConvNode(TrainableNode *input, size_t outDepth,
                                  size_t filterSize, size_t stride,
                                  size_t pad) {
  return addNode(new ConvNode(this, input, outDepth, filterSize, stride, pad));
}

MaxPoolNode *Network::createMaxPoolNode(TrainableNode *input, size_t filterSize,
                                        size_t stride, size_t pad) {
  return addNode(new MaxPoolNode(this, input, filterSize, stride, pad));
}

FullyConnectedNode *Network::createFullyConnectedNode(TrainableNode *input,
                                                      size_t outDepth) {
  return addNode(new FullyConnectedNode(this, input, outDepth));
}

RELUNode *Network::createRELUNode(TrainableNode *input) {
  return addNode(new RELUNode(this, input));
}

SigmoidNode *Network::createSigmoidNode(TrainableNode *input) {
  return addNode(new SigmoidNode(this, input));
}

SoftMaxNode *Network::createSoftMaxNode(TrainableNode *input) {
  return addNode(new SoftMaxNode(this, input));
}

RegressionNode *Network::createRegressionNode(TrainableNode *input) {
  return addNode(new RegressionNode(this, input));
}

MaxNode *Network::createMaxNode(TrainableNode *input) {
  return addNode(new MaxNode(this, input));
}

ArrayNode *Network::createArrayNode(ArrayRef<size_t> dims) {
  return addNode(new ArrayNode(this, dims));
}

void Network::registerDerivTensor(NodeBase *node, TrainableData *weights) {
  trainableBuffers_.push_back(weights);
}

namespace {

struct BackwardPass : NodeVisitor {
  virtual void pre(NodeBase *N) override { N->backward(); }
};

struct ForwardPass : NodeVisitor {
  virtual void post(NodeBase *N) override { N->forward(); }
};

struct PrinterPass : NodeVisitor {
  virtual void post(NodeBase *N) override { std::cout << N->getName() << "->"; }
};

} // namespace

/// Train the network starting with the node \p root. Perform \p iterations
/// iterations in the training loop. Update the nodes in \p nodes with the
/// values \p inputs.
void Network::train(NodeBase *root, size_t iterations,
                    ArrayRef<NodeBase *> nodes, ArrayRef<Tensor *> inputs) {

  for (size_t i = 0; i < iterations; i++) {
    // Update all of the inputs of all of the relevant nodes:
    for (int i = 0, e = nodes.size(); i < e; i++) {
      nodes[i]->updateInputs(inputs[i], trainCounter_);
    }

    // Forward scan.
    ForwardPass FP;
    root->visit(&FP);

    // Backward scan in reverse order.
    BackwardPass BP;
    root->visit(&BP);

    trainCounter_++;

    // Only update the gradient when we've reached the end of the batch.
    if (trainCounter_ % trainConf_.batchSize)
      continue;

    // Update the gradients.
    for (auto &buffer : trainableBuffers_) {
      buffer->train(trainConf_);
    }

    for (auto &buffer : trainableBuffers_) {
      buffer->clearGradient();
    }
  }
}

/// Perform a single training iteration for one input. Update the nodes in \p
/// nodes with the values \p inputs.
void Network::train(NodeBase *root, ArrayRef<NodeBase *> nodes,
                    ArrayRef<Tensor *> inputs) {
  assert(nodes.size() == inputs.size() && "Mismatched argument list");

  // Update all inputs.
  for (int i = 0, e = nodes.size(); i < e; i++) {
    nodes[i]->updateInput(inputs[i]);
  }

  // Forward scan.
  ForwardPass FP;
  root->visit(&FP);

  // Backward scan in reverse order.
  BackwardPass BP;
  root->visit(&BP);

  trainCounter_++;

  // Only update the gradient when we've reached the end of the batch.
  if (trainCounter_ % trainConf_.batchSize)
    return;

  // Update the gradients.
  for (auto &buffer : trainableBuffers_) {
    buffer->train(trainConf_);
  }

  for (auto &buffer : trainableBuffers_) {
    buffer->clearGradient();
  }
}

void Network::infer(NodeBase *root, ArrayRef<NodeBase *> nodes,
                    ArrayRef<Tensor *> inputs) {
  // Update all inputs.
  for (int i = 0, e = nodes.size(); i < e; i++) {
    nodes[i]->updateInput(inputs[i]);
  }

  // Forward scan.
  ForwardPass FP;
  root->visit(&FP);
}

void Network::dump(NodeBase *root) {
  std::cout << "Network structure:";

  // Print all of the nodes in the network.
  PrinterPass FP;
  root->visit(&FP);
  std::cout << "\n";

  std::cout << "Buffers content:\n";

  for (auto &buffer : trainableBuffers_) {
    buffer->dump();
  }

  std::cout << "\n";
}
