#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define FNAME_SIZE	256

// TODO: Learn this from the target filesystem
// TODO: experiment with various block sizes
// TODO: support creating a range of sizes
const size_t blocksize = 24;

typedef struct {
	int fd;
	size_t blocksize;
}file_spec_t;

void warn(const char *warning) {
	fprintf(stderr, "warning: %s", warning);
}

int read_block(file_spec_t *input, void *buf) {
	size_t rd_ct = 0;
	
	if(input->fd < 0 || !buf || !input->blocksize) {
		return 0;
	}
	
	rd_ct = read(input->fd, buf, input->blocksize);
	if(rd_ct < input->blocksize) {
		warn("short read\n");
	}
	
	return 1;
}

int write_block(file_spec_t *output, void * buf) {
	size_t wr_ct = 0;
	
	if(output->fd < 0 || !buf || !output->blocksize) {
		return 0;
	}
	
	wr_ct = write(output->fd, buf, output->blocksize);
	if(wr_ct < output->blocksize) {
		warn("short write.\n");
	}
	
	return 1;
}

int open_input(int use_urand) {
	int rand_fd = open(use_urand ? "/dev/urandom" : "/dev/random", O_RDONLY | O_NOFOLLOW);
	
	if(rand_fd < 0) {
		perror("open");
	}
	
	return rand_fd;
}

int open_output(const char *fname) {
	int fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_NOFOLLOW);
	
	if(fd < 0) {
		perror("open");
		return fd;
	}
	
	// turn disk caching off 
	if(fcntl(fd, F_NOCACHE, 1) < 0) {
		perror("fcntl");
		close(fd);
		return -1;
	}
	
	if(chmod(fname, 000600) < 0) {
		perror("chmod");
		close(fd);
		return -1;
	}
	
	return fd;
}

int create_fname(char *buf, size_t bufsize, uint32_t seed) {
	return snprintf(buf, bufsize, "tmp%d.smallfile", seed + 1);
}


int small_file(uint32_t findex, file_spec_t *in, void *buf) {
	char fname[FNAME_SIZE + 1] = {0};
	int ret = 1;
	file_spec_t out = {-1};

	if(in->fd < 0 || !in->blocksize) {
		fprintf(stderr, "error: input broken.\n");
		return 0;
	}
	
	if(!create_fname(fname, FNAME_SIZE, findex)) {
		fprintf(stderr, "error: failed formatting filename.\n");
		return 0;
	}
	
	// Make the BS match
	out.blocksize = in->blocksize;
	
	out.fd = open_output(fname);
	if(out.fd < 0) {
		fprintf(stderr, "error: could not create output file.\n");
		return 0;
	}
	
	ret = 1;
	
	do {
		// read rand
		if(!read_block(in, buf)) {
			fprintf(stderr, "error: reading.\n");
			ret = 0;
			break;
		}
		
		// write to small file
		if(!write_block(&out, buf)) {
			fprintf(stderr, "error: writing.\n");
			ret = 0;
			break;
		}
   } while(0);
	
	// Force the read/write
	sync();
	
	// close the file
	close(out.fd);
	
	// Full drive write/cache sync after each file is written
	fcntl(0, F_FULLFSYNC, 0);
	
	return ret;
}

int mktmpdir(int key, char *dirname, size_t dn_sz) {
	int ret = 1;
	
	if(!snprintf(dirname, dn_sz, "./smallfiles.%d", key)) {
		return 0;
	}
	
	mode_t oldmask = umask(000022);
	
	if(mkdir(dirname, 000755) < 0) {
		perror("mkdir");
		ret = 0;
	}
	
	if(chmod(dirname, 000755) < 0) {
		perror("chmod");
		ret = 0;
	}
	
	umask(oldmask);
	return ret;
}

int setup(char *dirname, size_t dn_sz) {
	
	if(!mktmpdir(getpid(), dirname, dn_sz)) {
		fprintf(stderr, "error: failed to make tmp dir.\n");
		return 0;
	}
	
	if(chdir(dirname) < 0) {
		perror("chdir");
		return 0;
	}
	
	// disable execute bits, enable RW for owner
	umask(000177);
	
	return 1;
}

int finalize(char *dirname) {
	// TODO: rmdir() the tmp directory
	return 1;
}

int main(int argc, char *argv[]) {
	uint32_t i = 0;
	file_spec_t input = {0};
	char tmpdir[256] = {0};
	
	// TODO: Arg parsing:
	/*
	 * - allow urandom or random use
	 * - allow setting block size
	 * - allow setting file count
	 * - allow setting filename len/pattern
	 */

	// data block
	uint8_t *data = (uint8_t *)malloc(blocksize);
	if(!data) {
		perror("malloc");
		exit(-1);
	}

	fprintf(stdout, "starting up...\n");
	
	input.blocksize = blocksize;
	
	if(!setup(tmpdir, 255)) {
		fprintf(stderr, "failed to setup. bailing.\n");
		exit(-1);
	}
	
	// Don't let a symlink fool us.
	input.fd = open_input(1);
	if(input.fd < 0) {
		exit(-1);
	}
	
	fprintf(stdout, "performing writes: ");
	fflush(stdout);
	
	for(i = 0; i < 1024; i++) {
		char fname[FNAME_SIZE + 1] = {0};
		
		if(!small_file(i, &input, data)) {
			// go to cleanup phase on failure
			break;
		}
		
		fprintf(stdout, ".");
		fflush(stdout);
	}
	
	// Full drive write/cache sync
	fcntl(0, F_FULLFSYNC, 0);
	
	if(!finalize(tmpdir)) {
		fprintf(stderr, "error: failed to finalize.\n");
	}
	
	// Full drive write/cache sync
	fcntl(0, F_FULLFSYNC, 0);

	fprintf(stdout, "\ndone.\n");
	fflush(stdout);
}
