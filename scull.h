#ifndef _SCULL_H_
#define _SCULL_H_

#include <asm-generic/ioctl.h> // _IO, _IOR, _IOW, _IOC_DIR, _IOC_TYPE, _IOC_NR, _IOC_SIZE

#define SCULL_IOC_MAGIC                       'j'
#define SCULL_IOC_RESET                       _IO(SCULL_IOC_MAGIC, 0)
#define SCULL_IOC_SET_NUM_QUANTUM             _IOW(SCULL_IOC_MAGIC, 1, int)
#define SCULL_IOC_SET_QUANTUM_SIZE            _IOW(SCULL_IOC_MAGIC, 2, int)
#define SCULL_IOC_TELL_NUM_QUANTUM            _IO(SCULL_IOC_MAGIC, 3)
#define SCULL_IOC_TELL_QUANTUM_SIZE           _IO(SCULL_IOC_MAGIC, 4)
#define SCULL_IOC_GET_NUM_QUANTUM             _IOR(SCULL_IOC_MAGIC, 5, int)
#define SCULL_IOC_GET_QUANTUM_SIZE            _IOR(SCULL_IOC_MAGIC, 6, int)
#define SCULL_IOC_QUERY_NUM_QUANTUM           _IO(SCULL_IOC_MAGIC, 7)
#define SCULL_IOC_QUERY_QUANTUM_SIZE          _IO(SCULL_IOC_MAGIC, 8)
#define SCULL_IOC_SET_AND_GET_NUM_QUANTUM     _IOWR(SCULL_IOC_MAGIC, 9, int)
#define SCULL_IOC_SET_AND_GET_QUANTUM_SIZE    _IOWR(SCULL_IOC_MAGIC, 10, int)
#define SCULL_IOC_TELL_AND_QUERY_NUM_QUANTUM  _IO(SCULL_IOC_MAGIC, 11)
#define SCULL_IOC_TELL_AND_QUERY_QUANTUM_SIZE _IO(SCULL_IOC_MAGIC, 12)
#define SCULL_IOC_IOC_NR_MAX                  12

#define DEFAULT_QUANTUM_SIZE                  4000
#define DEFAULT_NUM_QUANTUM                   100

#endif // _SCULL_H_
