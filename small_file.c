#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define FNAME_SIZE	256

size_t getBlocksize (void) {
    size_t blocksize = 1024;
    struct stat st;
    if (stat(".", &st) == 0) {
        blocksize = st.st_blksize;        
    }
    return blocksize;
}

int main(int argc, char *argv[]) {
	uint32_t i = 0;
	int rand_fd = 0, output_fd = 0;
    size_t blocksize = getBlocksize();

	// data block
	uint8_t *data = (uint8_t *)malloc(blocksize);
	if(!data) {
		perror("malloc");
		exit(-1);
	}
	
	// Don't let a symlink fool us.
	// TODO: Allow using urandom or random
	rand_fd = open("/dev/urandom", O_RDONLY | O_NOFOLLOW);
	if(rand_fd < 0) {
		perror("open");
		exit(-1);
	}

	fprintf(stdout, "starting up...\n");

	// disable execute bits, enable RW for owner
	umask(177);
	
	fprintf(stdout, "performing writes: ");
	fflush(stdout);
	
	for(i = 0; i < 1024; i++) {
		char fname[FNAME_SIZE + 1] = {0};
		ssize_t rd_ct = 0, wr_ct = 0;
		
		fprintf(stdout, ".");
		fflush(stdout);
		
		// TODO: Better random filename
		if(!snprintf(fname, FNAME_SIZE, "tmp%d.smallfile", i + 1)) {
			perror("\nsnprintf");
			exit(-2);
		}
		
		output_fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_NOFOLLOW);
		if(output_fd < 0) {
			perror("\nopen");
			exit(-3);
		}
		
		// turn disk caching off 
		fcntl(output_fd, F_NOCACHE, 1);
		
		// do the block read/write
		rd_ct = read(rand_fd, data, blocksize);
		if(rd_ct != blocksize) {
			fprintf(stderr, "warning: small block.\n");
		}
		
		wr_ct = write(output_fd, data, blocksize);
		if(wr_ct != blocksize) {
			fprintf(stderr, "warning: small block.\n");
		}
		
		// Force the write
		sync();
		// Full drive write/cache sync
		fcntl(0, F_FULLFSYNC, 0);
		
		// close the file
		close(output_fd);
		
		// kill the file
		unlink(fname);
	}
	
	// Full drive write/cache sync
	fcntl(0, F_FULLFSYNC, 0);

	fprintf(stdout, "\ndone.\n");
	fflush(stdout);
}
