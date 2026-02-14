#include "benchmark.hpp"
#include <iostream>
#include <fstream>
#include <cstring>

std::ofstream g_output;

extern void bench_veb_micro();
extern void bench_vector_bool_micro();
extern void bench_roaring_micro();
extern void bench_macro_insert();
extern void bench_macro_insert_sequential();
extern void bench_macro_insert_clustered();
extern void bench_macro_contains();
extern void bench_macro_remove();
extern void bench_macro_iteration();
extern void bench_macro_set_operations();

int main(int argc, char* argv[]) {
    std::string output_file = "benchmark_results.json";
    bool run_micro = true;
    bool run_macro = true;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--output" && i + 1 < argc) {
            output_file = argv[++i];
        } else if (std::string(argv[i]) == "--micro-only") {
            run_macro = false;
        } else if (std::string(argv[i]) == "--macro-only") {
            run_micro = false;
        }
    }

    g_output.open(output_file);
    g_output << "[\n";

    std::cout << "vEBitset Benchmark Suite\n";
    std::cout << "============================\n\n";

    if (run_micro) {
        std::cout << "Running microbenchmarks (nanobench)...\n";
        std::cout << "\n[vEBitset]\n";
        bench_veb_micro();
        
        std::cout << "\n[std::vector<bool>]\n";
        bench_vector_bool_micro();
        
        std::cout << "\n[Roaring Bitmap]\n";
        bench_roaring_micro();
    }

    if (run_macro) {
        std::cout << "\nRunning macrobenchmarks (comparison)...\n";
        std::cout << "\n[Insert Benchmarks - Random Pattern]\n";
        bench_macro_insert();
        
        std::cout << "\n[Insert Benchmarks - Sequential Pattern]\n";
        bench_macro_insert_sequential();
        
        std::cout << "\n[Insert Benchmarks - Clustered Pattern]\n";
        bench_macro_insert_clustered();
        
        std::cout << "\n[Contains Benchmark]\n";
        bench_macro_contains();
        
        std::cout << "\n[Remove Benchmark]\n";
        bench_macro_remove();
        
        std::cout << "\n[Iteration Benchmark]\n";
        bench_macro_iteration();
        
        std::cout << "\n[Set Operations Benchmark]\n";
        bench_macro_set_operations();
    }

    g_output << "\n]\n";
    g_output.close();
    
    std::cout << "\nBenchmark results written to: " << output_file << "\n";
    return 0;
}
