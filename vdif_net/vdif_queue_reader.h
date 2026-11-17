#ifndef INTERFACE
#include "runtime.h"
#endif

// Gets a block off the interchain queue and feeds it out
// frame by frame.


class VDIFQueueReader {
/* For linkage reasons don't put any code in here that refers to
  the runtime or interchain queue
*/
    
private:
#ifdef INTERFACE
    void *interchain_queue;
    void *rte;
#else
    bqueue<block> *interchain_queue;
    runtime *rte;
#endif

    unsigned char *block_data;      // raw data in contiguous memory
    unsigned block_length;
    unsigned frame_length;
    unsigned char *frame_ptr;           // index "block_data" as if it is a circular array of frames
    
    
    void read_block();
    
public:

    VDIFQueueReader(unsigned frame_size, unsigned bits_per_sample, unsigned nchannel, unsigned network_port);
    ~VDIFQueueReader();
    unsigned char *read_frame();

    unsigned get_frame_length() const { return frame_length; }
    unsigned get_block_length() const { return block_length; }
    unsigned char *get_frame_ptr() const { return frame_ptr; }
    unsigned char *get_block_ptr() const { return block_data; }
};
