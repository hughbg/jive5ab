// This process lives outside the chain/queue system and extracts vdif frames in blocks from memory (interchain_queue)
#include <interchain.h>
#include <runtime.h>
#include <vdif_queue_reader.h>

extern void net2mem_fn(runtime& rte);

unsigned extract_frame_length(unsigned *header) {
    return (header[2] & 0xffffff)*8;
}

VDIFQueueReader::VDIFQueueReader(unsigned frame_size, unsigned bits_per_sample, unsigned nchannel, unsigned network_port)
                                : block_data(NULL), block_length(0), frame_length(0), frame_ptr(NULL) {
    interchain_queue = request_interchain_queue();
    interchain_queue->enable_pop_only();
    
    rte = new runtime;
    
    rte->set_framesize(frame_size);
    rte->set_bits_per_sample(bits_per_sample);
    rte->set_nchannel(nchannel);
    rte->set_blocksize(16*rte->get_framesize());
    rte->port = network_port;
    rte->nblock = 8;
    
    // Set up and run reading chain
    net2mem_fn(*rte);
}

VDIFQueueReader::~VDIFQueueReader() {
    if ( block_data != NULL ) delete[] block_data;
    if ( rte != NULL ) delete rte;
}


    
unsigned char *VDIFQueueReader::read_frame() {  
    unsigned char *frame;
    
    // trigger a read if necessary
    if ( frame_ptr == NULL || frame_ptr >= block_data+block_length ) {
        read_block();
        frame_ptr = block_data;
    }

    frame = frame_ptr;      // return this one
    
    frame_ptr += frame_length;    // tick ptr to next frame
    
    return frame;
}
    
void VDIFQueueReader::read_block() {
    block b;
    block::const_iterator bptr;
    unsigned char* in_block_ptr;
    unsigned bsize;




    // the queue pop waits for data
    bool did_pop = interchain_queue->pop(b);        // this pop is in bqueue.h
    
    if ( !did_pop ) { 
        std::cout << "pop returned FALSE in VDIFQueueReader. Should not happen.\n";   // it might if queue is disabled
        block_length = frame_length = 0;
        return;
    }

    // Get the total size
    for( bptr=b.begin(), bsize=0; bptr!=b.end(); bptr++) {
        bsize += bptr->iov_len;
    }

    //std::cout << "block read len" << " " << bsize << "\n";
    if ( block_data == NULL ) { 
        // can initialize
        block_data = new unsigned char[bsize];
        block_length = bsize;
        frame_length = extract_frame_length((unsigned*)b.begin()->iov_base);

    }

    // Now get the block data
    for( bptr=b.begin(), in_block_ptr=block_data; bptr!=b.end(); bptr++) {
        memcpy(in_block_ptr, bptr->iov_base, bptr->iov_len);
        
        in_block_ptr += bptr->iov_len;
    }

}
