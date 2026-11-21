#include <iostream>
#include <torch/torch.h>
#include <nlohmann/json.hpp>

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "NEXUS C++17 + LibTorch ready" << std::endl;
    std::cout << "========================================" << std::endl;

    // Create a sample 2x3 random float tensor on CPU
    torch::Device device(torch::kCPU);
    torch::Tensor sample_tensor = torch::rand({2, 3}, torch::TensorOptions().device(device).dtype(torch::kFloat32));

    std::cout << "\n[Sample Tensor (2x3)]\n" << sample_tensor << std::endl;

    // Display shape and device
    std::cout << "\nTensor Properties:" << std::endl;
    std::cout << "  Shape: [";
    for (int64_t i = 0; i < sample_tensor.dim(); ++i) {
        std::cout << sample_tensor.size(i) << (i + 1 < sample_tensor.dim() ? ", " : "");
    }
    std::cout << "]" << std::endl;
    std::cout << "  Device: " << sample_tensor.device() << std::endl;
    std::cout << "  Dtype:  " << sample_tensor.dtype() << std::endl;

    // Verify nlohmann_json structured serialization
    nlohmann::json status_json;
    status_json["system"] = "NEXUS Core Engine";
    status_json["status"] = "INITIALIZED";
    status_json["cpp_standard"] = 17;
    status_json["libtorch_linked"] = true;
    status_json["sample_tensor"] = {
        {"device", sample_tensor.device().str()},
        {"shape", {sample_tensor.size(0), sample_tensor.size(1)}}
    };

    std::cout << "\n[JSON Integration Test]:\n" << status_json.dump(2) << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
