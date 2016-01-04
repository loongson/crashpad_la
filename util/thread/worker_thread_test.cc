// Copyright 2015 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/thread/worker_thread.h"

#include "gtest/gtest.h"
#include "util/misc/clock.h"
#include "util/synchronization/semaphore.h"

namespace crashpad {
namespace test {
namespace {

const uint64_t kNanosecondsPerSecond = 1E9;

class WorkDelegate : public WorkerThread::Delegate {
 public:
  WorkDelegate() {}
  ~WorkDelegate() {}

  void DoWork(const WorkerThread* thread) override {
    if (++work_count_ == waiting_for_count_)
      semaphore_.Signal();
  }

  //! \brief Suspends the calling thread until the DoWork() has been called
  //!     the specified number of times.
  void WaitForWorkCount(int times) {
    waiting_for_count_ = times;
    semaphore_.Wait();
  }

  int work_count() const { return work_count_; }

 private:
  Semaphore semaphore_{0};
  int work_count_ = 0;
  int waiting_for_count_ = -1;

  DISALLOW_COPY_AND_ASSIGN(WorkDelegate);
};

TEST(WorkerThread, DoWork) {
  WorkDelegate delegate;
  WorkerThread thread(0.05, &delegate);

  uint64_t start = ClockMonotonicNanoseconds();
  thread.Start(0);
  EXPECT_TRUE(thread.is_running());

  delegate.WaitForWorkCount(2);
  thread.Stop();
  EXPECT_FALSE(thread.is_running());

  EXPECT_GE(1 * kNanosecondsPerSecond, ClockMonotonicNanoseconds() - start);
}

TEST(WorkerThread, StopBeforeDoWork) {
  WorkDelegate delegate;
  WorkerThread thread(1, &delegate);

  thread.Start(15);
  thread.Stop();

  EXPECT_EQ(0, delegate.work_count());
}

TEST(WorkerThread, Restart) {
  WorkDelegate delegate;
  WorkerThread thread(0.05, &delegate);

  thread.Start(0);
  EXPECT_TRUE(thread.is_running());

  delegate.WaitForWorkCount(1);
  thread.Stop();
  ASSERT_FALSE(thread.is_running());

  thread.Start(0);
  delegate.WaitForWorkCount(2);
  thread.Stop();
  ASSERT_FALSE(thread.is_running());
}

TEST(WorkerThread, DoWorkNow) {
  WorkDelegate delegate;
  WorkerThread thread(100, &delegate);

  thread.Start(0);
  EXPECT_TRUE(thread.is_running());

  uint64_t start = ClockMonotonicNanoseconds();

  delegate.WaitForWorkCount(1);
  EXPECT_EQ(1, delegate.work_count());

  thread.DoWorkNow();
  delegate.WaitForWorkCount(2);
  thread.Stop();
  EXPECT_EQ(2, delegate.work_count());

  EXPECT_GE(100 * kNanosecondsPerSecond, ClockMonotonicNanoseconds() - start);
}

TEST(WorkerThread, DoWorkNowAtStart) {
  WorkDelegate delegate;
  WorkerThread thread(100, &delegate);

  uint64_t start = ClockMonotonicNanoseconds();

  thread.Start(100);
  EXPECT_TRUE(thread.is_running());

  thread.DoWorkNow();
  delegate.WaitForWorkCount(1);
  EXPECT_EQ(1, delegate.work_count());

  EXPECT_GE(100 * kNanosecondsPerSecond, ClockMonotonicNanoseconds() - start);

  thread.Stop();
  EXPECT_FALSE(thread.is_running());
}

}  // namespace
}  // namespace test
}  // namespace crashpad
