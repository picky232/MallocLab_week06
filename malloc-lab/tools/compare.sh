#!/usr/bin/env bash
# compare.sh - 여러 mm 구현/옵션 조합을 같은 trace 묶음에서 돌려 패밀리별 평균 util 을 비교한다
#
# 사용법 (컨테이너 안에서, malloc-lab 폴더에서):
#   tools/compare.sh <trace폴더> <라벨@소스.c@컴파일옵션> ...
#   예) tools/compare.sh traces_gen  explicit@mm-explicit.c@  complete@mm-complete.c@-DTOP_BIG=80
#       tools/compare.sh traces      complete@mm-complete.c@     # 주어진 11개 trace 비교도 가능
#
# 환경변수: VERBOSE=1 이면 trace 별 util 도 stderr 로 출력
# 출력: 패밀리(파일명에서 첫 '-' 앞부분)별 평균 util(%), 전체 평균, 무효(valid=no) trace 수
# 참고: mdriver 는 util 을 정수 %로 출력하므로 소수점 이하는 평균으로만 의미가 있다.
set -u
cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1

DIR="${1:?trace 폴더를 지정하세요}"
shift
[ "$#" -gt 0 ] || { echo "비교할 구현을 하나 이상 지정하세요 (라벨@소스.c@옵션)"; exit 2; }

# 공통 오브젝트 준비 (mdriver.c 등은 한 번만 컴파일)
make mdriver.o memlib.o fsecs.o fcyc.o clock.o ftimer.o >/dev/null 2>&1 || { echo "공통 오브젝트 빌드 실패"; exit 1; }

LABELS=()
for spec in "$@"; do
    IFS='@' read -r label src flags <<<"$spec"
    # shellcheck disable=SC2086
    if ! gcc -O2 -g -I. $flags -o "mdriver-cmp-$label" mdriver.o memlib.o fsecs.o fcyc.o clock.o ftimer.o "$src" 2>/dev/null; then
        echo "빌드 실패: $spec"; exit 1
    fi
    LABELS+=("$label")
done

RESULT=$(mktemp)
for f in "$DIR"/*.rep; do
    base=$(basename "$f" .rep)
    fam=${base%%-*}
    for label in "${LABELS[@]}"; do
        # util 줄: " 0       yes   99%   ..."  또는 " 0        no     -  ..."
        line=$(./"mdriver-cmp-$label" -v -f "$f" 2>&1 | grep -E '^ *0 +(yes|no)' | head -1)
        if echo "$line" | grep -q ' yes '; then
            u=$(echo "$line" | awk '{sub(/%/,"",$3); print $3}')
            printf '%s\t%s\t%s\t%s\n' "$fam" "$label" "$u" ok >>"$RESULT"
            [ "${VERBOSE:-0}" = 1 ] && printf '  %-14s %-12s %s%%\n' "$base" "$label" "$u" >&2
        else
            printf '%s\t%s\t%s\t%s\n' "$fam" "$label" 0 bad >>"$RESULT"
            echo "  ! 무효/크래시: $label  $base" >&2
        fi
    done
done

awk -F'\t' -v labels="${LABELS[*]}" '
    BEGIN { n = split(labels, L, " ") }
    # 패밀리는 첫 등장 순서(= 파일명 알파벳 순)로 출력한다. (mawk 에는 asorti 가 없음)
    { sum[$1, $2] += $3; cnt[$1, $2]++; tot[$2] += $3; tc[$2]++; if ($4 == "bad") bad[$2]++
      if (!($1 in fams)) { fams[$1] = 1; F[++m] = $1 } }
    END {
        printf "%-9s", "family"
        for (i = 1; i <= n; i++) printf " %14s", L[i]
        printf "\n"
        for (j = 1; j <= m; j++) {
            printf "%-9s", F[j]
            for (i = 1; i <= n; i++) printf " %14.1f", sum[F[j], L[i]] / cnt[F[j], L[i]]
            printf "\n"
        }
        printf "%-9s", "ALL"
        for (i = 1; i <= n; i++) printf " %14.2f", tot[L[i]] / tc[L[i]]
        printf "\n%-9s", "invalid"
        for (i = 1; i <= n; i++) printf " %14d", bad[L[i]] + 0
        printf "\n"
    }' "$RESULT"
rm -f "$RESULT"
