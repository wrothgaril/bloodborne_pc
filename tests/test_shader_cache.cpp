// SPDX-License-Identifier: GPL-2.0-or-later
// Exercises the production IO queue and disk cache without launching a game or GPU.
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>
#include "video_core/cache_storage.h"

int main(int argc, char** argv) {
    assert(argc == 3); // CTest supplies an isolated, workspace-local scratch directory.
    const auto root = std::filesystem::absolute(argv[1]);
    assert(!std::filesystem::exists(root));
    std::filesystem::create_directories(root / "TEST00001");
    const auto dir = root / "TEST00001";
    const auto write = [&](const char* name, size_t bytes) {
        std::ofstream file(dir / name, std::ios::binary);
        file << std::string(bytes, 'x');
    };
    write("old.spv", 700000);
    write("new.spv", 700000);
    write("profile.bin", 32);
    write("keep.txt", 64);
    std::filesystem::last_write_time(dir / "old.spv",
        std::filesystem::file_time_type::clock::now() - std::chrono::hours(24));

    auto& cache = Storage::DataBase::Instance();
    cache.Open(root, "TEST00001"); // Each CTest case supplies its cache limit.
    assert(cache.IsOpened());
    assert(std::filesystem::exists(dir / "old.spv") == (std::string_view(argv[2]) != "prune"));
    assert(std::filesystem::exists(dir / "new.spv"));
    assert(std::filesystem::exists(dir / "profile.bin"));
    assert(std::filesystem::exists(dir / "keep.txt"));

    std::vector<std::thread> producers;
    for (unsigned t = 0; t < 4; ++t) {
        producers.emplace_back([&, t] {
            for (unsigned i = 0; i < 100; ++i) {
                assert(cache.Save(Storage::BlobType::ShaderBinary,
                    std::to_string(t * 100 + i), std::vector<u32>{t, i, 0x07230203}));
            }
        });
    }
    for (auto& producer : producers) producer.join();
    assert(cache.Flush(std::chrono::seconds(10)));
    assert(cache.Statistics().written_bytes == 400 * 3 * sizeof(u32));
    cache.Close();

    cache.Open(root, "TEST00001");
    assert(cache.Statistics().written_bytes == 0);
    for (unsigned i = 0; i < 400; ++i) {
        std::vector<u32> read;
        cache.Load(Storage::BlobType::ShaderBinary, std::to_string(i), read);
        assert((read == std::vector<u32>{i / 100, i % 100, 0x07230203}));
    }
    assert(cache.Statistics().hits == 400);
    assert(cache.Statistics().read_bytes == 400 * 3 * sizeof(u32));
    std::vector<u32> read{99};
    cache.Load(Storage::BlobType::ShaderBinary, "absent", read);
    assert(read.empty());
    write("bad.spv", 3); // A truncated word must be a miss, never passed to Vulkan.
    cache.Load(Storage::BlobType::ShaderBinary, "bad", read);
    assert(read.empty());
    assert(cache.Statistics().misses == 2);
    // Close must drain accepted work even without an explicit Flush.
    for (unsigned i = 0; i < 100; ++i)
        assert(cache.Save(Storage::BlobType::ShaderMeta, "exit" + std::to_string(i),
                          std::vector<u8>{1, 2, 3, 4}));
    cache.Close();
    for (unsigned i = 0; i < 100; ++i)
        assert(std::filesystem::file_size(dir / ("exit" + std::to_string(i) + ".meta")) == 4);
    cache.Open(root, "TEST00001");
    cache.Clear();
    assert(std::filesystem::exists(dir / "keep.txt"));
    assert(!std::filesystem::exists(dir / "profile.bin"));
    assert(!std::filesystem::exists(dir / "0.spv"));
    cache.Close();
    // Exact test-owned directory created above; never remove an existing cache.
    std::filesystem::remove_all(root);
    std::puts("Shader cache: concurrent saves, flush, close drain, reopen, pruning and statistics PASS");
}
