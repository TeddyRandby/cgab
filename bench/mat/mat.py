from __future__ import print_function

import sys
import time


class TimedContext(object):

    def __enter__(self):
        self.t1 = time.time()

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.t2 = time.time()
        print(self.t2 - self.t1)


class SimpleMatrix(object):

    def __init__(self, N, data):
        self._data = data
        self._N = N

    def __add__(self, other):
        assert(self._N == other._N)
        N = self._N
        data = list(range(N*N))
        for i in range(N*N):
            data[i] = self._data[i] + other._data[i]
        return SimpleMatrix(N, data)

    def __mul__(self, other):
        assert(self._N == other._N)
        N = self._N
        data = list(range(N*N))
        for i in range(N):
            for j in range(N):
                sum = 0
                for k in range(N):
                    sum += self._data[i*N+k] * other._data[k*N+j]
                data[i*N+j] = sum
        return SimpleMatrix(N, data)

    def diag_sum(self):
        N = self._N
        sum = 0
        for i in range(N):
            sum += self._data[i*(N+1)]
        return sum

sum = 0


N = 2


for i in range(2000):
    a = SimpleMatrix(N, [i for i in range(N * N)])
    b = SimpleMatrix(N, [i for i in range(N * N)])

    c = a + b
    d = c * a * b

    sum = d.diag_sum()

print(sum)
