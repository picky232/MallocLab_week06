#!/usr/bin/env python3
"""
gen_traces.py - 일반화 검증용 trace 생성기

주어진 11개 trace 에만 맞춰 튜닝하면 "그 trace 에서만 점수가 잘 나오는" 과적합이 생길 수 있다.
그래서 주어진 trace 와 다른 패턴의 trace 를 여러 종류 만들어서, 튜닝에 쓰지 않은 데이터로 검증한다.

사용법:  python3 tools/gen_traces.py [출력폴더]      (기본: traces_gen)
출력:    <폴더>/<패밀리>-<번호>.rep   (mdriver -f 로 한 개씩 실행 가능, 형식은 traces/README 참고)

패밀리:
  rsmall   작은 크기(1~512B) 랜덤 alloc/free
  rmixed   1B~32KB 로그 균등 분포 랜덤 alloc/free
  rbig     1KB~60KB 랜덤 alloc/free
  binary   (작은 크기, 큰 크기)를 번갈아 할당 → 큰 것만 free → 두 크기의 합으로 재할당 (여러 크기 조합)
  coal     두 블록 할당 → 둘 다 free → 두 배 크기 할당 (여러 크기)
  rgrow    한 블록을 조금씩 realloc 으로 키우면서 작은 블록을 할당/해제 (여러 증가량)
  rrand    무작위 블록을 무작위 크기로 realloc (줄이기/늘리기 모두)
  rshrink  큰 블록을 반복해서 줄이기
  stack    LIFO 순서로 할당/해제
  queue    FIFO 순서로 할당/해제
  phase    크기 X 를 많이 할당 → 절반 free → 다른 크기 Y 할당 (여러 조합)
"""
import os
import random
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else "traces_gen"
os.makedirs(OUT, exist_ok=True)


class Trace:
    """ops 를 모아서 mdriver 형식으로 쓴다. 블록 id 는 0부터 순서대로 부여한다."""

    def __init__(self):
        self.ops = []
        self.nid = 0
        self.live = {}      # id -> 현재 크기

    def alloc(self, size):
        i = self.nid
        self.nid += 1
        self.ops.append(("a", i, size))
        self.live[i] = size
        return i

    def realloc(self, i, size):
        self.ops.append(("r", i, size))
        self.live[i] = size

    def free(self, i):
        self.ops.append(("f", i, None))
        del self.live[i]

    def free_all(self, order=None):
        ids = list(self.live) if order is None else order
        for i in ids:
            if i in self.live:
                self.free(i)

    def write(self, name):
        with open(os.path.join(OUT, name + ".rep"), "w") as f:
            f.write("0\n%d\n%d\n1\n" % (self.nid, len(self.ops)))
            for op, i, size in self.ops:
                f.write("f %d\n" % i if op == "f" else "%s %d %d\n" % (op, i, size))


def log_uniform(rng, lo, hi):
    """lo~hi 사이에서 로그 스케일로 균등하게 뽑는다."""
    import math
    return int(math.exp(rng.uniform(math.log(lo), math.log(hi))))


def random_trace(name, seed, n_ops, size_fn, live_cap_bytes, p_alloc=0.55):
    """랜덤 alloc/free. 살아 있는 총 바이트가 상한을 넘으면 free 만 한다."""
    rng = random.Random(seed)
    t = Trace()
    total = 0
    for _ in range(n_ops):
        if t.live and (total > live_cap_bytes or rng.random() > p_alloc):
            i = rng.choice(list(t.live))
            total -= t.live[i]
            t.free(i)
        else:
            s = size_fn(rng)
            t.alloc(s)
            total += s
    t.free_all()
    t.write(name)


# ---- 랜덤 계열 -------------------------------------------------------------
for k, seed in enumerate((11, 12, 13)):
    random_trace("rsmall-%d" % k, seed, 8000, lambda r: r.randint(1, 512), 4 << 20)
for k, seed in enumerate((21, 22, 23)):
    random_trace("rmixed-%d" % k, seed, 6000, lambda r: log_uniform(r, 1, 32768), 8 << 20)
for k, seed in enumerate((31, 32)):
    random_trace("rbig-%d" % k, seed, 4000, lambda r: r.randint(1000, 60000), 10 << 20)

# ---- binary 계열: (작은, 큰, 반복) ------------------------------------------
for k, (s1, s2, n) in enumerate([(32, 224, 3000), (128, 896, 1500), (24, 232, 3000),
                                 (256, 1792, 600), (16, 48, 5000), (100, 2000, 800),
                                 (8, 56, 6000), (512, 3584, 400)]):
    t = Trace()
    small, big = [], []
    for _ in range(n):
        small.append(t.alloc(s1))
        big.append(t.alloc(s2))
    for i in big:
        t.free(i)
    for _ in range(n):
        t.alloc(s1 + s2)
    t.free_all()
    t.write("binary-%d" % k)

# ---- coalescing 계열 ---------------------------------------------------------
for k, b in enumerate([500, 1500, 3000, 6000, 12000]):
    t = Trace()
    for _ in range(1500):
        x = t.alloc(b)
        y = t.alloc(b)
        t.free(x)
        t.free(y)
        z = t.alloc(2 * b)
        t.free(z)
    t.write("coal-%d" % k)

# ---- realloc 으로 한 블록을 키우는 계열: (시작, 증가량, 반복, 작은 블록 크기) ------
for k, (start, inc, n, small) in enumerate([(64, 32, 4000, 64), (1000, 100, 3000, 48),
                                            (256, 1024, 500, 200), (2000, 7, 6000, 24),
                                            (16, 16, 6000, 100)]):
    t = Trace()
    big = t.alloc(start)
    prev = t.alloc(small)
    size = start
    for _ in range(n):
        size += inc
        t.realloc(big, size)
        cur = t.alloc(small)
        t.free(prev)
        prev = cur
    t.free_all()
    t.write("rgrow-%d" % k)

# ---- 무작위 realloc (줄이기/늘리기 모두) ---------------------------------------
for k, seed in enumerate((41, 42, 43)):
    rng = random.Random(seed)
    t = Trace()
    ids = [t.alloc(log_uniform(rng, 8, 4096)) for _ in range(150)]
    for _ in range(6000):
        r = rng.random()
        if r < 0.5:
            i = rng.choice(list(t.live))
            t.realloc(i, log_uniform(rng, 8, 8192))
        elif r < 0.75 and len(t.live) > 20:
            t.free(rng.choice(list(t.live)))
        else:
            t.alloc(log_uniform(rng, 8, 4096))
    t.free_all()
    t.write("rrand-%d" % k)

# ---- 큰 블록을 반복해서 줄이기 -------------------------------------------------
for k, (start, dec) in enumerate([(100000, 500), (20000, 37), (4000, 3)]):
    t = Trace()
    keep = [t.alloc(48) for _ in range(20)]       # 중간에 끼는 작은 블록
    big = t.alloc(start)
    keep += [t.alloc(48) for _ in range(20)]
    size = start
    while size - dec > 8:
        size -= dec
        t.realloc(big, size)
        if len(keep) < 300:
            keep.append(t.alloc(48))
    t.free_all()
    t.write("rshrink-%d" % k)

# ---- stack / queue ------------------------------------------------------------
for k, seed in enumerate((51, 52)):
    rng = random.Random(seed)
    t = Trace()
    stack = []
    for _ in range(6000):
        if stack and rng.random() < 0.48:
            t.free(stack.pop())
        else:
            stack.append(t.alloc(log_uniform(rng, 8, 8192)))
    while stack:
        t.free(stack.pop())
    t.write("stack-%d" % k)
for k, seed in enumerate((61, 62)):
    rng = random.Random(seed)
    t = Trace()
    queue = []
    for _ in range(6000):
        if len(queue) > 100 and rng.random() < 0.5:
            t.free(queue.pop(0))
        else:
            queue.append(t.alloc(log_uniform(rng, 8, 8192)))
    for i in queue:
        t.free(i)
    t.write("queue-%d" % k)

# ---- phase: X 를 많이 할당 → 절반 free → Y 할당 ---------------------------------
for k, (x, y, n) in enumerate([(100, 150, 4000), (1000, 600, 1500), (50, 400, 4000),
                               (4000, 3000, 600), (300, 300, 3000)]):
    rng = random.Random(70 + k)
    t = Trace()
    ids = [t.alloc(x) for _ in range(n)]
    rng.shuffle(ids)
    for i in ids[: n // 2]:
        t.free(i)
    for _ in range(n // 2):
        t.alloc(y)
    t.free_all()
    t.write("phase-%d" % k)

print("생성 완료: %s (%d개)" % (OUT, len([f for f in os.listdir(OUT) if f.endswith('.rep')])))
