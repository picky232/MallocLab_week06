# 📘 Docker + VSCode DevContainer 기반 C 개발 환경 구축 가이드 (MallocLab)

이 문서는 **Windows**와 **macOS** 사용자가 Docker와 VSCode DevContainer 기능을 활용하여 C 개발 및 디버깅 환경을 빠르게 구축할 수 있도록 도와줍니다.

[**주의**] 기존 차수와 다른 점만 확인하시면 4장부터 6장만 확인하시면 됩니다.

---

## 1. Docker란 무엇인가요?

**Docker**는 애플리케이션을 어떤 컴퓨터에서든 **동일한 환경에서 실행**할 수 있게 도와주는 **가상화 플랫폼**입니다.  

Docker는 다음 구성요소로 이루어져 있습니다:

- **Docker Engine**: 컨테이너를 실행하는 핵심 서비스
- **Docker Image**: 컨테이너 생성에 사용되는 템플릿 (레시피 📃)
- **Docker Container**: 이미지를 기반으로 생성된 실제 실행 환경 (요리 🍜)

### ✅ AWS EC2와의 차이점

| 구분 | EC2 같은 VM | Docker 컨테이너 |
|------|-------------|-----------------|
| 실행 단위 | OS 포함 전체 | 애플리케이션 단위 |
| 실행 속도 | 느림 (수십 초 이상) | 매우 빠름 (거의 즉시) |
| 리소스 사용 | 무거움 | 가벼움 |

---

## 2. VSCode DevContainer란 무엇인가요?

**DevContainer**는 VSCode에서 Docker 컨테이너를 **개발 환경**처럼 사용할 수 있게 해주는 기능입니다.

- 코드를 실행하거나 디버깅할 때 **컨테이너 내부 환경에서 동작**
- 팀원 간 **환경 차이 없이 동일한 개발 환경 구성** 가능
- `.devcontainer` 폴더에 정의된 설정을 VSCode가 읽어 자동 구성

---

## 3. Docker Desktop 설치하기

1. Docker 공식 사이트에서 설치 파일 다운로드:  
   👉 [https://www.docker.com/products/docker-desktop](https://www.docker.com/products/docker-desktop)

2. 설치 후 Docker Desktop 실행  
   - Windows: Docker 아이콘이 트레이에 떠야 함  
   - macOS: 상단 메뉴바에 Docker 아이콘 확인

---

## 4. 프로젝트 파일 다운로드 (히스토리 없이)

터미널(CMD, PowerShell, zsh 등)에서 아래 명령어로 프로젝트 폴더만 내려받습니다:

```bash
git clone --depth=1 https://github.com/krafton-jungle/malloc_lab_docker.git
```

- `--depth=1` 옵션은 git commit 히스토리를 생략하고 **최신 파일만 가져옵니다.**

### 📂 다운로드 후 폴더 구조 설명

```
malloc_lab_docker/
├── .devcontainer/
│   ├── devcontainer.json      # VSCode에서 컨테이너 환경 설정
│   └── Dockerfile             # C 개발 환경 이미지 정의
│
├── .vscode/
│   ├── launch.json            # 디버깅 설정 (F5 실행용)
│   └── tasks.json             # 컴파일 자동화 설정
│
├── malloc-lab
│   ├── short1-bal.rep          # 테스트 케이스
│   ├── Makefile                # 과제를 컴파일하고 테스트하기 위한 파일
│   ├── test                    # (추가) 빌드 + 테스트 + 결과 막대그래프 출력 명령 (./test mm-xxx.c 로 구현 선택)
│   ├── mm-explicit.c           # (추가) 명시적 가용 리스트(이중 연결) 구현
│   ├── mm-segregated.c         # (추가) 분리 가용 리스트(segregated fits) 구현
│   ├── mm-complete.c           # (추가) 최종 구현: 분리 리스트 + 푸터 제거 + 적응형 배치 + realloc 제자리 확장 (98/100)
│   ├── test-explicit           # (추가) mm-explicit.c 전용 빌드/실행 스크립트
│   ├── traces_gen/             # (추가) 일반화 검증용 trace 41개 (tools/gen_traces.py 로 생성)
│   ├── traces_official/        # (추가) 공식 11개 trace 심볼릭 링크 (compare.sh 용)
│   ├── tools/                  # (추가) test 명령이 사용하는 보조 스크립트
│   │   ├── parse.awk           #   mdriver 출력 해석
│   │   ├── progress.sh         #   실행 중 진행률 바
│   │   ├── render.awk          #   막대그래프 출력
│   │   ├── gen_traces.py       #   (추가) 일반화 검증용 trace 생성기
│   │   └── compare.sh          #   (추가) 여러 구현을 같은 trace 묶음에서 패밀리별로 비교
│   ├── improvement-plan.md     # (추가) mm.c 개선 계획 (이용률/처리량 단계별)
│   └── README.md               # malloc-lab 과제 설명
│
└── README.md  # 설치 및 사용법 설명 문서
```

---

## 5. VSCode에서 해당 프로젝트 폴더 열기

1. VSCode를 실행
2. `파일 → 폴더 열기`로 방금 클론한 `malloc_lab_docker` 폴더를 선택

---

## 6. 개발 컨테이너: 컨테이너에서 열기

1. VSCode에서 `Ctrl+Shift+P` (Windows/Linux) 또는 `Cmd+Shift+P` (macOS)를 누릅니다.
2. 명령어 팔레트에서 `Dev Containers: Reopen in Container`를 선택합니다.
3. 이후 컨테이너가 자동으로 실행되고 빌드됩니다. 처음 컨테이너를 열면 빌드하는 시간이 오래걸릴 수 있습니다. 빌드 후, 프로젝트가 **컨테이너 안에서 실행됨**.

---

## 7. C 파일에 브레이크포인트 설정 후 디버깅 (F5)

이제 본격적으로 문제를 풀 시간입니다. `malloc-lab/README.md` 파일을 참조하셔서 rbtree 문제를 풀어보세요.

C 언어로 문제를 풀다가 디버깅이 필요하시면 소스코드에 BreakPoint를 설정한 뒤에 키보드에서 `F5`를 눌러 디버깅을 시작할 수 있습니다.`F5`를 누르면 `malloc-lab`폴더에서 `mdriver -V -f short1-bal.rep` 를 실행하여 테스트 코드를 디버깅 모드로 실행합니다.
- 참고로 변수, 메모리, 스택, 출력 등을 VSCode에서 확인할 수도 있습니다.

### 7-1. `./test` 로 결과를 그래프로 보기 (추가한 기능)

`mdriver -v` 의 표 출력은 보기 불편해서, 결과를 **막대그래프로 보여 주는 `test` 명령**을 추가했습니다.
`mdriver.c`, `mm.c`, `Makefile` 등 기존 파일은 수정하지 않았습니다.

```bash
cd malloc-lab
./test                  # 기본 trace 11개 전체 (make 도 자동으로 실행)
./test short1-bal       # trace 하나 (.rep 는 생략 가능, 현재 폴더와 traces/ 에서 찾음)
./test short1-bal amptjp-bal   # 여러 개를 각각 실행
./test -h               # 도움말
```

**mm 구현 파일을 골라서 테스트하기**: 첫 번째 인자가 `.c` 파일(또는 `mm` 으로 시작하는 이름)이면 `mm.c` 대신 그 소스로 빌드해서 테스트합니다. 아무것도 지정하지 않은 `./test` 는 지금처럼 `mm.c` 를 실행합니다. 뒤에 trace 이름을 붙이면 그 trace 만 실행합니다.

```bash
./test                              # mm.c (기본)
./test mm-segregated.c              # 분리 가용 리스트 구현, 기본 trace 전체
./test mm-segregated                # .c 는 생략 가능 (mm 으로 시작하는 이름만)
./test mm-explicit.c short1-bal     # 명시적 가용 리스트 구현, trace 하나만
MM_DEBUG=1 ./test mm-segregated.c   # 힙/가용 리스트 일관성 검사 켜고 실행 (느림)
```

지정한 소스는 `mdriver-<이름>` (예: `mdriver-mm-segregated`)으로 따로 빌드하므로 `mm.c` 와 `mdriver` 는 건드리지 않습니다.

> 쉘에는 `test` 라는 내장 명령이 이미 있어서 `test short1-bal` 로 치면 동작하지 않습니다. 반드시 **`./test`** 로 실행하세요.

출력 예 (이용률 막대: 80% 이상 초록, 50% 이상 노랑, 그 미만 빨강):

```
  #  trace               util                          ops       secs    Kops
  0  amptjp-bal.rep      ━━━━━━━━━━━━━━━━━━━━  99%     5694   0.005641    1009  ✔
  9  realloc-bal.rep     ━━━━━───────────────  27%    14401   0.049364     292  ⚠ 이용률 낮음

  평균 이용률  ━━━━━━━━━━━━━━━───── 74%
  처리량       ━━━━━━────────────── 165 Kops  (만점 기준 600 Kops 의 27%)
  점수         이용률 44 + 처리량 11 = 55 / 100   ━━━━━━━━━━━─────────
```

- 막대는 기본적으로 `━`(채움) / `─`(빈 부분)로 그립니다. 칸 전체를 채우는 `█` 는 터미널(특히 VSCode)에서 위아래 줄의 막대가 서로 붙어 하나의 덩어리처럼 보여서 기본값에서 뺐습니다.
- 예전 모양(`█ ░`)을 쓰려면 `MM_BAR=block ./test` 로 실행하세요.

- 터미널에서 실행하면 trace를 하나씩 돌리는 동안 **진행률 바**(`테스트 중 ━━━───  6/11  random2-bal.rep`)가 같은 줄에서 갱신되고, 끝나면 `테스트 완료 ━━━━ 11/11` 줄로 바뀌어 남은 채 그 아래에 결과가 나옵니다. 진행률을 보려고 trace를 하나씩 먼저 돌린 다음, 점수는 `mdriver` 전체 실행 결과를 그대로 쓰기 때문에 전체 실행 시간은 약 2배가 됩니다. 크래시한 trace가 있으면 진행률 줄에 `✘ 크래시 N` 이 표시됩니다.
- 파이프나 파일로 출력하거나 `NO_COLOR=1` 이면 진행률은 나오지 않습니다. 터미널에서도 끄려면 `MM_PROGRESS=0 ./test` 로 실행하세요.
- **정확성 오류**가 있으면 어떤 trace의 몇 번째 줄에서 났는지 `오류` 항목으로 보여 줍니다.
- **세그폴트**로 `mdriver` 가 죽으면 trace를 하나씩 따로 실행해서 크래시한 trace를 표시하고, `gdb` 로 확인하는 방법을 안내합니다.
- 종료 코드는 모두 통과하면 `0`, 오류/크래시가 있으면 `1`, 없는 파일을 지정하면 `2` 입니다.
- 색상은 터미널에서만 나오며 `NO_COLOR=1 ./test` 로 끌 수 있습니다.

---

## 8. 새로운 Git 리포지토리에 Commit & Push 하기

금주 프로젝트를 개인 Git 리포와 같은 다른 리포지토리에 업로드하려면, 기존 Git 연결을 제거하고 새롭게 초기화해야 합니다.

### ✅ 완전히 새로운 Git 리포로 업로드하는 방법

아래 명령어를 순서대로 실행하세요:

```bash
rm -rf .git
git init
git remote add origin https://github.com/myusername/my-new-repo.git
git add .
git commit -m "Clean start"
git push -u origin main
```

### 📌 설명

- `rm -rf .git`: 기존 Git 기록과 연결을 완전히 삭제합니다.
- `git init`: 현재 폴더를 새로운 Git 리포지토리로 초기화합니다.
- `git remote add origin ...`: 새로운 리포지토리 주소를 origin으로 등록합니다.
- `git add .` 및 `git commit`: 모든 파일을 커밋합니다.
- `git push`: 새로운 리포에 최초 업로드(Push)합니다.

이 과정을 거치면 기존 리포와의 연결은 완전히 제거되고, **새로운 독립적인 프로젝트로 관리**할 수 있습니다.

## 🎉 끝

이제 Docker와 DevContainer를 활용한 C 개발 환경이 완성되었습니다.

- (주의) 위 내용은 처음 설치하는 사람을 기준으로 작성된 내용입니다. malloc-lab 폴더에서 있는 프로젝트를 반복적으로 개발할 경우 5에서 7장의 내용만 반복하시면 됩니다.
- 어떤 운영체제에서든 동일한 환경으로 개발 가능  
- VSCode 내에서 코드 작성, 컴파일, 디버깅까지 한 번에 가능

---
