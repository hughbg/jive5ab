#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <vdif_queue_reader.h>


void rawdump(const char *fname, const unsigned char *p, unsigned count);

static runtime rte;



int main() {

    VDIFQueueReader vdif(8032, 4, 1, 22001);   


    // VDIFQueueReader will have requested an interchain queue
    // and set it to enable pop and started running.


    int fd;
    if ( (fd = creat("jive5ab_chain_out.vdif", S_IRUSR|S_IWUSR)) == -1 ) {
        std::cout << "Failed to jive5ab_chain_out.vdif" << std::endl;
        return 1;
    }
    
    while ( true ) {
        unsigned char *frame = vdif.read_frame();

        //std::cout << "frame write, block ptr " << (uint64_t)vdif.get_block_ptr() << " frame ptr " << (uint64_t)frame << "\n";
        if ( write(fd, frame, vdif.get_frame_length()) != vdif.get_frame_length()) {
            std::cout << "Failed block write" << std::endl;
            return 1;
        }

    }
    
    close(fd);

    return 0;

}
