#include <sys/uio.h>
#include <interchain.h>

typedef std::pair<struct iovec *, size_t> iovec_result;

class VDIFQueueReader {
private:
    bqueue<block>* interchain_queue;

public:
    VDIFQueueReader() {
        interchain_queue = request_interchain_queue();
        interchain_queue->enable_pop_only();
    }

    iovec_result read_block();
};
