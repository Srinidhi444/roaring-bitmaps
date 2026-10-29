#include <bit>
#include <iostream>

int main() {
    unsigned int value = 0b101101;

    std::cout << "Set bits: "
              << std::popcount(value) << '\n';
}