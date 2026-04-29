#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "streamvbyte.h"

#define MAX_INTEGERS 10000

size_t read_integers(const char* filename, uint32_t* buffer);
void write_file_or_die(const char* filename, ssize_t length, const uint8_t* data);

// This application reads decimal-encoded integers from an input file
// and compresses them using StreamVByte. There's a fairly low limit
// on how much data is allowed to be in the input file because this
// application is, for the moment, intended to measure the compression
// ratio.
// If the second argument, the output filename, is absent, then this
// application does not write the compressed data to the file system,
// but it still prints out information about the compression ratio.
int main(int argc, char *argv[]){
  if(argc != 2 && argc != 3) {
    fprintf(stderr, "Expected arguments: IN_FILENAME, [OUT_FILENAME]\n");
    exit(EXIT_FAILURE);
  }
  char* in_filename = argv[1];
  char* out_filename = NULL;
  if(argc == 3) {
    out_filename = argv[2];
  }
  
  uint32_t input[MAX_INTEGERS] = {0};
  size_t count = read_integers(in_filename, input);
	uint8_t* compressed_buffer = malloc(streamvbyte_max_compressedbytes(count));
	size_t compressed_size = streamvbyte_encode(input, count, compressed_buffer);
	size_t millibytes_per_element = (1000 * compressed_size) / count;
  fprintf(
    stdout,
    "Compressed %llu 32-bit integers to %llu bytes (%llu.%03llu bytes per integer)\n",
    (long long unsigned)count,
    (long long unsigned)compressed_size,
    (long long unsigned)(millibytes_per_element / 1000),
    (long long unsigned)(millibytes_per_element % 1000)
  );
  if(out_filename != NULL) {
    fprintf(stdout, "Writing file to %s\n", out_filename);
    write_file_or_die(out_filename, compressed_size, compressed_buffer);
  }
  return 0;
}

size_t read_integers(const char* filename, uint32_t* buffer) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open file '%s'\n", filename);
        exit(1);
    }

    size_t count = 0;
    uint32_t value;
    while (fscanf(fp, "%" SCNu32, &value) == 1) {
        if (count >= MAX_INTEGERS) {
            fprintf(stderr, "Fatal: more than %d integers in file\n", MAX_INTEGERS);
            fclose(fp);
            exit(1);
        }
        buffer[count++] = value;
    }

    fclose(fp);
    return count;
}

int open_trunc_or_die(const char* filename) {
  int out_fd = open(filename, O_TRUNC | O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
  if (out_fd == -1) {
    perror("Could not open output file");
    exit(EXIT_FAILURE);
  }
  return out_fd;
}

void append_or_die(int fd, ssize_t length, const uint8_t* data) {
  if (write(fd, data, length) != length) {
    fprintf(stderr, "Some kind of problem writing the output file\n");
    exit(EXIT_FAILURE);
  }
}

void write_file_or_die(const char* filename, ssize_t length, const uint8_t* data) {
  int out_fd = open_trunc_or_die(filename);
  append_or_die(out_fd, length, data);
  close(out_fd);
}
