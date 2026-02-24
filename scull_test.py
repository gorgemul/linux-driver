#!/usr/bin/python

import os

device_path = "/dev/scull0"
device_io_boundary_size = 4000

def log_success(test_name):
    print(f"[SUCCESS] {test_name}")

def log_fail(test_name):
    print(f"[FAIL] {test_name}")

def read_after_write(data):
    with open(device_path, "wb", buffering=0) as f:
        rc = f.write(data)
        if rc != len(data):
            return False
    with open(device_path, "rb", buffering=0) as f:
        read_buf = f.read(device_io_boundary_size);
        return True if read_buf == data else False

def test_single_write():
    success = read_after_write(b"foobarbazlaiwjdilawjdli2ujlajdlajdilwajdlijwaldijai38bdfkauhdak")
    if success:
        log_success("test_single_write")
    else:
        log_fail("test_single_write");

def test_multiple_writes():
    for i in range(100):
        success = read_after_write(f"foobarbazlaiwjdilawjdli2ujlajdlajdilwajdlijwaldijai38bdfkauhdak-{i}".encode());
        if not success:
            log_fail("test_multiple_writes");
            return
    log_success("test_multiple_writes");

def test_boundary():
    extra_size = 100
    data = os.urandom(device_io_boundary_size + extra_size)
    with open(device_path, "wb", buffering=0) as f:
        rc = f.write(data)
        if rc != device_io_boundary_size:
            log_fail("test_boundary")
            return
        rc = f.write(data[device_io_boundary_size:])
        if rc != extra_size:
            log_fail("test_boundary")
            return
    with open(device_path, "rb", buffering=0) as f:
        read_buf = f.read(device_io_boundary_size + extra_size)
        if len(read_buf) != device_io_boundary_size or read_buf != data[:device_io_boundary_size]:
            log_fail("test_boundary")
            return
        read_buf = f.read(device_io_boundary_size + extra_size)
        if len(read_buf) != extra_size or read_buf != data[device_io_boundary_size:]:
            log_fail("test_boundary")
            return
    log_success("test_boundary")

if __name__ == "__main__":
    test_single_write()
    test_multiple_writes()
    test_boundary()
