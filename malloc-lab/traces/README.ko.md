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

여러분이 작성한 `mm.c` 파일을 테스트하는 **malloc 드라이버 프로그램**입니다.

### `short{1,2}-bal.rep`

실습을 시작하고 테스트하는 데 도움을 주기 위한 **아주 작은 두 개의 trace 파일**입니다.

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

**interval timer**와 `gettimeofday()`를 기반으로 동작하는 타이머 함수입니다.

### `memlib.{c,h}`

**힙(heap)과 `sbrk` 함수의 동작을 모델링하는 파일**입니다.

Malloc Lab에서 여러분이 작성하는 메모리 할당기는 이 파일이 제공하는 가상의 힙 환경을 사용하게 됩니다.

------------------------------------------------------------------------

## 드라이버 빌드 및 실행 (Building and running the driver)

드라이버를 빌드하려면 셸(shell)에서 다음과 같이 입력합니다.

``` bash
make
```

작은 테스트용 trace 파일을 이용하여 드라이버를 실행하려면 다음과 같이 입력합니다.

``` bash
mdriver -V -f short1-bal.rep
```

`-V` 옵션은 프로그램의 실행 과정을 추적하는 데 도움이 되는 정보와 **요약 정보**를 출력합니다.

드라이버에서 사용할 수 있는 옵션 목록을 확인하려면 다음과 같이 입력합니다.

``` bash
mdriver -h
```

------------------------------------------------------------------------

## 핵심 정리

이 배포 파일에서 직접 구현하고 수정해야 하는 핵심 파일은 **`mm.c` 하나**입니다.

`mdriver.c`는 작성한 메모리 할당기를 테스트하고, `short1-bal.rep`과 `short2-bal.rep`는 초기 테스트에 사용할 작은 trace 파일입니다. `memlib.c`와 `memlib.h`는 실제 운영체제의 힙을 직접 사용하는 대신 실습에서 사용할 힙과 `sbrk`의 동작을 모델링합니다.

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
