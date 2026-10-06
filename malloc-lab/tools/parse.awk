# parse.awk - mdriver -v 출력을 탭 구분 중간 형식으로 변환
#
# 사용:
#   awk -v names="a.rep,b.rep" [-v base=0] [-v rowsonly=0] -f parse.awk < mdriver출력
#
#   names    : trace 번호(0부터) 순서대로 나열한 trace 이름 (쉼표 구분)
#   base     : 표시할 trace 번호에 더할 값 (trace를 하나씩 따로 돌릴 때 사용)
#   rowsonly : 1이면 TOTAL/PERF/TERM 줄은 내보내지 않음 (개별 실행 결과를 합칠 때)
#
# 출력 형식 (탭 구분):
#   ROW   번호  이름  yes|no  이용률  ops  secs  Kops
#   TOTAL 이용률  ops  secs  Kops
#   PERF  이용률점수  처리량점수  총점
#   TERM  오류수
#   ERR   오류 메시지

BEGIN {
    n = split(names, NAME, ",")
    inres = 0
}

# 점수와 무관한 디버그 출력 무시
/getopt returned/ { next }

# mdriver 는 secs 를 "%10.6f", Kops 를 "%6.0f" 로 찍기 때문에 Kops 가 6자리 이상이면
# 두 숫자 사이에 공백이 없어 "0.000132109008" 처럼 붙는다. 소수점 아래 6자리까지가 secs.
# 인자: secs 필드, kops 필드 → SPLIT_SECS, SPLIT_KOPS 에 나눈 값을 저장
function split_secs_kops(a, b,   p) {
    if (b == "" && a ~ /\.[0-9][0-9][0-9][0-9][0-9][0-9][0-9]/) {
        p = index(a, ".")
        SPLIT_SECS = substr(a, 1, p + 6)
        SPLIT_KOPS = substr(a, p + 7)
    } else {
        SPLIT_SECS = a
        SPLIT_KOPS = b
    }
}

/^Results for mm malloc/ { inres = 1; next }

# trace별 결과 한 줄:  " 0       yes   99%    5694  0.006992   814"
inres && /^ *[0-9]+ +(yes|no) / {
    idx = $1 + 0
    nm = (idx + 1 <= n) ? NAME[idx + 1] : ("trace" idx)
    if ($2 == "yes") {
        u = $3
        sub(/%/, "", u)
        split_secs_kops($5, $6)
        printf "ROW\t%d\t%s\tyes\t%s\t%s\t%s\t%s\n", idx + base, nm, u, $4, SPLIT_SECS, SPLIT_KOPS
    } else {
        printf "ROW\t%d\t%s\tno\t-\t-\t-\t-\n", idx + base, nm
    }
    next
}

# 합계 줄:  "Total          74%  112372  0.574264   196"  (오류가 있으면 "-" 로 나옴)
inres && /^Total/ {
    if (!rowsonly && $2 ~ /%$/) {
        u = $2
        sub(/%/, "", u)
        split_secs_kops($4, $5)
        printf "TOTAL\t%s\t%s\t%s\t%s\n", u, $3, SPLIT_SECS, SPLIT_KOPS
    }
    next
}

# 최종 점수:  "Perf index = 44 (util) + 13 (thru) = 57/100"
/^Perf index/ {
    if (!rowsonly) {
        t = $10
        sub(/\/100/, "", t)
        printf "PERF\t%s\t%s\t%s\n", $4, $7, t
    }
    next
}

# "Terminated with 2 errors"
/^Terminated with/ {
    if (!rowsonly) printf "TERM\t%s\n", $3
    next
}

# "ERROR [trace 3, line 12]: mm_malloc failed."  /  "ERROR: mem_sbrk failed. ..."
/^ERROR/ {
    if ($0 ~ /^ERROR \[trace /) {
        s = $0
        sub(/^ERROR \[trace /, "", s)
        i = s + 0
        nm = (i + 1 <= n) ? NAME[i + 1] : ("trace" i)
        printf "ERR\t[%s] %s\n", nm, $0
    } else {
        printf "ERR\t%s\n", $0
    }
    next
}

# 그 밖의 줄(팀 정보, 안내 문구 등)은 무시
