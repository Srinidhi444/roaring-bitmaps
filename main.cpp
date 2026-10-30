#include "roaring.hpp"
#include <cstdint>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>

static void require(bool ok) { if (!ok) throw std::runtime_error("test failed"); }
int main() {
    RoaringBitmap a,b;
    std::set<uint32_t> sa,sb;
    std::mt19937 gen(42);
    for (int i=0;i<30000;++i) {
        uint32_t v = gen();
        require(a.add(v)==sa.insert(v).second);
        if (i%2==0) { require(b.add(v)==sb.insert(v).second); }
    }
    for (int i=0;i<7000;++i) {
        a.add(static_cast<uint32_t>(i)); sa.insert(static_cast<uint32_t>(i));
        b.add(static_cast<uint32_t>(i+3000)); sb.insert(static_cast<uint32_t>(i+3000));
    }
    a.add(UINT32_MAX); sa.insert(UINT32_MAX);
    b.add(UINT32_MAX); sb.insert(UINT32_MAX);
    a.run_optimize(); b.run_optimize();
    auto both=a.intersection(b), either=a.unite(b);
    std::set<uint32_t> expected_and,expected_or=sa;
    for (auto v:sb) { if (sa.contains(v)) expected_and.insert(v); expected_or.insert(v); }
    require(both.cardinality()==expected_and.size());
    require(either.cardinality()==expected_or.size());
    for (auto v:expected_and) require(both.contains(v));
    for (auto v:expected_or) require(either.contains(v));
    for (int i=0;i<7000;++i) {
        uint32_t v=static_cast<uint32_t>(i);
        require(a.remove(v)==(sa.erase(v)>0));
        require(a.contains(v)==sa.contains(v));
    }
    require(a.cardinality()==sa.size());
    std::cout << "All correctness tests passed!\n";
    std::cout << "Estimated Roaring memory: " << a.estimated_memory_usage() << " bytes\n";
}
