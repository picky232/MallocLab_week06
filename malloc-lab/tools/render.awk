# render.awk - parse.awk 가 만든 중간 형식을 터미널 막대그래프로 출력
#
# 사용:  awk -v color=1 -f render.awk
#   color : 1이면 ANSI 색상 사용
#
# 입력 줄 종류: HEAD / NOTE / ROW / TOTAL / PERF / TERM / ERR  (탭 구분)
# 종료 코드: 오류/실패/크래시가 하나라도 있으면 1, 아니면 0

BEGIN {
    FS = "\t"
    if (color == 1) {
        GRN = "\033[32m"; YLW = "\033[33m"; RED = "\033[31m"
        DIM = "\033[2m";  BLD = "\033[1m";  RST = "\033[0m"
    }
    # 막대 모양 (style 변수, 기본 line)
    #   line  : ━ ─  칸 높이를 다 채우지 않아서 위아래 줄이 붙어 보이지 않음 (기본)
    #   block : █ ░  칸 전체를 채움. 터미널에 따라 위아래 줄이 하나로 붙어 보일 수 있음
    if (style == "block") { FULL = "█"; EMPTY = "░" }
    else                  { FULL = "━"; EMPTY = "─" }
    W = 20          # 막대 칸 수
    LIBC = 600      # config.h 의 AVG_LIBC_THRUPUT (600 Kops/sec) - 처리량 만점 기준
    MAXERR = 6      # 화면에 보여 줄 오류 메시지 최대 개수
}

$1 == "HEAD"  { head = $2; next }
$1 == "NOTE"  { note[++nnote] = $2; next }
$1 == "ROW"   {
    nrow++
    r_idx[nrow] = $2; r_name[nrow] = $3; r_valid[nrow] = $4
    r_util[nrow] = $5; r_ops[nrow] = $6; r_secs[nrow] = $7; r_kops[nrow] = $8
    if ($4 == "crash") ncrash++
    else if ($4 == "no") nbad++
    next
}
$1 == "TOTAL" { have_total = 1; t_util = $2; t_ops = $3; t_secs = $4; t_kops = $5; next }
$1 == "PERF"  { have_perf = 1; p_u = $2; p_t = $3; p_s = $4; next }
$1 == "TERM"  { term = $2; next }
$1 == "ERR"   { if (!($2 in seen)) { seen[$2] = 1; err[++nerr] = $2 } next }

# ---------- 보조 함수 ----------
function pad(s, w,   k)  { k = w - length(s); while (k-- > 0) s = s " "; return s }
function lpad(s, w,  k)  { k = w - length(s); while (k-- > 0) s = " " s; return s }

# 값에 따른 색: 80 이상 초록, 50 이상 노랑, 그 미만 빨강
function col(p) { return (p >= 80) ? GRN : (p >= 50) ? YLW : RED }

# p(0~100)%를 w칸 막대로: 채운 부분은 값에 따른 색, 빈 부분은 흐리게
function bar(p, w,   f, i, a, b) {
    f = int(p / 100 * w + 0.5)
    if (f > w) f = w
    if (f < 0) f = 0
    a = ""; b = ""
    for (i = 0; i < f; i++) a = a FULL
    for (; i < w; i++) b = b EMPTY
    return col(p) a RST DIM b RST
}

function rep(ch, n,   s) { s = ""; while (n-- > 0) s = s ch; return s }

END {
    nw = 5
    for (i = 1; i <= nrow; i++) if (length(r_name[i]) > nw) nw = length(r_name[i])

    print ""
    print BLD " " head RST
    print DIM " " rep("─", nw + W + 44) RST
    for (i = 1; i <= nnote; i++) print YLW " ! " note[i] RST
    print ""

    # 표 머리글
    print DIM " " lpad("#", 2) "  " pad("trace", nw) "  " pad("util", W + 5) "  " lpad("ops", 6) "  " lpad("secs", 9) "  " lpad("Kops", 6) RST

    # trace별 한 줄
    for (i = 1; i <= nrow; i++) {
        head_part = " " lpad(r_idx[i], 2) "  " pad(r_name[i], nw) "  "
        if (r_valid[i] == "yes") {
            p = r_util[i] + 0
            mid = bar(p, W) " " lpad(r_util[i], 3) "% "
            st = (p < 50) ? YLW "⚠ 이용률 낮음" RST : GRN "✔" RST
            tail = "  " lpad(r_ops[i], 6) "  " lpad(r_secs[i], 9) "  " lpad(r_kops[i], 6) "  " st
        } else {
            mid = bar(0, W) "   - "
            tail = "  " lpad("-", 6) "  " lpad("-", 9) "  " lpad("-", 6) "  "
            tail = tail RED ((r_valid[i] == "crash") ? "✘ 크래시(세그폴트)" : "✘ 정확성 검사 실패") RST
        }
        print head_part mid tail
    }

    # ---------- 요약 ----------
    print ""
    if (have_total) {
        p = t_util + 0
        print "  평균 이용률  " bar(p, W) " " t_util "%"
        k = t_kops + 0
        pk = k / LIBC * 100
        if (pk > 100) pk = 100
        print "  처리량       " bar(pk, W) " " t_kops " Kops  " DIM "(만점 기준 " LIBC " Kops 의 " int(pk) "%)" RST
    }
    if (have_perf) {
        s = p_s + 0
        print "  점수         이용률 " p_u " + 처리량 " p_t " = " BLD p_s " / 100" RST "   " bar(s, W)
    } else if (ncrash > 0) {
        print "  점수         " RED "계산 불가 - 크래시한 trace가 있음" RST
    } else if (nerr > 0 || term > 0 || nbad > 0) {
        print "  점수         " RED "0점 - 오류가 있으면 mdriver 는 점수를 계산하지 않음" RST
    }

    # ---------- 오류 메시지 ----------
    if (nerr > 0) {
        print ""
        print RED BLD "  오류" RST
        for (i = 1; i <= nerr && i <= MAXERR; i++) print RED "   - " err[i] RST
        if (nerr > MAXERR) print DIM "   … 외 " (nerr - MAXERR) "건" RST
    }

    # ---------- 크래시 디버깅 안내 ----------
    # 첫 번째 크래시 trace 하나만 안내 (전부 같은 원인인 경우가 많아 반복하지 않음)
    if (ncrash > 0) {
        for (i = 1; i <= nrow; i++) if (r_valid[i] == "crash") { first = i; break }
        print ""
        print YLW "  디버깅 방법 - 가장 먼저 크래시한 trace 하나로 확인" RST
        print DIM "    gdb --args ./mdriver -v -f traces/" r_name[first] RST
        print DIM "      (gdb) run   →   (gdb) bt   →   (gdb) frame 0   →   (gdb) info locals" RST
        if (ncrash > 1) print DIM "    (크래시한 trace는 모두 " ncrash "개)" RST
    }
    print ""

    exit ((ncrash > 0 || nerr > 0 || nbad > 0 || term > 0) ? 1 : 0)
}
