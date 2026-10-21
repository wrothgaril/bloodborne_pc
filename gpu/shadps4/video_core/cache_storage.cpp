// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/elf_info.h"
#include "common/io_file.h"
#include "common/polyfill_thread.h"
#include "common/thread.h"
#include "core/emulator_settings.h"

#include "video_core/cache_storage.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"

#include <miniz.h>

#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <queue>

#include <algorithm>
#include <charconv>
#include <limits>
#include <atomic>
#include <cstdlib>
#include <vector>

namespace {

std::condition_variable_any request_cv{};
std::condition_variable idle_cv{};
std::queue<std::function<void()>> req_queue{};
bool io_busy{};
std::mutex m_request{};

mz_zip_archive zip_ar{};
bool ar_is_read_only{true};

// bbport: cache statistics (BB_CACHE_STATS=1). Counted on the loading threads, so atomics.
struct CacheStats {
    std::atomic<u64> hits{0};
    std::atomic<u64> misses{0};
    std::atomic<u64> read_bytes{0};
    std::atomic<u64> written_bytes{0};
};
CacheStats cache_stats{};

bool CacheStatsEnabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("BB_CACHE_STATS");
        return value != nullptr && value[0] == '1';
    }();
    return enabled;
}

// bbport: BB_CACHE_MAX_MB caps the on-disk cache (default 2048, 0 disables); the oldest blobs by
// modification time are removed first, so a long playthrough cannot fill the disk unnoticed.
u64 CacheMaxBytes() {
    static const u64 limit = [] {
        const char* value = std::getenv("BB_CACHE_MAX_MB");
        u64 mb = 2048;
        if (value && *value) {
            const std::string_view text{value};
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), mb);
            if (error != std::errc{} || end != text.data() + text.size() ||
                mb > std::numeric_limits<u64>::max() / (1024ULL * 1024ULL)) {
                mb = 2048;
            }
        }
        return mb * 1024ULL * 1024ULL;
    }();
    return limit;
}

void PruneCache(const std::filesystem::path& dir) {
    const u64 limit = CacheMaxBytes();
    if (limit == 0) {
        return;
    }
    std::error_code error;
    std::vector<std::pair<std::filesystem::file_time_type, std::pair<u64, std::filesystem::path>>> files;
    u64 total = 0;
    for (const auto& entry : std::filesystem::directory_iterator{dir, error}) {
        // Only shader blobs in this serial directory; retain profile.bin and unrelated files.
        // Do not follow links into another cache or directory.
        const auto status = entry.symlink_status(error);
        if (error || !std::filesystem::is_regular_file(status)) {
            continue;
        }
        const auto ext = entry.path().extension();
        if (ext != ".spv" && ext != ".meta" && ext != ".key") {
            continue;
        }
        const auto size = static_cast<u64>(entry.file_size(error));
        if (error) {
            continue;
        }
        const auto time = entry.last_write_time(error);
        if (error) {
            continue;
        }
        total += size;
        files.emplace_back(time, std::pair<u64, std::filesystem::path>{size, entry.path()});
    }
    if (total <= limit) {
        return;
    }
    std::sort(files.begin(), files.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    // Leave some headroom so the sweep does not run on every start.
    const u64 target = limit - limit / 10;
    u64 removed = 0;
    for (const auto& [time, info] : files) {
        if (total <= target) {
            break;
        }
        if (std::filesystem::remove(info.second, error)) {
            total -= std::min(total, info.first);
            ++removed;
        }
    }
    LOG_WARNING(Render, "Shader cache pruned: {} file(s) removed, {} MiB left (cap {} MiB)", removed,
                total >> 20, limit >> 20);
}

} // namespace

namespace Storage {

void ProcessIO(const std::stop_token& stoken) {
    Common::SetCurrentThreadName("shadPS4:PipelineCacheIO");

    for (;;) {
        std::function<void()> request;
        {
            std::unique_lock lk{m_request};
            io_busy = false;
            idle_cv.notify_all();
            // bbport: one mutex guards the queue and the wait. The separate request count was
            // changed under different locks and could lose queued writes.
            Common::CondvarWait(request_cv, lk, stoken, [&] { return !req_queue.empty(); });
            // A stop drains the writes already accepted.
            if (req_queue.empty()) {
                return;
            }
            request = std::move(req_queue.front());
            req_queue.pop();
            io_busy = true;
        }
        try {
            request();
        } catch (const std::exception& e) {
            LOG_ERROR(Render, "Cache write failed: {}", e.what());
        }
    }
}

constexpr std::string GetBlobFileExtension(BlobType type) {
    switch (type) {
    case BlobType::ShaderMeta: {
        return "meta";
    }
    case BlobType::ShaderBinary: {
        return "spv";
    }
    case BlobType::PipelineKey: {
        return "key";
    }
    case BlobType::ShaderProfile: {
        return "bin";
    }
    default:
        UNREACHABLE();
    }
}

void DataBase::Open() {
    if (opened) {
        return;
    }

    using namespace Common::FS;
    // bbport: BB_CACHE_DIR points every install (installer edition, portable edition, a second
    // build) at one cache root instead of three separate copies under each user directory and
    // %APPDATA%; the blobs are per game serial and share nothing else.
    std::filesystem::path cache_root = GetUserPath(PathType::CacheDir);
    if (const char* override_dir = std::getenv("BB_CACHE_DIR"); override_dir != nullptr && *override_dir != '\0') {
        cache_root = std::filesystem::u8path(override_dir);
    }
    Open(cache_root, Common::ElfInfo::Instance().GameSerial());
}

void DataBase::Open(const std::filesystem::path& root, std::string_view serial) {
    if (opened) {
        return;
    }
    cache_stats.hits = cache_stats.misses = cache_stats.read_bytes = cache_stats.written_bytes = 0;
    std::error_code error;
    std::filesystem::create_directories(root, error);
    if (error) {
        LOG_WARNING(Render, "Shader cache unavailable at {}: {}", root.string(), error.message());
        return;
    }
    if (EmulatorSettings.IsPipelineCacheArchived()) {
        mz_zip_zero_struct(&zip_ar);
        ar_is_read_only = true;

        cache_path = root / std::filesystem::path{serial}.replace_extension(".zip");

        if (!mz_zip_reader_init_file(&zip_ar, cache_path.string().c_str(),
                                     MZ_ZIP_FLAG_READ_ALLOW_WRITING) ||
            !mz_zip_validate_archive(&zip_ar, 0)) {
            LOG_INFO(Render, "Cache archive {} is not found or archive is corrupted",
                     cache_path.string().c_str());
            mz_zip_reader_end(&zip_ar);
            if (!mz_zip_writer_init_file(&zip_ar, cache_path.string().c_str(), 0)) {
                LOG_WARNING(Render, "Cannot create shader cache archive {}", cache_path.string());
                return;
            }
            ar_is_read_only = false;
        }
    } else {
        cache_path = root / serial;
        std::filesystem::create_directories(cache_path, error);
        if (error) {
            LOG_WARNING(Render, "Shader cache unavailable at {}: {}", cache_path.string(), error.message());
            return;
        }
        PruneCache(cache_path);
    }

    io_worker = std::jthread{ProcessIO};
    opened = true;
}

void DataBase::Close() {
    if (!IsOpened()) {
        return;
    }

    io_worker.request_stop();
    io_worker.join();

    if (EmulatorSettings.IsPipelineCacheArchived()) {
        mz_zip_writer_finalize_archive(&zip_ar);
        mz_zip_writer_end(&zip_ar);
    }

    opened = false;
    ReportStatistics();
    LOG_INFO(Render, "Cache dumped");
}

CacheStatistics DataBase::Statistics() const {
    return {cache_stats.hits.load(std::memory_order_relaxed),
            cache_stats.misses.load(std::memory_order_relaxed),
            cache_stats.read_bytes.load(std::memory_order_relaxed),
            cache_stats.written_bytes.load(std::memory_order_relaxed)};
}

void DataBase::ReportStatistics() const {
    if (CacheStatsEnabled()) {
        const u64 hits = cache_stats.hits.load(std::memory_order_relaxed);
        const u64 misses = cache_stats.misses.load(std::memory_order_relaxed);
        const u64 total = hits + misses;
        LOG_INFO(Render,
                 "Shader cache: {} hit(s) / {} miss(es) ({:.1f}% reuse), {} MiB read, {} MiB written, "
                 "dir {}",
                 hits, misses, total ? 100.0 * static_cast<double>(hits) / static_cast<double>(total) : 0.0,
                 cache_stats.read_bytes.load(std::memory_order_relaxed) >> 20,
                 cache_stats.written_bytes.load(std::memory_order_relaxed) >> 20, cache_path.string());
    }
}

bool DataBase::Flush(std::chrono::milliseconds timeout) {
    if (!IsOpened()) {
        return true;
    }
    std::unique_lock lk{m_request};
    return idle_cv.wait_for(lk, timeout, [] { return req_queue.empty() && !io_busy; });
}

template <typename T>
bool WriteVector(const BlobType type, std::filesystem::path&& path_, std::vector<T>&& v) {
    {
        // bbport: the payload moves into the queue instead of being copied on the GPU thread.
        std::function<void()> request = [type, path_ = std::move(path_), v = std::move(v)]() {
            auto path{path_};
            path.replace_extension(GetBlobFileExtension(type));
            if (EmulatorSettings.IsPipelineCacheArchived()) {
                ASSERT_MSG(!ar_is_read_only,
                           "The archive is read-only. Did you forget to call `FinishPreload`?");
                if (!mz_zip_writer_add_mem(&zip_ar, path.string().c_str(), v.data(),
                                           v.size() * sizeof(T), MZ_BEST_COMPRESSION)) {
                    LOG_ERROR(Render, "Failed to add {} to the archive", path.string().c_str());
                } else {
                    cache_stats.written_bytes += v.size() * sizeof(T);
                }
            } else {
                // bbport: write beside and rename into place, so a game that ends during the
                // write never leaves a truncated blob that
                // the next start would load.
                using namespace Common::FS;
                auto temp{path};
                temp += ".tmp";
                {
                    const auto file = IOFile{temp, FileAccessMode::Create};
                    if (!file.IsOpen() || file.Write(v) != v.size()) {
                        return;
                    }
                }
                std::error_code error;
                std::filesystem::rename(temp, path, error);
                if (!error) {
                    cache_stats.written_bytes += v.size() * sizeof(T);
                }
            }
        };
        std::scoped_lock lock{m_request};
        req_queue.emplace(std::move(request));
    }

    request_cv.notify_one();
    return true;
}

template <typename T>
void LoadVector(BlobType type, std::filesystem::path& path, std::vector<T>& v) {
    using namespace Common::FS;
    v.clear();
    path.replace_extension(GetBlobFileExtension(type));
    const auto miss = [] { cache_stats.misses.fetch_add(1, std::memory_order_relaxed); };
    u64 bytes = 0;
    if (EmulatorSettings.IsPipelineCacheArchived()) {
        const int index = mz_zip_reader_locate_file(&zip_ar, path.string().c_str(), nullptr, 0);
        mz_zip_archive_file_stat stat{};
        if (index < 0 || !mz_zip_reader_file_stat(&zip_ar, index, &stat) ||
            stat.m_uncomp_size % sizeof(T) != 0) {
            miss();
            return;
        }
        bytes = stat.m_uncomp_size;
        v.resize(bytes / sizeof(T));
        if (!mz_zip_reader_extract_to_mem(&zip_ar, index, v.data(), bytes, 0)) {
            v.clear();
            miss();
            return;
        }
    } else {
        const auto file = IOFile{path, FileAccessMode::Read};
        if (!file.IsOpen()) {
            miss();
            return;
        }
        bytes = file.GetSize();
        if (bytes % sizeof(T) != 0) {
            miss();
            return;
        }
        v.resize(bytes / sizeof(T));
        if (file.Read(v) != v.size()) {
            v.clear();
            miss();
            return;
        }
    }
    cache_stats.hits.fetch_add(1, std::memory_order_relaxed);
    cache_stats.read_bytes.fetch_add(bytes, std::memory_order_relaxed);
}

bool DataBase::Save(BlobType type, const std::string& name, std::vector<u8>&& data) {
    if (!opened) {
        return false;
    }

    auto path = EmulatorSettings.IsPipelineCacheArchived() ? std::filesystem::path{name}
                                                           : cache_path / name;
    return WriteVector(type, std::move(path), std::move(data));
}

bool DataBase::Save(BlobType type, const std::string& name, std::vector<u32>&& data) {
    if (!opened) {
        return false;
    }

    auto path = EmulatorSettings.IsPipelineCacheArchived() ? std::filesystem::path{name}
                                                           : cache_path / name;
    return WriteVector(type, std::move(path), std::move(data));
}

void DataBase::Load(BlobType type, const std::string& name, std::vector<u8>& data) {
    if (!opened) {
        return;
    }

    auto path = EmulatorSettings.IsPipelineCacheArchived() ? std::filesystem::path{name}
                                                           : cache_path / name;
    return LoadVector(type, path, data);
}

void DataBase::Load(BlobType type, const std::string& name, std::vector<u32>& data) {
    if (!opened) {
        return;
    }

    auto path = EmulatorSettings.IsPipelineCacheArchived() ? std::filesystem::path{name}
                                                           : cache_path / name;
    return LoadVector(type, path, data);
}

void DataBase::ForEachBlob(BlobType type, const std::function<void(std::vector<u8>&& data)>& func) {
    const auto& ext = GetBlobFileExtension(type);
    if (EmulatorSettings.IsPipelineCacheArchived()) {
        const auto num_files = mz_zip_reader_get_num_files(&zip_ar);
        for (int index = 0; index < num_files; ++index) {
            std::array<char, MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE> file_name{};
            file_name.fill(0);
            mz_zip_reader_get_filename(&zip_ar, index, file_name.data(), file_name.size());
            if (std::string{file_name.data()}.ends_with(ext)) {
                mz_zip_archive_file_stat stat{};
                mz_zip_reader_file_stat(&zip_ar, index, &stat);
                std::vector<u8> data(stat.m_uncomp_size);
                mz_zip_reader_extract_to_mem(&zip_ar, index, data.data(), data.size(), 0);
                func(std::move(data));
            }
        }
    } else {
        for (const auto& file_name : std::filesystem::directory_iterator{cache_path}) {
            if (file_name.path().extension().string().ends_with(ext)) {
                using namespace Common::FS;
                const auto& file = IOFile{file_name, FileAccessMode::Read};
                if (file.IsOpen()) {
                    std::vector<u8> data(file.GetSize());
                    file.Read(data);
                    func(std::move(data));
                }
            }
        }
    }
}

void DataBase::Clear() {
    if (!opened || EmulatorSettings.IsPipelineCacheArchived()) {
        return;
    }
    std::error_code ec;
    u64 removed = 0;
    for (const auto& entry : std::filesystem::directory_iterator(cache_path, ec)) {
        const auto ext = entry.path().extension();
        if (ext != ".spv" && ext != ".meta" && ext != ".key" && ext != ".bin" && ext != ".tmp") {
            continue;
        }
        if (std::filesystem::is_regular_file(entry.symlink_status(ec)) &&
            std::filesystem::remove(entry.path(), ec)) {
            ++removed;
        }
    }
    LOG_WARNING(Render, "Pipeline cache cleared ({} files): it is rebuilt for this build", removed);
}

void DataBase::FinishPreload() {
    if (EmulatorSettings.IsPipelineCacheArchived() && ar_is_read_only) {
        mz_zip_writer_init_from_reader(&zip_ar, cache_path.string().c_str());
        ar_is_read_only = false;
    }
}

} // namespace Storage
