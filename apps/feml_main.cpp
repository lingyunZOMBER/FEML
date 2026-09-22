#include "feml/core/version.hpp"

#include <Eigen/Core>
#include <iostream>

int main() {
    std::cout << "FEML " << feml::versionString() << "\n";
    std::cout << "Eigen " << EIGEN_WORLD_VERSION << "."
        << EIGEN_MAJOR_VERSION << "." << EIGEN_MINOR_VERSION << "\n";
    return 0;
}