#include "scull.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <sys/ioctl.h>

#define DEVICE_PATH "/dev/scull0"

int quantum_size = DEFAULT_QUANTUM_SIZE;

void fill_buffer_with_random_chars(char *buf, int len)
{
        for (int i = 0; i < len; i++)
                buf[i] = rand() % 255;
}

void log_success(const char *test_name)
{
        printf("[SUCCESS] %s\n", test_name);
}

void log_fail(const char *test_name)
{
        printf("[FAIL] %s\n", test_name);
}

bool is_read_after_write_one_quantum_random_bytes_match(int len)
{
        bool success = false;
        int fd = -1;
        char *write_buf = NULL;
        char *read_buf = NULL;
        ssize_t n;

        fd = open(DEVICE_PATH, O_WRONLY);
        if (fd < 0)
                goto fail;
        write_buf = malloc(len);
        if (!write_buf)
                goto fail;
        fill_buffer_with_random_chars(write_buf, len);
        n = write(fd, write_buf, len);
        if (n != len)
                goto fail;
        close(fd);
        fd = open(DEVICE_PATH, O_RDONLY);
        if (fd < 0)
                goto fail;
        read_buf = malloc(len);
        if (!read_buf)
                goto fail;
        n = read(fd, read_buf, len);
        if (n != len || memcmp(write_buf, read_buf, len) != 0)
                goto fail;
        success = true;
fail:
        if (fd >= 0)
                close(fd);
        if (write_buf)
                free(write_buf);
        if (read_buf)
                free(read_buf);
        return success;
}

void test_one_quantum_write(void)
{
        if (is_read_after_write_one_quantum_random_bytes_match(quantum_size))
                log_success("test_one_quantum_write");
        else
                log_fail("test_one_quantum_write");
}

void test_multiple_quantum_write(void)
{
        for (int i = 0; i < 1000; i++) {
                if (!is_read_after_write_one_quantum_random_bytes_match(quantum_size)) {
                        log_fail("test_multiple_quantum_write");
                        return;
                }
        }
        log_success("test_multiple_quantum_write");
}

void test_margin_quantum_write(void)
{
        bool success = false;
        int fd = -1;
        int extra_size = 100;
        int total_size = quantum_size + extra_size;
        ssize_t n;
        char *write_buf = NULL;
        char *read_buf = NULL;

        fd = open(DEVICE_PATH, O_WRONLY);
        if (fd < 0)
                goto fail;
        write_buf = malloc(total_size);
        if (!write_buf)
                goto fail;
        fill_buffer_with_random_chars(write_buf, total_size);
        n = write(fd, write_buf, total_size);
        if (n != quantum_size)
                goto fail;
        n = write(fd, write_buf + quantum_size, extra_size);
        if (n != extra_size)
                goto fail;
        close(fd);
        fd = open(DEVICE_PATH, O_RDONLY);
        if (fd < 0)
                goto fail;
        read_buf = malloc(total_size);
        if (!read_buf)
                goto fail;
        n = read(fd, read_buf, total_size);
        if (n != quantum_size)
                goto fail;
        n = read(fd, read_buf + quantum_size, extra_size);
        if (n != extra_size || memcmp(read_buf, write_buf, total_size) != 0)
                goto fail;
        success = true;
fail:
        if (fd >= 0)
                close(fd);
        if (write_buf)
                free(write_buf);
        if (read_buf)
                free(read_buf);
        if (success)
                log_success("test_margin_quantum_write");
        else
                log_fail("test_margin_quantum_write");
}

void test_ioctl_set_and_get_quantum_size(void)
{
        bool success = false;
        int fd = -1;
        int rc;
        int get_quantum_size;
        int set_quantum_size = 2500;
        int swap_quantum_size_value = 3000;
        int swap_quantum_size = swap_quantum_size_value;

        fd = open(DEVICE_PATH, O_RDWR);
        if (fd < 0)
                goto fail;
        rc = ioctl(fd, SCULL_IOC_SET_QUANTUM_SIZE, &set_quantum_size);
        if (rc != 0)
                goto fail;
        rc = ioctl(fd, SCULL_IOC_GET_QUANTUM_SIZE, &get_quantum_size);
        if (rc != 0 || get_quantum_size != set_quantum_size || !is_read_after_write_one_quantum_random_bytes_match(set_quantum_size))
                goto fail;
        rc = ioctl(fd, SCULL_IOC_SET_AND_GET_QUANTUM_SIZE, &swap_quantum_size);
        if (rc != 0 || swap_quantum_size != set_quantum_size)
                goto fail;
        rc = ioctl(fd, SCULL_IOC_GET_QUANTUM_SIZE, &get_quantum_size);
        if (rc != 0 || get_quantum_size != swap_quantum_size_value || !is_read_after_write_one_quantum_random_bytes_match(swap_quantum_size_value))
                goto fail;
        success = true;
fail:
        if (fd >= 0)
                close(fd);
        if (success)
                log_success("test_ioctl_set_and_get_quantum_size");
        else
                log_fail("test_ioctl_set_and_get_quantum_size");
}

void test_ioctl_set_and_get_num_quantum(void)
{
        bool success = false;
        int fd = -1;
        int set_num_quantum = 200;
        int get_num_quantum;
        int swap_num_quantum_value = 300;
        int swap_num_quantum = swap_num_quantum_value;
        int rc;

        fd = open(DEVICE_PATH, O_RDWR);
        if (fd < 0)
                goto fail;
        rc = ioctl(fd, SCULL_IOC_SET_NUM_QUANTUM, &set_num_quantum);
        if (rc != 0)
                goto fail;
        rc = ioctl(fd, SCULL_IOC_GET_NUM_QUANTUM, &get_num_quantum);
        if (rc != 0 || get_num_quantum != set_num_quantum)
                goto fail;
        rc = ioctl(fd, SCULL_IOC_SET_AND_GET_NUM_QUANTUM, &swap_num_quantum);
        if (rc != 0 || swap_num_quantum != set_num_quantum)
                goto fail;
        rc = ioctl(fd, SCULL_IOC_GET_NUM_QUANTUM, &get_num_quantum);
        if (rc != 0 || get_num_quantum != swap_num_quantum_value)
                goto fail;
        success = true;
fail:
        if (fd >= 0)
                close(fd);
        if (success)
                log_success("test_ioctl_set_and_get_num_quantum");
        else
                log_fail("test_ioctl_set_and_get_num_quantum");
}

void test_ioctl_reset(void)
{
        bool success = false;
        int fd = -1;
        int rc;
        int set_quantum_size = 2800;
        int get_quantum_size;
        int set_num_quantum = 500;
        int get_num_quantum;

        fd = open(DEVICE_PATH, O_RDWR);
        if (fd < 0)
                goto fail;
        rc = ioctl(fd, SCULL_IOC_SET_QUANTUM_SIZE, &set_quantum_size);
        if (rc != 0)
                goto fail;
        rc = ioctl(fd, SCULL_IOC_SET_NUM_QUANTUM, &set_num_quantum);
        if (rc != 0)
                goto fail;
        rc = ioctl(fd, SCULL_IOC_RESET);
        if (rc != 0)
                goto fail;
        rc = ioctl(fd, SCULL_IOC_GET_QUANTUM_SIZE, &get_quantum_size);
        if (rc != 0 || get_quantum_size != DEFAULT_QUANTUM_SIZE)
                goto fail;
        rc = ioctl(fd, SCULL_IOC_GET_NUM_QUANTUM, &get_num_quantum);
        if (rc != 0 || get_num_quantum != DEFAULT_NUM_QUANTUM)
                goto fail;
        success = true;
fail:
        if (fd >= 0)
                close(fd);
        if (success)
                log_success("test_ioctl_reset");
        else
                log_fail("test_ioctl_reset");
}

void test_ioctl_tell_and_query_quantum_size(void)
{
        bool success = false;
        int fd = -1;
        int rc;
        int tell_quantum_size = 2000;
        int swap_quantum_size = 3000;
        int query_quantum_size = -1;

        fd = open(DEVICE_PATH, O_RDWR);
        if (fd < 0)
                goto fail;
        rc = ioctl(fd, SCULL_IOC_TELL_QUANTUM_SIZE, tell_quantum_size);
        if (rc != 0)
                goto fail;
        query_quantum_size = ioctl(fd, SCULL_IOC_QUERY_QUANTUM_SIZE);
        if (query_quantum_size != tell_quantum_size || !is_read_after_write_one_quantum_random_bytes_match(tell_quantum_size))
                goto fail;
        query_quantum_size = ioctl(fd, SCULL_IOC_TELL_AND_QUERY_QUANTUM_SIZE, swap_quantum_size);
        if (query_quantum_size != tell_quantum_size)
                goto fail;
        query_quantum_size = ioctl(fd, SCULL_IOC_QUERY_QUANTUM_SIZE);
        if (query_quantum_size != swap_quantum_size || !is_read_after_write_one_quantum_random_bytes_match(swap_quantum_size))
                goto fail;
        success = true;
fail:
        if (fd >= 0)
                close(fd);
        if (success)
                log_success("test_ioctl_tell_and_query_quantum_size");
        else
                log_fail("test_ioctl_tell_and_query_quantum_size");
}

void test_ioctl_tell_and_query_num_quantum(void)
{
        bool success = false;
        int fd = -1;
        int rc;
        int tell_num_quantum = 600;
        int swap_num_quantum = 700;
        int query_num_quantum;

        fd = open(DEVICE_PATH, O_RDWR);
        if (fd < 0)
                goto fail;
        rc = ioctl(fd, SCULL_IOC_TELL_NUM_QUANTUM, tell_num_quantum);
        if (rc != 0)
                goto fail;
        query_num_quantum = ioctl(fd, SCULL_IOC_QUERY_NUM_QUANTUM);
        if (query_num_quantum != tell_num_quantum)
                goto fail;
        query_num_quantum = ioctl(fd, SCULL_IOC_TELL_AND_QUERY_NUM_QUANTUM, swap_num_quantum);
        if (query_num_quantum != tell_num_quantum)
                goto fail;
        query_num_quantum = ioctl(fd, SCULL_IOC_QUERY_NUM_QUANTUM);
        if (query_num_quantum != swap_num_quantum)
                goto fail;
        success = true;
fail:
        if (fd >= 0)
                close(fd);
        if (success)
                log_success("test_ioctl_tell_and_query_num_quantum");
        else
                log_fail("test_ioctl_tell_and_query_num_quantum");
}

int main(void)
{
        srand(time(NULL));
        test_one_quantum_write();
        test_multiple_quantum_write();
        test_margin_quantum_write();
        test_ioctl_set_and_get_quantum_size();
        test_ioctl_set_and_get_num_quantum();
        test_ioctl_reset();
        test_ioctl_tell_and_query_quantum_size();
        test_ioctl_tell_and_query_num_quantum();
}
