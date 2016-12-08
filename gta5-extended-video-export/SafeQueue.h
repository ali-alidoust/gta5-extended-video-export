#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

template <class T>
class SafeQueue {
public:
	SafeQueue(uint32_t capacity)
		: q()
		, m()
		, cv_empty()
		, capacity(capacity)
	{}

	~SafeQueue(void) {}

	void enqueue(T t) {
		std::unique_lock<std::mutex> lock(m);
		while (q.size() >= capacity) {
			cv_full.wait(lock);
		}
		q.push(t);
		cv_empty.notify_one();
	}

	T dequeue(void) {
		std::unique_lock<std::mutex> lock(m);
		while (q.empty()) {
			cv_empty.wait(lock);
		}
		T val = q.front();
		q.pop();
		if (q.size() < capacity) {
			cv_full.notify_one();
		}
		return val;
	}

	int getCapacity() {
		return capacity;
	}
private:
	uint32_t capacity;
	std::queue<T> q;
	mutable std::mutex m;
	std::condition_variable cv_empty;
	std::condition_variable cv_full;
};