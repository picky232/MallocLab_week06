#####################################################################
# CS:APP Malloc Lab
# Handout files for students
#
# Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
# May not be used, modified, or copied without permission.
#
######################################################################

***********
Main Files:
***********

mm.{c,h}	
	Your solution malloc package. mm.c is the file that you
	will be handing in, and is the only file you should modify.

mdriver.c	
	The malloc driver that tests your mm.c file

short{1,2}-bal.rep
	Two tiny tracefiles to help you get started. 

Makefile	
	Builds the driver

**********************************
Other support files for the driver
**********************************

config.h	Configures the malloc lab driver
fsecs.{c,h}	Wrapper function for the different timer packages
clock.{c,h}	Routines for accessing the Pentium and Alpha cycle counters
fcyc.{c,h}	Timer functions based on cycle counters
ftimer.{c,h}	Timer functions based on interval timers and gettimeofday()
memlib.{c,h}	Models the heap and sbrk function

*******************************
Building and running the driver
*******************************
To build the driver, type "make" to the shell.

To run the driver on a tiny test trace:

	unix> mdriver -V -f short1-bal.rep

The -V option prints out helpful tracing and summary information.

To get a list of the driver flags:

	unix> mdriver -h

# CS:APP Malloc Lab - 학생용 배포 파일 안내

> **CS:APP Malloc Lab**\
> **학생용 배포 파일(Handout files for students)**
>
> Copyright (c) 2002, R. Bryant and D. O'Hallaron. All rights reserved.\
> 허가 없이 사용, 수정 또는 복사할 수 없습니다.

------------------------------------------------------------------------

## 주요 파일 (Main Files)

### `mm.{c,h}`

여러분이 구현할 **malloc 패키지**입니다.

-   `mm.c`는 최종적으로 제출할 파일입니다.
-   **수정해야 하는 유일한 파일도 `mm.c`입니다.**

### `mdriver.c`

여러분이 작성한 `mm.c` 파일을 테스트하는 **malloc 드라이버
프로그램**입니다.

### `short{1,2}-bal.rep`

실습을 시작하고 테스트하는 데 도움을 주기 위한 **아주 작은 두 개의 trace
파일**입니다.

즉 다음 두 파일을 의미합니다.

-   `short1-bal.rep`
-   `short2-bal.rep`

### `Makefile`

드라이버 프로그램을 **빌드(build)** 하는 데 사용합니다.

------------------------------------------------------------------------

## 드라이버를 위한 기타 지원 파일 (Other support files for the driver)

### `config.h`

Malloc Lab 드라이버의 설정을 담당합니다.

### `fsecs.{c,h}`

여러 종류의 타이머 패키지를 사용하기 위한 **wrapper 함수**입니다.

### `clock.{c,h}`

Pentium과 Alpha 프로세서의 **cycle counter**에 접근하기 위한 루틴입니다.

### `fcyc.{c,h}`

CPU의 **cycle counter를 기반으로 동작하는 타이머 함수**입니다.

### `ftimer.{c,h}`

**interval timer**와 `gettimeofday()`를 기반으로 동작하는 타이머
함수입니다.

### `memlib.{c,h}`

**힙(heap)과 `sbrk` 함수의 동작을 모델링하는 파일**입니다.

Malloc Lab에서 여러분이 작성하는 메모리 할당기는 이 파일이 제공하는
가상의 힙 환경을 사용하게 됩니다.

------------------------------------------------------------------------

## 드라이버 빌드 및 실행 (Building and running the driver)

드라이버를 빌드하려면 셸(shell)에서 다음과 같이 입력합니다.

``` bash
make
```

작은 테스트용 trace 파일을 이용하여 드라이버를 실행하려면 다음과 같이
입력합니다.

``` bash
mdriver -V -f short1-bal.rep
```

`-V` 옵션은 프로그램의 실행 과정을 추적하는 데 도움이 되는 정보와 **요약
정보**를 출력합니다.

드라이버에서 사용할 수 있는 옵션 목록을 확인하려면 다음과 같이
입력합니다.

``` bash
mdriver -h
```

------------------------------------------------------------------------

## 핵심 정리

이 배포 파일에서 직접 구현하고 수정해야 하는 핵심 파일은 **`mm.c`
하나**입니다.

`mdriver.c`는 작성한 메모리 할당기를 테스트하고, `short1-bal.rep`과
`short2-bal.rep`는 초기 테스트에 사용할 작은 trace 파일입니다.
`memlib.c`와 `memlib.h`는 실제 운영체제의 힙을 직접 사용하는 대신
실습에서 사용할 힙과 `sbrk`의 동작을 모델링합니다.

기본적인 작업 흐름은 다음과 같습니다.

``` text
mm.c 수정
   ↓
make
   ↓
mdriver 실행
   ↓
trace 파일을 이용해 mm.c 테스트
```

초기 테스트 예시는 다음과 같습니다.

``` bash
make
mdriver -V -f short1-bal.rep
```

------------------------------------------------------------------------

## 진행 상황: 명시적 가용 리스트 (mm-explicit.c)

`mm.c`(암시적 리스트, best-fit)는 그대로 두고, 이중 연결 리스트 방식은
별도 파일 `mm-explicit.c`에 구현해서 비교한다.

- 가용 블록 payload에 `pred`/`succ`를 저장하고, `insert_free`(LIFO 삽입) /
  `delete_free`(O(1) 제거) 함수로 리스트를 관리한다.
- `coalesce`는 경계 태그(헤더/푸터)로 인접 가용 블록을 판단하고, 합쳐지는
  이웃은 헤더를 바꾸기 전에 리스트에서 제거한 뒤 결과 블록을 한 번 삽입한다.
- `find_fit`은 힙 전체 대신 가용 리스트만 순회하는 best-fit이다.
- 64비트에서는 pred/succ(16B) + 헤더/푸터(8B)가 필요하므로 최소 블록 크기가
  24바이트(`MINBLOCK`)다.

테스트 (컨테이너 안에서 실행):

``` bash
./test-explicit               # mm-explicit.c 빌드 후 전체 trace 실행
MM_DEBUG=1 ./test-explicit    # 힙/가용 리스트 일관성 검사 포함
```

| 구현 | util | throughput | 총점 |
|---|---|---|---|
| mm.c (암시적 리스트) | 45 | 26 | 71/100 |
| mm-explicit.c (명시적 리스트) | 45 | 40 | 85/100 |

------------------------------------------------------------------------

## 진행 상황: 분리 가용 리스트 (mm-segregated.c)

가용 블록을 크기 구간별 이중 연결 리스트로 나눠 담는 구현이다. `mm.c`는 그대로 둔다.

- `seg_list[SEG_COUNT]` 배열에 구간별 head를 저장한다. class 0은 32바이트 이하,
  이후 2배씩 커진다 (`get_class`).
- 삽입은 해당 구간 리스트의 맨 앞(LIFO)이고, 구간 안은 정렬하지 않는다.
- `find_fit`은 요청 크기가 속한 구간부터 큰 구간 순으로 탐색하고, 처음으로 맞는
  블록이 나온 구간에서 best-fit을 고른다.
- 블록 크기가 바뀌면 속한 구간도 바뀐다. `delete_free`는 헤더의 크기로 구간을 찾기
  때문에 `coalesce`/`place`에서 헤더를 바꾸기 전에 호출한다.

테스트 (컨테이너 안에서 실행):

``` bash
./test                               # mm.c (기본)
./test mm-segregated.c               # mm-segregated.c 로 전체 trace
./test mm-segregated.c short1-bal    # trace 하나만
MM_DEBUG=1 ./test mm-segregated.c    # 구간/링크 일관성 검사 포함 (느림)
```

`./test`는 첫 인자가 `.c` 파일이면 그 소스를 `mdriver-<이름>`으로 따로 빌드해서
테스트하고, 지정하지 않으면 기존처럼 `mm.c`를 쓴다.

| 구현 | util | throughput | 총점 |
|---|---|---|---|
| mm-explicit.c (명시적 리스트, 위 표) | 45 | 40 (약 1263 Kops) | 85/100 |
| mm-segregated.c (분리 가용 리스트) | 45 | 40 (약 2730 Kops) | 85/100 |

처리량은 600 Kops에서 상한이 걸려서 둘 다 만점이고, util은 두 구현이 같다(75%).
`MM_DEBUG=1`로 구간/링크/가용 블록 수 검사를 켠 채 11개 trace가 모두 통과했다.

------------------------------------------------------------------------

## 진행 상황: 최종 구현 (mm-complete.c) — 98/100

```bash
./test mm-complete.c            # 공식 11개 trace, 98/100 (util 58 + 처리량 40)
MM_DEBUG=1 ./test mm-complete.c # 힙/가용 리스트 일관성 검사 포함
```

### 점수를 올린 방법 (util = 최대 동시 payload / 최종 힙 크기)

처리량은 이미 만점이라 점수는 util로만 올릴 수 있다. trace별 손실 원인을 분석해서 해결했다.

| 문제 (trace) | 원인 | 해결 |
|---|---|---|
| 할당 블록마다 8바이트 헤더/푸터 | 오버헤드 | 할당 블록은 헤더 4바이트만 사용(푸터 제거), 이전 블록 할당 여부는 헤더 bit1에 저장 |
| 가용 블록 최소 24바이트 | pred/succ가 8바이트 포인터 | 힙 시작 기준 4바이트 오프셋으로 저장 → 최소 16바이트 |
| coalescing (66% → 100%) | 큰 요청에도 4KB 고정 확장 | 힙 끝 가용 블록을 고려해 부족한 만큼만 확장 |
| binary, binary2 (55%, 51% → 96%, 88%) | 작은/큰 블록이 번갈아 놓여 큰 블록 구멍에 512B가 안 들어감 | 작은 요청은 가용 블록 아랫쪽, 큰 요청은 윗쪽에서 잘라 큰 구멍끼리 합쳐지게 함 |
| realloc, realloc2 (31%, 30% → 100%, 87%) | 매번 새로 할당 + 복사 | 다음 블록이 가용이거나 힙 끝이면 제자리 확장 |

### 일반화 (주어진 trace에만 맞춘 과적합 방지)

점수를 올리다 보면 주어진 11개 trace에만 맞는 값이 섞일 수 있다. 처음에 쓴 "작은 요청 = 72바이트 이하"
같은 고정 임계값이 그랬다. 그래서 튜닝에 쓰지 않는 별도 trace 41개를 만들어 검증했다.

```bash
python3 tools/gen_traces.py traces_gen    # 일반화 검증용 trace 생성 (랜덤/binary 변형/realloc/LIFO/FIFO/단계형 등)
tools/compare.sh traces_gen explicit@mm-explicit.c@ complete@mm-complete.c@   # 구현별 패밀리 평균 util 비교
VERBOSE=1 tools/compare.sh traces_official complete@mm-complete.c@            # trace 별 util (공식 11개)
```

검증으로 드러난 과적합/약점과 수정:

| 발견 | 수정 |
|---|---|
| binary 변형(크기 조합이 다른 경우)에서 분리가 안 됨 (고정 임계값) | 임계값을 요청 크기의 로그 스케일 이동 평균으로 자동 조정 (`decide_top`) |
| 무작위 realloc에서 explicit보다 나쁨 (70% vs 81%) | 줄일 때 더 작은 구멍에 딱 맞게 들어가면 옮기고, 아니면 제자리에서 자름 |
| 작은 trace에서 초기/확장 4KB 낭비 (coal 74%) | 확장 단위를 `min(4KB, 힙 크기/4)`로 힙에 비례하게 함 (coal 99%) |

| 구현 | 공식 11개 util | 시험 41개 util |
|---|---|---|
| mm-explicit.c | 74.6% (점수 85) | 70.5% |
| mm-segregated.c | 74.6% (점수 85) | 70.5% |
| mm-complete.c | **96.3% (점수 98)** | **91.1%** |

시험 trace에서도 큰 폭으로 앞서고, 일관성 검사를 켠 채 공식 11개 + 시험 41개가 모두 통과했다.
약점도 남아 있다: FIFO(queue)와 무작위 realloc(rrand)은 explicit보다 낮다.
작은 블록/큰 블록을 위아래로 나누는 배치가 FIFO처럼 크기가 연속적으로 섞인 패턴에서는 손해이기 때문이다.

### 100점이 아닌 이유

공식 점수 = 0.6 × 평균 util + 40(처리량 만점)이라 98.5점 이상(반올림 99)이 되려면 평균 util이
97.5%가 필요한데, 이 trace들에서는 구조적으로 어렵다.

- binary2(88%): 16바이트 payload가 24바이트 블록이 된다. 헤더 4바이트 + 8바이트 정렬 때문에 블록당 8바이트가 필수라서 이 trace의 상한이 90% 안팎이다.
- realloc2(87%): 처음 블록이 한 번은 이동해야 해서 옛 자리(4KB)가 힙에 남는다. 이후 블록이 제자리 확장하므로 더 줄이려면 미래를 알아야 한다.
- random, random2(95%): 무작위 단편화로 이 정도가 한계에 가깝다.

trace에 맞춘 꼼수(trace 파일을 읽어 미리 배치하는 등)로 100점을 만들 수는 있지만 과제 규칙에 어긋나고
일반화도 안 되므로 하지 않았다.
