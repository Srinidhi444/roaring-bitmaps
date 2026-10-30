
#include <cassert>
#include <iostream>
#include "array_container.hpp"

int main() {
    ArrayContainer container;

    assert(container.add(40));
    assert(container.add(10));
    assert(container.add(30));
    assert(container.add(20));

    // Duplicate insertion should fail.
    assert(!container.add(20));

    assert(container.cardinality() == 4);
    assert(container.contains(30));
    assert(!container.contains(99));

    assert(container.remove(30));
    assert(!container.remove(30));
    assert(!container.contains(30));

    assert(container.cardinality() == 3);

    std::cout << "Stored values: ";
    for (uint16_t value : container.values()) {
        std::cout << value << " ";
    }

    std::cout << "\nArray container tests passed!\n";
}
