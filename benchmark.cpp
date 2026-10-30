#include "roaring.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

using Clock = std::chrono::steady_clock;
volatile std::uint64_t benchmark_sink = 0;

template<class F> double milliseconds(F fn) {
    std::vector<double> samples;
    for(int i=0;i<5;++i) {
        const auto start=Clock::now();
        const auto value=fn();
        const auto end=Clock::now();
        benchmark_sink = benchmark_sink + value;
        samples.push_back(std::chrono::duration<double,std::milli>(end-start).count());
    }
    std::sort(samples.begin(),samples.end());
    return samples[2];
}

class PlainBitmap {
    std::vector<uint64_t> words_;
public:
    explicit PlainBitmap(uint32_t max_value)
        : words_(static_cast<std::size_t>(max_value)/64 + 1) {}
    void add(uint32_t x) { words_[x/64] |= uint64_t{1} << (x%64); }
    bool contains(uint32_t x) const { return (words_[x/64] >> (x%64)) & 1u; }
    std::size_t and_count(const PlainBitmap& b) const {
        std::size_t count=0;
        for (std::size_t i=0;i<words_.size();++i) count+=std::popcount(words_[i]&b.words_[i]);
        return count;
    }
    std::size_t memory_usage() const { return sizeof(*this)+words_.capacity()*sizeof(uint64_t); }
};

std::vector<uint32_t> random_values(std::size_t count,uint32_t limit,uint32_t seed) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<uint32_t> dist(0,limit);
    std::unordered_set<uint32_t> s;
    s.reserve(count*2);
    while(s.size()<count) s.insert(dist(gen));
    return {s.begin(),s.end()};
}

void run_case(const std::string& label,const std::vector<uint32_t>& data,uint32_t max_value,bool plain_ok) {
    std::vector<uint32_t> other;
    for(std::size_t i=0;i<data.size();i+=2) other.push_back(data[i]);
    RoaringBitmap r,rb;
    std::set<uint32_t> s,sb;
    std::unordered_set<uint32_t> h,hb;
    h.reserve(data.size()); hb.reserve(other.size());
    for(auto x:data) { r.add(x);s.insert(x);h.insert(x); }
    for(auto x:other) { rb.add(x);sb.insert(x);hb.insert(x); }
    if(r.cardinality()!=s.size()||r.intersection(rb).cardinality()!=sb.size())
        throw std::runtime_error("roaring cardinality or AND incorrect");
    for(auto x:data) if(!r.contains(x)) throw std::runtime_error("roaring lookup incorrect");
    std::vector<uint32_t> queries=data;
    for(auto x:data) queries.push_back(x ^ 0x9e3779b9u);
    const auto lookup=[&](auto& collection) -> std::uint64_t {
        std::uint64_t n=0;
        for(auto x:queries) n+=collection.contains(x);
        return n;
    };
    const auto expected=lookup(s);
    if(lookup(r)!=expected||lookup(h)!=expected) throw std::runtime_error("lookup mismatch");
    auto tree_and=[&]()->std::uint64_t {
        std::uint64_t n=0;
        auto a=s.begin(),b=sb.begin();
        while(a!=s.end()&&b!=sb.end()) {
            if(*a==*b){++n;++a;++b;}
            else if(*a<*b)++a;
            else ++b;
        }
        return n;
    };
    auto hash_and=[&]()->std::uint64_t {
        std::uint64_t n=0;
        for(auto x:hb)n+=h.contains(x);
        return n;
    };
    auto roaring_and=[&]()->std::uint64_t {return r.intersection(rb).cardinality();};
    if(tree_and()!=sb.size()||hash_and()!=sb.size()) throw std::runtime_error("baseline mismatch");
    auto ri=milliseconds([&]()->std::uint64_t{RoaringBitmap tmp;for(auto x:data)tmp.add(x);return tmp.cardinality();});
    auto si=milliseconds([&]()->std::uint64_t{std::set<uint32_t> tmp;for(auto x:data)tmp.insert(x);return tmp.size();});
    auto hi=milliseconds([&]()->std::uint64_t{std::unordered_set<uint32_t> tmp;tmp.reserve(data.size());for(auto x:data)tmp.insert(x);return tmp.size();});
    auto rl=milliseconds([&](){return lookup(r);});
    auto sl=milliseconds([&](){return lookup(s);});
    auto hl=milliseconds([&](){return lookup(h);});
    auto ra=milliseconds(roaring_and),sa=milliseconds(tree_and),ha=milliseconds(hash_and);
    // Memory estimates: tree/hash nodes have allocator-specific overhead. These are not measured allocations.
    const std::size_t tree_est=sizeof(s)+s.size()*(sizeof(uint32_t)+3*sizeof(void*)+sizeof(void*));
    const std::size_t hash_est=sizeof(h)+h.bucket_count()*sizeof(void*)+h.size()*(sizeof(uint32_t)+2*sizeof(void*));
    std::cout<<"\n=== "<<label<<" ===\n";
    std::cout<<"Values: "<<data.size()<<" | Roaring containers: "<<r.container_count()<<"\n";
    std::cout<<std::left<<std::setw(20)<<"Structure"<<std::right<<std::setw(12)<<"Insert ms"<<std::setw(12)<<"Lookup ms"<<std::setw(12)<<"AND ms"<<std::setw(16)<<"Est. bytes"<<'\n';
    const auto print=[&](const char* name,double insert,double look,double and_ms,std::size_t memory) {
        std::cout<<std::left<<std::setw(20)<<name<<std::right<<std::fixed<<std::setprecision(3)
            <<std::setw(12)<<insert<<std::setw(12)<<look<<std::setw(12)<<and_ms<<std::setw(16)<<memory<<'\n';
    };
    print("Roaring",ri,rl,ra,r.estimated_memory_usage());
    print("std::set",si,sl,sa,tree_est);
    print("std::unordered_set",hi,hl,ha,hash_est);
    if(plain_ok) {
        PlainBitmap p(max_value),q(max_value);
        for(auto x:data)p.add(x);
        for(auto x:other)q.add(x);
        auto plain_lookup=[&]()->std::uint64_t {std::uint64_t n=0;for(auto x:queries)if(x<=max_value)n+=p.contains(x);return n;};
        if(plain_lookup()!=expected||p.and_count(q)!=sb.size())throw std::runtime_error("plain bitmap mismatch");
        auto pi=milliseconds([&]()->std::uint64_t{PlainBitmap tmp(max_value);for(auto x:data)tmp.add(x);return data.size();});
        print("Plain bitmap",pi,milliseconds(plain_lookup),milliseconds([&]()->std::uint64_t{return p.and_count(q);}),p.memory_usage());
    } else std::cout<<"Plain bitmap: skipped (would require ~512 MiB)\n";
    auto optimized=r,optimized_b=rb;
    auto optimize_ms=milliseconds([&]()->std::uint64_t{auto tmp=r;tmp.run_optimize();return tmp.cardinality();});
    optimized.run_optimize();optimized_b.run_optimize();
    if(optimized.intersection(optimized_b).cardinality()!=sb.size())throw std::runtime_error("run optimization incorrect");
    const auto opt_and=milliseconds([&]()->std::uint64_t{return optimized.intersection(optimized_b).cardinality();});
    std::cout<<"After run_optimize: "<<optimized.estimated_memory_usage()<<" est. bytes"
             <<" | optimization "<<optimize_ms<<" ms | AND "<<opt_and<<" ms\n";
}

int main() {
    try {
        run_case("Sparse random",random_values(20000,999999,42),999999,true);
        run_case("Dense random",random_values(50000,99999,43),99999,true);
        std::vector<uint32_t> runs;
        for(uint32_t i=100000;i<130000;++i)runs.push_back(i);
        run_case("Consecutive",runs,129999,true);
        run_case("Wide random 32-bit",random_values(20000,UINT32_MAX,44),UINT32_MAX,false);
        std::cout<<"\nAll benchmark correctness checks passed.\n";
    } catch(const std::exception& e) {
        std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;
    }
}
