// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <cassert>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstring>
#include "video_core/renderer_vulkan/pipeline_compile_queue.h"
#include "video_core/renderer_vulkan/occlusion_model.h"
#include "src/respawn_patch.h"

int main() {
    Vulkan::PipelineJobQueue queue;
    std::array<int, 256> owners{};
    std::array<std::atomic<int>, 256> runs{};
    for (size_t i = 0; i < owners.size(); ++i)
        queue.Push(&owners[i], [&, i] { ++runs[i]; });
    std::thread worker([&] { for (size_t i = 0; i < owners.size(); ++i)
        if (auto job = queue.Take(&owners[i])) job(); });
    for (size_t i = 0; i < owners.size(); ++i)
        if (auto job = queue.Take(&owners[i])) job();
    worker.join();
    for (const auto& count : runs) assert(count == 1);
    assert(queue.Size() == 0);
    int order = 0;
    queue.Push(&owners[0], [&] { assert(order++ == 0); });
    queue.Push(&owners[1], [&] { assert(order++ == 1); });
    queue.Pop()(); queue.Pop()();
    std::uint64_t first = 0;
    assert(Vulkan::WithinSkipBudget(first, 100, 50));
    assert(Vulkan::WithinSkipBudget(first, 149, 50));
    assert(!Vulkan::WithinSkipBudget(first, 150, 50));
    assert(!Vulkan::WithinSkipBudget(first, 200, 0));

    using namespace OcclusionModel;
    for (unsigned pipes = 1; pipes <= MaxPipes; ++pipes) {
        std::array<std::uint64_t, 2 * MaxPipes> block{};
        std::uint64_t sum = 0;
        for (unsigned i = 0; i < pipes; ++i) {
            sum += Share(127, pipes, i);
            block[i * 2] = ValidBit | Share(100, pipes, i);
            block[i * 2 + 1] = ValidBit | Share(100, pipes, i);
        }
        assert(sum == 127);
        assert(ReadBlock(block.data(), pipes) == Block::Occluded);
        ++block[1];
        assert(ReadBlock(block.data(), pipes) == Block::Visible);
        block[0] = 0;
        assert(ReadBlock(block.data(), pipes) == Block::NotReady);
    }
    assert(Samples(0, false, true, 1 << 16) == Unmeasured);
    assert(Samples(1, true, false, 1 << 16) == Unmeasured);
    assert(Samples(2, true, true, SampleScale(960, 540)) == 8);
    assert(PredicatedDraws(false, true, true));
    BlockHistory history;
    assert(history.Report(0) == 1);
    assert(history.Report(0) == 1);
    assert(history.Report(0) == 0);
    assert(history.Report(7) == 7);

    std::vector<unsigned char> image(0x4000);
    const size_t at = 64, constant = 0x3000;
    std::memcpy(image.data() + at, bb_respawn_head, sizeof(bb_respawn_head));
    std::memcpy(image.data() + at + BB_RESPAWN_LOAD_AT, bb_respawn_load, sizeof(bb_respawn_load));
    const int32_t disp = static_cast<int32_t>(constant - at - BB_RESPAWN_TAIL_AT);
    std::memcpy(image.data() + at + BB_RESPAWN_LOAD_AT + 4, &disp, sizeof(disp));
    std::memcpy(image.data() + at + BB_RESPAWN_TAIL_AT, bb_respawn_tail, sizeof(bb_respawn_tail));
    std::memcpy(image.data() + at + BB_RESPAWN_READY_AT, bb_respawn_ready, sizeof(bb_respawn_ready));
    std::memcpy(image.data() + constant, bb_respawn_twelve, sizeof(bb_respawn_twelve));
    size_t found_at = 0; int state = 0;
    assert(bb_respawn_find(image.data(), image.size(), 0, image.size(), &found_at, &state) == 1);
    assert(found_at == at && state == BB_RESPAWN_ORIGINAL);
    bb_respawn_apply(image.data(), at);
    assert(bb_respawn_match(image.data(), image.size(), at) == BB_RESPAWN_ALREADY_APPLIED);
    image[at + BB_RESPAWN_READY_AT] ^= 1;
    assert(bb_respawn_match(image.data(), image.size(), at) == 0);
    assert(bb_respawn_match(image.data(), 16, at) == 0);
    std::puts("Pipeline jobs: exactly-once race, FIFO, skip deadline, occlusion fallback and respawn guards PASS");
}
