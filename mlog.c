/*

  Copyright (c) 2016 Martin Sustrik

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "iov.h"
#include "msock.h"
#include "dsock.h"
#include "utils.h"

dsock_unique_id(mlog_type);

static void *mlog_hquery(struct hvfs *hvfs, const void *type);
static void mlog_hclose(struct hvfs *hvfs);
static int mlog_msendv(struct msock_vfs *mvfs,
    const struct iovec *iov, size_t iovlen, int64_t deadline);
static ssize_t mlog_mrecvv(struct msock_vfs *mvfs,
    const struct iovec *iov, size_t iovlen, int64_t deadline);

struct mlog_sock {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    /* Underlying socket. */
    int s;
    /* This socket. */
    int h;
};

int mlog_start(int s) {
    /* Check whether underlying socket is a bytestream. */
    if(dsock_slow(!hquery(s, msock_type))) return -1;
    /* Create the object. */
    struct mlog_sock *obj = malloc(sizeof(struct mlog_sock));
    if(dsock_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->hvfs.query = mlog_hquery;
    obj->hvfs.close = mlog_hclose;
    obj->mvfs.msendv = mlog_msendv;
    obj->mvfs.mrecvv = mlog_mrecvv;
    obj->s = s;
    /* Create the handle. */
    int h = hcreate(&obj->hvfs);
    if(dsock_slow(h < 0)) {
        int err = errno;
        free(obj);
        errno = err;
        return -1;
    }
    obj->h = h;
    return h;
}

int mlog_stop(int s) {
    struct mlog_sock *obj = hquery(s, mlog_type);
    if(dsock_slow(!obj)) return -1;
    int u = obj->s;
    free(obj);
    return u;
}

static int mlog_msendv(struct msock_vfs *mvfs,
      const struct iovec *iov, size_t iovlen, int64_t deadline) {
    struct mlog_sock *obj = dsock_cont(mvfs, struct mlog_sock, mvfs);
    size_t len = iov_size(iov, iovlen);
    size_t i, j;
    fprintf(stderr, "handle: %-4d send %8zuB: 0x", obj->h, len);
    for(i = 0; i != iovlen; ++i) {
        for(j = 0; j != iov[i].iov_len; ++j) {
            fprintf(stderr, "%02x", (int)((uint8_t*)iov[i].iov_base)[j]);
        }
    }
    fprintf(stderr, "\n");
    return msendv(obj->s, iov, iovlen, deadline);
}

static ssize_t mlog_mrecvv(struct msock_vfs *mvfs,
      const struct iovec *iov, size_t iovlen, int64_t deadline) {
    struct mlog_sock *obj = dsock_cont(mvfs, struct mlog_sock, mvfs);
    ssize_t sz = mrecvv(obj->s, iov, iovlen, deadline);
    if(dsock_slow(sz < 0)) return -1;
    size_t len = iov_size(iov, iovlen);
    size_t i, j;
    fprintf(stderr, "handle: %-4d recv %8zuB: 0x", obj->h, sz);
    size_t toprint = sz;
    for(i = 0; i != iovlen && toprint; ++i) {
        for(j = 0; j != iov[i].iov_len && toprint; ++j) {
            fprintf(stderr, "%02x", (int)((uint8_t*)iov[i].iov_base)[j]);
            --toprint;
        }
    }
    fprintf(stderr, "\n");
    return sz;
}

static void *mlog_hquery(struct hvfs *hvfs, const void *type) {
    struct mlog_sock *obj = (struct mlog_sock*)hvfs;
    if(type == msock_type) return &obj->mvfs;
    if(type == mlog_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

static void mlog_hclose(struct hvfs *hvfs) {
    struct mlog_sock *obj = (struct mlog_sock*)hvfs;
    int rc = hclose(obj->s);
    dsock_assert(rc == 0);
    free(obj);
}

