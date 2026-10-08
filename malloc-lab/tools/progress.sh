# progress.sh - ./test 가 trace 를 하나씩 돌리는 동안 진행률 바를 표시 (source 해서 사용)
#
# 사용:
#   PROGRESS_ENABLE=1   진행률을 켤 때 (터미널일 때만 test 가 1로 설정)
#   progress_init N     진행률 시작 (전체 trace 개수 N)
#   progress_step 이름  trace 하나를 시작할 때마다 호출 (바를 같은 줄에서 갱신)
#   progress_bad        방금 trace 가 크래시했을 때 호출 (표시에 개수 반영)
#   progress_end        진행률 줄을 "테스트 완료" 줄로 바꿔 남김 (결과는 그 아래에 출력됨)
#
# 진행률은 stderr 로 출력한다 (stdout 은 결과 그래프 파이프로 쓰이기 때문).
# PROGRESS_ENABLE 이 1이 아니면 모든 함수가 아무 것도 하지 않는다.

PROGRESS_ENABLE=${PROGRESS_ENABLE:-0}
PROG_ON=0
PROG_N=0
PROG_I=0
PROG_BAD=0
PROG_BAR=""

progress_init() {
    [ "$PROGRESS_ENABLE" = 1 ] || return 0
    PROG_ON=1
    PROG_N=$1
    PROG_I=0
    PROG_BAD=0
}

# 막대 문자열 만들기: 인자 채울 칸 수 → PROG_BAR
progress_make_bar() {
    local filled=$1 w=20 k=0 full="━" none="─"
    [ "${MM_BAR:-line}" = "block" ] && { full="█"; none="░"; }
    PROG_BAR=""
    while [ "$k" -lt "$w" ]; do
        if [ "$k" -lt "$filled" ]; then PROG_BAR="$PROG_BAR$full"; else PROG_BAR="$PROG_BAR$none"; fi
        k=$((k + 1))
    done
}

progress_step() {
    [ "$PROG_ON" = 1 ] || return 0
    local name="$1" bad=""
    PROG_I=$((PROG_I + 1))
    progress_make_bar $((PROG_I * 20 / PROG_N))
    [ "$PROG_BAD" -gt 0 ] && bad="  ✘ 크래시 $PROG_BAD"
    printf '\r\033[K  \033[2m테스트 중\033[0m  %s  %d/%d  %s%s' "$PROG_BAR" "$PROG_I" "$PROG_N" "$name" "$bad" >&2
}

progress_bad() {
    [ "$PROG_ON" = 1 ] || return 0
    PROG_BAD=$((PROG_BAD + 1))
}

# 진행률 줄을 지우지 않고 "테스트 완료" 줄로 바꿔서 남긴다 (결과는 그 아래에 출력됨)
progress_end() {
    [ "$PROG_ON" = 1 ] || return 0
    local bad=""
    progress_make_bar $((PROG_I * 20 / PROG_N))
    [ "$PROG_BAD" -gt 0 ] && bad="  ✘ 크래시 $PROG_BAD"
    printf '\r\033[K  \033[2m테스트 완료\033[0m  %s  %d/%d%s\n' "$PROG_BAR" "$PROG_I" "$PROG_N" "$bad" >&2
    PROG_ON=0
}
