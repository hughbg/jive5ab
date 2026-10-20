#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <runtime.h>
#include <vdif_queue_reader.h>

extern void net2mem_fn(runtime& rte );
void rawdump(const char *fname, const unsigned char *p, unsigned count);

static runtime rte;



int main() {

    VDIFQueueReader vdif;    // establishes queue before anything is run

    rte.set_framesize(8032);
    rte.set_bits_per_sample(4);
    rte.set_nchannel(1);
    rte.set_blocksize(16*rte.get_framesize());
    rte.nblock = 8;

    // Set up reading chain
    net2mem_fn(rte);

    sleep(3);



    // Read from memory
    iovec_result vdif_block;
    while ( (vdif_block=vdif.read_block()).first == NULL )
        sleep(1);

    int fd;
    if ( (fd = creat("vdif_block.dat", S_IRUSR|S_IWUSR)) == -1 ) {
        std::cout << "Failed" << std::endl;
        return 1;
    }

    if ( writev(fd, vdif_block.first, vdif_block.second)!=(ssize_t)rte.get_blocksize() ) {
        std::cout << "Failed" << std::endl;
        return 1;
    }
    close(fd);

    return 0;

}
