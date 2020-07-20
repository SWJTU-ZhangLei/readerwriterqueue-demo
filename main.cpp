#include <stdio.h>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <thread>
#include <vector>
#include "readerwriterqueue.h"


class Frame {
public:
    Frame() { size = 0; ptr = nullptr;};
    Frame(uint32_t _size) {
	size = _size;
        ptr  = new uint8_t[_size];
    };
    Frame(Frame& frame) {
        size = frame.size;
        ptr  = new uint8_t[size];
    };
    Frame(Frame&& frame) {
        size      = frame.size;
	ptr       = frame.ptr;
	frame.ptr = nullptr;
    };
    Frame& operator=(Frame& frame) {
        size = frame.size;
        ptr  = new uint8_t[size];
    
    };
    Frame& operator=(Frame&& frame) {
        size      = frame.size;
	ptr       = frame.ptr;
	frame.ptr = nullptr;
    }    
    ~Frame(){ 
        if (ptr) {
	    delete[] ptr;
	}
    };
private:
    uint32_t size;
    uint8_t* ptr;
};

using my_queue = moodycamel::ReaderWriterQueue<Frame>;
static std::shared_ptr<my_queue> pool                   = std::make_shared<my_queue>(10);
static std::shared_ptr<std::mutex> pool_lock            = std::make_shared<std::mutex>();
static std::shared_ptr<std::condition_variable> pool_cv = std::make_shared<std::condition_variable>();

static std::shared_ptr<my_queue> data                   = std::make_shared<my_queue>(10);
static std::shared_ptr<std::mutex> data_lock            = std::make_shared<std::mutex>();
static std::shared_ptr<std::condition_variable> data_cv = std::make_shared<std::condition_variable>();

void producer()
{
    while(true) {
        Frame frame;
	if (!pool->try_dequeue(frame)) {
	    printf("producer: pool empty, wait...\n");
	    std::unique_lock<std::mutex> pool_ulock(*pool_lock);
	    pool_cv->wait(pool_ulock);
	    pool->try_dequeue(frame);
        }
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        printf("producer: produce a frame\n");
	if (!data->try_enqueue(std::move(frame))) {
	    printf("producer: data full, wait...\n");
	    std::unique_lock<std::mutex> data_ulock(*data_lock);
	    data_cv->wait(data_ulock);
	    data->try_dequeue(frame);
	}
        data_cv->notify_one();
    }
}

void consumer()
{
   while(true) {
        Frame frame;
	if (!data->try_dequeue(frame)) {
	    printf("consumer: data empty, wait...\n");
	    std::unique_lock<std::mutex> data_ulock(*data_lock);
	    data_cv->wait(data_ulock);
	    data->try_dequeue(frame);
	}
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        printf("consumer: consume a frame\n");

	if (!pool->try_enqueue(std::move(frame))) {
	    printf("consumer: pool full, wait...\n");
	    std::unique_lock<std::mutex> pool_ulock(*pool_lock);
	    pool_cv->wait(pool_ulock);
            pool->try_dequeue(frame);
	}
        pool_cv->notify_one();
   }

}

int main()
{
    for (int i = 0; i < 50; i++) {
        bool res = pool->try_emplace(1920 * 1080);
    }
    std::vector<std::thread> vec;
    vec.emplace_back(producer);
    vec.emplace_back(consumer);
    for(auto &t: vec) {
        t.join();
    }
    return 0;
}
