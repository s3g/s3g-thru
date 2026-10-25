#pragma once

#include "thru_waveset_map.h"
#include "thru_background_thread.h"

#include <atomic>
#include <memory>
#include <new>
#include <pthread.h>
#include <time.h>
#if defined(__linux__)
#include <sys/resource.h>
#endif

namespace s3g_daisy {

// Single-producer/single-consumer snapshot mailbox. Audio copies a fixed
// number of frames, never waits or allocates, and never lends mutable source
// memory to the worker. Completed maps replace active banks between blocks.
class ThruWavesetWorker {
public:
    enum State { idle, copying, requested, analyzing, complete };
    ~ThruWavesetWorker() { stop(); }

    bool prepare(std::size_t capacity)
    {
        capacity_ = capacity;
        snapshot_.reset(new (std::nothrow) float[capacity * 2u]);
        if (!snapshot_) return false;
        running_.store(true, std::memory_order_release);
        if (!startBackgroundThread(thread_, entry, this)) {
            running_.store(false, std::memory_order_release);
            return false;
        }
        started_ = true;
        return true;
    }

    void stop()
    {
        if (!started_) return;
        running_.store(false, std::memory_order_release);
        pthread_join(thread_, nullptr);
        started_ = false;
    }

    State state() const { return state_.load(std::memory_order_acquire); }
    std::size_t lane() const { return lane_; }
    std::uint64_t revision() const { return revision_; }
    const ThruWavesetBank* bank(std::size_t lane) const
    {
        return lane < active_.size() ? active_[lane].get() : nullptr;
    }

    void begin(std::size_t lane, std::uint64_t revision,
               std::size_t frames, float rate)
    {
        if (!started_ || state() != idle || lane >= active_.size()
            || frames == 0u || frames > capacity_) return;
        lane_ = lane;
        revision_ = revision;
        frames_ = frames;
        rate_ = rate;
        copied_ = 0u;
        state_.store(copying, std::memory_order_release);
    }

    void cancelCopy()
    {
        if (state() == copying) state_.store(idle, std::memory_order_release);
    }

    std::size_t copy(const float* left, const float* right, std::size_t budget)
    {
        if (state() != copying || !left || !right) return 0u;
        const auto count = std::min(budget, frames_ - copied_);
        std::copy_n(left + copied_, count, snapshot_.get() + copied_);
        std::copy_n(right + copied_, count,
                    snapshot_.get() + capacity_ + copied_);
        copied_ += count;
        if (copied_ == frames_) state_.store(requested, std::memory_order_release);
        return count;
    }

    // Call accept first, install bank() on all readers, then releaseResult.
    // Retired map destruction happens on the worker after that acknowledgement.
    bool accept(bool currentRevision)
    {
        if (state() != complete) return false;
        if (currentRevision && result_) {
            retired_ = std::move(active_[lane_]);
            active_[lane_] = std::move(result_);
            return true;
        }
        return false;
    }

    void releaseResult() { state_.store(idle, std::memory_order_release); }

private:
    static void* entry(void* opaque)
    {
        auto& self = *static_cast<ThruWavesetWorker*>(opaque);
        configureBackgroundThread();
        while (self.running_.load(std::memory_order_acquire)) {
            if (self.state() != requested) {
                const timespec delay {0, 1000000};
                nanosleep(&delay, nullptr);
                continue;
            }
            self.state_.store(analyzing, std::memory_order_release);
            self.retired_.reset();
            self.result_.reset();
            try {
                self.result_ = std::make_unique<ThruWavesetBank>();
                self.result_->analyze(self.snapshot_.get(),
                    self.snapshot_.get() + self.capacity_, self.frames_,
                    self.rate_, self.revision_);
            } catch (...) {
                self.result_.reset();
            }
            self.state_.store(complete, std::memory_order_release);
        }
        return nullptr;
    }

    pthread_t thread_ {};
    bool started_ = false;
    std::atomic<bool> running_ {false};
    std::atomic<State> state_ {idle};
    std::unique_ptr<float[]> snapshot_;
    std::array<std::unique_ptr<ThruWavesetBank>, 4u> active_;
    std::unique_ptr<ThruWavesetBank> result_, retired_;
    std::size_t capacity_ = 0u, frames_ = 0u, copied_ = 0u, lane_ = 0u;
    float rate_ = 44100.0f;
    std::uint64_t revision_ = 0u;
};

} // namespace s3g_daisy
