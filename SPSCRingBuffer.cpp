template<typename T, size_t CAPACITY>
class SPSCRingBuffer {
    T buffer[CAPACITY];

    // alignas(64) forces variables onto separate CPU cache lines 
    // to prevent the hardware penalty of "False Sharing"
    alignas(64) std::atomic<size_t> write_idx{0};
    alignas(64) std::atomic<size_t> read_idx{0};

public:
    void push(const T& item) {
        auto current_write = write_idx.load(std::memory_order_relaxed);
        
        // Spin lock: Wait until there is space in the buffer
        while (current_write - read_idx.load(std::memory_order_acquire) >= CAPACITY) {
            // CPU pause instruction goes here
        }

        buffer[current_write % CAPACITY] = item;
        
        // Publish the new data to the consumer thread
        write_idx.store(current_write + 1, std::memory_order_release); 
    }
};
