#include "classifier.hpp"

#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/filesystem.hpp>
#include <inference_engine.hpp>

using namespace InferenceEngine;
using namespace cv;
using namespace cv::utils::fs;

void topK(const std::vector<float>& src, unsigned k,
          std::vector<float>& dst,
          std::vector<unsigned>& indices) {
    std::vector<float> tmp = src; 
    std::sort(tmp.rbegin(), tmp.rend());

    for (int i = 0; i < k; i++)
    {
        dst.push_back(tmp[i]);
        for (int j = 0; src.size(); j++)
        {
            if (tmp[i] == src[j])
            {
                indices.push_back(j);
                break;
            }
        }
    }
}

void softmax(std::vector<float>& values) {
    float sum = 0.0f; 
    float max = *std::max_element(values.begin(), values.end());
    for (int i = 0; i < values.size(); i++)
    {
        sum += exp(values[i] - max);
    }
    for (int i = 0; i < values.size(); i++)
    {
        values[i] = exp(values[i] - max) / sum;
    }
}

Blob::Ptr wrapMatToBlob(const Mat& m) {
    std::vector<size_t> dims = {1, (size_t)m.channels(), (size_t)m.rows, (size_t)m.cols};
    return make_shared_blob<uint8_t>(TensorDesc(Precision::U8, dims, Layout::NHWC),
                                     m.data);

}

Classifier::Classifier() {
    Core ie;

    // Load deep learning network into memory
    CNNNetwork net = ie.ReadNetwork(join(DATA_FOLDER, "DenseNet_121.xml"),
                                    join(DATA_FOLDER, "DenseNet_121.bin"));

    // Specify preprocessing procedures
    // (NOTE: this part is different for different models!)
    InputInfo::Ptr inputInfo = net.getInputsInfo()["data"];
    inputInfo->getPreProcess().setResizeAlgorithm(ResizeAlgorithm::RESIZE_BILINEAR);
    inputInfo->setLayout(Layout::NHWC);
    inputInfo->setPrecision(Precision::U8);
    outputName = net.getOutputsInfo().begin()->first;

    // Initialize runnable object on CPU device
    ExecutableNetwork execNet = ie.LoadNetwork(net, "CPU");

    // Create a single processing thread
    req = execNet.CreateInferRequest();
}

void Classifier::classify(const cv::Mat& image, int k, std::vector<float>& probabilities,
                          std::vector<unsigned>& indices) {
    // Create 4D blob from BGR image
    Blob::Ptr input = wrapMatToBlob(image);

    // Pass blob as network's input. "data" is a name of input from .xml file
    req.SetBlob("data", input);

    // Launch network
    req.Infer();

    // Copy output. "prob" is a name of output from .xml file
    float* output = req.GetBlob(outputName)->buffer();

    std::vector<float> out;
    for (int i = 0; i < req.GetBlob(outputName)->size(); i++)
    {
        out.push_back(output[i]);
    }
    topK(out, k, probabilities, indices);
    softmax(probabilities);
}
