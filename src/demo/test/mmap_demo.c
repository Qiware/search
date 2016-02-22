#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>

#define SLOT_DATA_MAX_NUM  (126322568)

typedef struct
{
    int id;
    char data[30];
} slot_data_t;


int  _fwrite(const char *fname, size_t num);
int  _fread(const char *fname, size_t num);
int  _fread2(const char *fname, size_t num);
int  _mmap(const char *fname, size_t num);
int  _mmap2(const char *fname, size_t num);

int main(int argc, char *argv[])
{
    int opt;

    if (2 != argc) {
        return -1;
    }

    opt = atoi(argv[1]);
    switch (opt) {
        case 1:
        default:
        {
            return _fwrite("test.data", SLOT_DATA_MAX_NUM);
        }
        case 2:
        {
            return _fread("test.data", 5);
        }
        case 3:
        {
            return _mmap("test.data", 5);
        }
        case 4:
        {
            return _fread2("test.data", 5);
        }
        case 5:
        {
            return _mmap2("test.data", 5);
        }
    }

    return 0;
}

int _fwrite(const char *fname, size_t num)
{
    int fd, n;
    size_t idx;
    slot_data_t data;

    fd = open(fname, O_CREAT|O_RDWR, 0666);
    if (fd < 0) {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    for (idx=0; idx<num; ++idx) {
        data.id = idx;
        n = write(fd, &data, sizeof(data));
        if (n != sizeof(data)) {
            fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
            break;
        }
    }

    close(fd);

    return 0;
}

int  _fread(const char *fname, size_t num)
{
    int fd, n;
    size_t idx, off, total;
    struct stat st;
    slot_data_t data;

    fd = open(fname, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    fstat(fd, &st);

    total = st.st_size / sizeof(data);
    off = total/num * sizeof(data);

    for (idx=0; idx<num; ++idx) {
        lseek(fd, st.st_size - off * (idx + 1), SEEK_SET);

        n = read(fd, &data, sizeof(data));
        if (n != sizeof(data)) {
            fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
            break;
        }

        //fprintf(stderr, "[%d] Data id:%d\n", idx, data.id);
    }

    close(fd);

    return 0;
}

int  _fread2(const char *fname, size_t num)
{
    int fd, n;
    size_t idx, off;
    struct stat st;
    slot_data_t data;

    fd = open(fname, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    fstat(fd, &st);

    for (idx=0; idx<num; ++idx) {
        lseek(fd, st.st_size - sizeof(data) * (idx + 1), SEEK_SET);

        n = read(fd, &data, sizeof(data));
        if (n != sizeof(data)) {
            fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
            break;
        }

        //fprintf(stderr, "[%d] Data id:%d\n", idx, data.id);
    }

    close(fd);

    return 0;
}

int  _mmap(const char *fname, size_t num)
{
    void *addr;
    int fd;
    size_t idx, off, total;
    struct stat st;
    slot_data_t data;

    fd = open(fname, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    fstat(fd, &st);

    addr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    total = st.st_size / sizeof(data);
    off = total/num * sizeof(data);

    for (idx=0; idx<num; ++idx) {
        memcpy(&data, addr + st.st_size - off * (idx + 1), sizeof(data));
    }

    munmap(addr, st.st_size);
    close(fd);

    return 0;
}

int  _mmap2(const char *fname, size_t num)
{
    int fd;
    void *addr;
    size_t idx;
    struct stat st;
    slot_data_t data;

    fd = open(fname, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    fstat(fd, &st);

    addr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    for (idx=0; idx<num; ++idx) {
        memcpy(&data, addr + st.st_size - sizeof(data) * (idx + 1), sizeof(data));
    }

    munmap(addr, st.st_size);
    close(fd);

    return 0;
}
