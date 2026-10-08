/*
 * mm-complete.c - 분리 가용 리스트 + 여러 최적화를 합친 최종 구현
 *
 * ======================================================================
 * [채점 기준 복습]
 *   점수 = 60 * (평균 util) + 40 * min(1, 처리량 / 600Kops)
 *   util = (trace 중 동시에 살아 있던 payload 최대 합) / (최종 힙 크기)
 *   처리량은 이 구현으로도 이미 만점 기준(600Kops)을 크게 넘으므로,
 *   점수를 더 올리는 방법은 "힙 낭비를 줄이는 것" 하나뿐이다.
 *
 * [블록 구조]  (모든 블록 크기는 8의 배수, payload 는 8바이트 정렬)
 *     할당 블록:  | 헤더 4B | payload ...                           |   (푸터 없음)
 *     가용 블록:  | 헤더 4B | pred 4B | succ 4B | (빈 공간) | 푸터 4B |
 *   헤더/푸터 한 워드 = 크기(8의 배수) | bit1: 앞 블록이 할당? | bit0: 이 블록이 할당?
 *
 * [적용한 최적화]
 *  1. 할당 블록은 헤더 4바이트만 사용한다 (푸터 제거).
 *       - "앞 블록이 할당 상태인가"를 내 헤더의 bit1 에 저장한다.
 *       - 푸터는 가용 블록에만 있으므로, 앞 블록이 가용일 때만 PREV_BLKP 를 쓴다.
 *  2. 가용 블록의 pred/succ 를 8바이트 포인터가 아니라 힙 시작 기준 4바이트 오프셋으로 저장.
 *       - 가용 블록 최소 크기가 16바이트(헤더 + pred + succ + 푸터)로 줄어든다.
 *  3. 분리 가용 리스트: 128바이트 이하는 크기별 리스트(정확히 같은 크기),
 *     그 이상은 2배씩 커지는 구간별 리스트. 구간 안에서는 best-fit.
 *  4. 적응형 배치: 최근 요청 크기의 평균보다 확실히 큰 요청은 가용 블록의 "윗쪽"에서,
 *     나머지는 "아랫쪽"에서 잘라낸다.
 *       - 크기가 다른 블록들이 번갈아 할당되고 큰 것만 free 되는 패턴(binary)에서,
 *         큰 블록의 구멍이 서로 이어져 합쳐지므로 다음 큰 요청이 들어간다.
 *       - 임계값을 고정하지 않고 요청 크기의 로그 스케일 이동 평균으로 정하기 때문에
 *         크기 조합이 달라도 동작한다 (고정 임계값은 주어진 trace 에만 맞는 과적합이었다).
 *  5. 힙 확장은 필요한 만큼만: 힙 끝 블록이 가용이면 부족한 만큼만 늘리고,
 *     작은 요청의 확장 단위도 힙 크기에 비례(최대 CHUNKSIZE)시켜 작은 힙의 낭비를 줄인다.
 *  6. realloc: 다음 블록이 가용이거나 이 블록이 힙 끝이면 복사 없이 제자리 확장.
 *     줄일 때는 더 작은 구멍에 딱 맞게 들어가면 옮기고, 아니면 제자리에서 자른다.
 *
 * [제약]
 *  - 힙 크기 4GB 미만 (오프셋과 블록 크기가 32비트). 이 랩의 힙은 20MB 이하.
 *  - 단일 스레드 전용 (락 없음).
 *
 * 테스트:  ./test mm-complete.c        (MM_DEBUG=1 이면 힙/가용 리스트 일관성 검사 포함)
 * ======================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "Ultra hobi",
    /* First member's full name */
    "picky232",
    /* First member's email address */
    "mistick0215@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* ------------------------------------------------------------------------
 * 튜닝 값 (컴파일 옵션 -DNAME=값 으로 바꿔서 실험할 수 있다)
 *   값은 주어진 trace 와 tools/gen_traces.py 로 만든 별도 trace 묶음을 함께 보고 정했다.
 * ---------------------------------------------------------------------- */
#ifndef CHUNKSIZE
#define CHUNKSIZE (1 << 12)  // 작은 요청으로 힙을 늘릴 때 확장 단위의 상한 (바이트)
#endif
#ifndef CHUNK_DIV
#define CHUNK_DIV 4          // 확장 단위 = min(CHUNKSIZE, 현재 힙 크기 / CHUNK_DIV): 작은 힙일수록 조금씩 확장
#endif
#ifndef EXACT_ABOVE
#define EXACT_ABOVE (1 << 12) // 이 크기 이상의 요청은 확장 단위 없이 "필요한 만큼만" 확장
#endif
#ifndef EMA_SHIFT
#define EMA_SHIFT 2          // 요청 크기 이동 평균의 반응 속도: 새 값의 1/2^EMA_SHIFT 를 반영
#endif
#ifndef MARGIN
#define MARGIN 6             // 평균보다 이만큼(1/16 옥타브 단위) 더 커야 "큰 요청"으로 취급
#endif
#define MAX_REQUEST (1u << 30) // 이보다 큰 요청은 실패 처리 (크기 계산 overflow 방지)

/* ------------------------------------------------------------------------
 * 기본 상수와 매크로
 * ---------------------------------------------------------------------- */
#define ALIGNMENT 8                                       // 8바이트 정렬
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)   // 8의 배수로 올림

#define WSIZE 4          // 워드 = 헤더/푸터/오프셋 하나의 크기 (바이트)
#define DSIZE 8          // 더블 워드
#define MIN_BLOCK 16     // 최소 블록 크기: 헤더4 + pred4 + succ4 + 푸터4 (가용일 때 필요)

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* 헤더/푸터 한 워드 = 크기(상위 비트, 8의 배수) | prev_alloc(bit1) | alloc(bit0) */
#define PACK(size, prev_alloc, alloc) ((size) | ((prev_alloc) << 1) | (alloc))

#define GET(p) (*(unsigned int *)(p))              // 주소 p 의 워드 읽기
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // 주소 p 에 워드 쓰기

#define GET_SIZE(p) (GET(p) & ~0x7)                // 크기 필드
#define GET_ALLOC(p) (GET(p) & 0x1)                // 이 블록의 할당 비트
#define GET_PREV_ALLOC(p) ((GET(p) >> 1) & 0x1)    // 이전 블록의 할당 비트

// 헤더 한 워드의 prev_alloc 비트만 바꾼다 (다른 비트는 유지)
#define SET_PREV_ALLOC(hp, v) (PUT((hp), (v) ? (GET(hp) | 0x2) : (GET(hp) & ~0x2)))

/* bp = payload 시작 주소 */
#define HDRP(bp) ((char *)(bp) - WSIZE)                                // 헤더 주소
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)           // 푸터 주소 (가용 블록에서만 유효)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))              // 다음 블록
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))  // 이전 블록 (이전이 가용일 때만 유효)

/* ------------------------------------------------------------------------
 * 가용 리스트 매크로: pred/succ 는 힙 시작 기준 4바이트 오프셋 (0 = NULL)
 * 힙 오프셋 0 은 정렬 패딩 워드라서 실제 블록이 위치할 수 없으므로 NULL 로 써도 안전하다.
 * ---------------------------------------------------------------------- */
#define OFF2P(off) ((off) == 0 ? NULL : (void *)(heap_base + (off)))            // 오프셋 → 포인터
#define P2OFF(p) ((p) == NULL ? 0u : (unsigned int)((char *)(p) - heap_base))   // 포인터 → 오프셋

#define GET_PRED(bp) OFF2P(GET(bp))                    // pred : payload 첫 워드
#define GET_SUCC(bp) OFF2P(GET((char *)(bp) + WSIZE))  // succ : payload 둘째 워드
#define SET_PRED(bp, p) PUT((bp), P2OFF(p))
#define SET_SUCC(bp, p) PUT((char *)(bp) + WSIZE, P2OFF(p))

/* 분리 리스트 구간: 크기 16~128 은 8바이트 단위 15개(구간 안의 모든 블록이 같은 크기), 이후는 2배씩 */
#define EXACT_MAX 128    // 이 크기 이하는 크기별 리스트
#define EXACT_CLASSES 15 // 16,24,...,128 → 15개
#define NLISTS 32        // 전체 리스트 개수 (마지막 구간은 그 위의 모든 크기)

static char *heap_base;        // 힙 시작 주소 (오프셋 계산 기준)
static char *heap_listp;       // 프롤로그 블록의 bp (디버그 검사에서 힙을 훑을 때 사용)
static void *seg_list[NLISTS]; // 구간별 가용 리스트 head (비어 있으면 NULL)
static int size_avg;           // 최근 요청 크기의 이동 평균 (log2 * 16 스케일, -1 = 아직 없음)

/* ------------------------------------------------------------------------
 * 함수 선언
 * ---------------------------------------------------------------------- */
int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);

static void *extend_heap(size_t bytes);
static void *coalesce(void *bp);
static void *place(void *bp, size_t asize, int top);
static void *find_fit(size_t asize);
static size_t adjust_size(size_t size);
static int get_class(size_t size);
static int lg16(size_t x);
static int decide_top(size_t asize);
static void insert_free(void *bp);
static void delete_free(void *bp);
static int expand_inplace(void *ptr, size_t asize);

#ifdef MM_DEBUG
static void check_heap(const char *where);
#define CHECK(where) check_heap(where)
#else
#define CHECK(where) ((void)0)
#endif

/* ------------------------------------------------------------------------
 * mm_init - 힙 초기화: [패딩][프롤로그 헤더][프롤로그 푸터][에필로그 헤더]
 *   첫 가용 블록은 만들지 않는다. 첫 mm_malloc 이 필요한 만큼만 힙을 늘린다.
 * ---------------------------------------------------------------------- */
int mm_init(void)
{
    int i;
    char *p;

    if ((p = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    if (((size_t)p & 0x7) != 0)              // payload 정렬은 힙 시작이 8바이트 정렬이라는 가정에 의존
        return -1;
    heap_base = p;                           // 이후 모든 오프셋의 기준
    PUT(p, 0);                               // 정렬 패딩 (오프셋 0 은 NULL 용으로 비워 둠)
    PUT(p + 1 * WSIZE, PACK(DSIZE, 1, 1));   // 프롤로그 헤더: 크기 8, 할당
    PUT(p + 2 * WSIZE, PACK(DSIZE, 1, 1));   // 프롤로그 푸터
    PUT(p + 3 * WSIZE, PACK(0, 1, 1));       // 에필로그 헤더: 크기 0, 이전(프롤로그)은 할당
    heap_listp = p + 2 * WSIZE;

    for (i = 0; i < NLISTS; i++)             // trace 마다 mm_init 이 다시 불리므로 상태를 모두 초기화
        seg_list[i] = NULL;
    size_avg = -1;
    return 0;
}

/* ------------------------------------------------------------------------
 * 크기 계산 / 구간 분류
 * ---------------------------------------------------------------------- */

// 요청 크기 → 실제 블록 크기: 헤더 4바이트를 더해 8의 배수로 올림, 최소 16
static size_t adjust_size(size_t size)
{
    return MAX((size_t)MIN_BLOCK, ALIGN(size + WSIZE));
}

// 블록 크기 → 리스트 번호. 128 이하는 8바이트 단위 정확 분류, 이후는 2배씩 커지는 구간
static int get_class(size_t size)
{
    int c;
    size_t limit;

    if (size <= EXACT_MAX)
        return (int)(size >> 3) - 2;         // 16→0, 24→1, ..., 128→14
    c = EXACT_CLASSES;                       // 129~256 → 15
    limit = 256;
    while (size > limit && c < NLISTS - 1) { // 257~512 → 16, 513~1024 → 17, ...
        limit <<= 1;
        c++;
    }
    return c;
}

// log2(x) * 16 의 정수 근사 (x >= 16). 정수 부분 + 바로 아래 4비트를 소수 부분으로 사용
static int lg16(size_t x)
{
    int lg = (int)(sizeof(unsigned long) * 8 - 1) - __builtin_clzl(x);
    return lg * 16 + (int)((x >> (lg - 4)) & 15);
}

// 이 요청을 가용 블록의 윗쪽에서 잘라낼지 결정한다 (1 = 윗쪽, 0 = 아랫쪽)
//   최근 요청 크기(로그 스케일)의 이동 평균보다 MARGIN 이상 큰 요청만 윗쪽에 둔다.
//   → 크기가 두 종류 섞여 들어오면 작은 것은 아래, 큰 것은 위에 모인다.
//   → 크기가 한 가지뿐이면 항상 아랫쪽 (보통의 best-fit 과 같다).
static int decide_top(size_t asize)
{
    int g = lg16(asize);
    int top;

    if (size_avg < 0)                        // 첫 요청: 평균을 그 값으로 시작
        size_avg = g;
    top = g > size_avg + MARGIN;
    size_avg += (g - size_avg) / (1 << EMA_SHIFT);
    return top;
}

/* ------------------------------------------------------------------------
 * 가용 리스트 조작 (이중 연결, LIFO)
 * ---------------------------------------------------------------------- */

// 블록 크기에 맞는 구간 리스트의 맨 앞에 삽입
static void insert_free(void *bp)
{
    int c = get_class(GET_SIZE(HDRP(bp)));
    void *head = seg_list[c];

    SET_PRED(bp, NULL);
    SET_SUCC(bp, head);
    if (head != NULL)
        SET_PRED(head, bp);                  // 기존 head 의 pred 가 새 head 를 가리키게
    seg_list[c] = bp;
}

// 리스트에서 제거 (O(1)). 구간은 헤더의 크기로 찾으므로 헤더 크기를 바꾸기 *전에* 호출해야 한다
static void delete_free(void *bp)
{
    void *pred = GET_PRED(bp);
    void *succ = GET_SUCC(bp);

    if (pred != NULL)
        SET_SUCC(pred, succ);
    else
        seg_list[get_class(GET_SIZE(HDRP(bp)))] = succ;   // bp 가 head 였음
    if (succ != NULL)
        SET_PRED(succ, pred);
}

/* ------------------------------------------------------------------------
 * extend_heap - 힙을 bytes 만큼 늘려 새 가용 블록으로 만들고, 앞 블록이 가용이면 합친다
 *   (기존 에필로그 자리가 새 블록의 헤더가 된다. 새 에필로그는 끝에 다시 쓴다)
 * ---------------------------------------------------------------------- */
static void *extend_heap(size_t bytes)
{
    char *bp;
    size_t size = MAX(ALIGN(bytes), (size_t)MIN_BLOCK);   // 가용 블록은 pred/succ 가 들어갈 최소 16바이트 필요
    unsigned int prev_alloc;

    if (size > MAX_REQUEST || (long)(bp = mem_sbrk((int)size)) == -1)
        return NULL;

    prev_alloc = GET_PREV_ALLOC(HDRP(bp));            // 옛 에필로그가 기억하던 "마지막 블록의 할당 여부"
    PUT(HDRP(bp), PACK(size, prev_alloc, 0));         // 새 가용 블록 헤더
    PUT(FTRP(bp), PACK(size, prev_alloc, 0));         // 새 가용 블록 푸터
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 0, 1));          // 새 에필로그 (이전 블록 = 가용)

    return coalesce(bp);
}

/* ------------------------------------------------------------------------
 * coalesce - 가용 블록 bp 를 인접 가용 블록과 합치고 결과를 리스트에 등록
 *   입력: bp 의 헤더/푸터는 이미 "가용"으로 써져 있고 리스트에는 아직 없다.
 *   합쳐지는 이웃은 헤더를 바꾸기 전에 리스트에서 뺀다.
 * ---------------------------------------------------------------------- */
static void *coalesce(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    unsigned int prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    void *next = NEXT_BLKP(bp);
    unsigned int next_alloc = GET_ALLOC(HDRP(next));

    if (!prev_alloc) {                                 // 이전 블록이 가용: 푸터로 위치를 찾아 합침
        void *prev = PREV_BLKP(bp);
        delete_free(prev);
        size += GET_SIZE(HDRP(prev));
        prev_alloc = GET_PREV_ALLOC(HDRP(prev));       // 합쳐진 블록의 prev_alloc 은 이전 블록의 값
        bp = prev;
    }
    if (!next_alloc) {                                 // 다음 블록이 가용: 합침
        delete_free(next);
        size += GET_SIZE(HDRP(next));
    }

    PUT(HDRP(bp), PACK(size, prev_alloc, 0));
    PUT(FTRP(bp), PACK(size, prev_alloc, 0));
    SET_PREV_ALLOC(HDRP(NEXT_BLKP(bp)), 0);            // 뒤따르는 블록에게 "앞이 가용"임을 알림
    insert_free(bp);
    return bp;
}

/* ------------------------------------------------------------------------
 * find_fit - 요청 크기가 속한 구간부터 큰 구간 순으로 탐색.
 *   처음으로 맞는 블록이 나온 구간에서 best-fit (정확히 같은 크기면 바로 반환)
 *   128바이트 이하의 구간은 구간 안의 블록이 모두 같은 크기라 첫 블록이 곧 정답이다.
 * ---------------------------------------------------------------------- */
static void *find_fit(size_t asize)
{
    int c;
    void *bp;
    void *best;

    for (c = get_class(asize); c < NLISTS; c++) {
        best = NULL;
        for (bp = seg_list[c]; bp != NULL; bp = GET_SUCC(bp)) {
            size_t cursize = GET_SIZE(HDRP(bp));
            if (cursize >= asize) {
                if (cursize == asize)
                    return bp;
                if (best == NULL || GET_SIZE(HDRP(best)) > cursize)
                    best = bp;
            }
        }
        if (best != NULL)
            return best;
    }
    return NULL;
}

/* ------------------------------------------------------------------------
 * place - 가용 블록 bp 에 asize 만큼 할당하고, 남는 조각이 최소 블록 이상이면 분할한다.
 *   top == 0 : 아랫쪽(낮은 주소)을 할당, 남은 조각은 위쪽에 둔다
 *   top == 1 : 윗쪽(높은 주소)을 할당, 남은 조각은 아래쪽에 둔다
 *   실제로 할당된 블록의 bp 를 반환한다 (top 분할이면 bp 와 다르다)
 * ---------------------------------------------------------------------- */
static void *place(void *bp, size_t asize, int top)
{
    size_t csize = GET_SIZE(HDRP(bp));
    size_t remain = csize - asize;
    unsigned int prev_alloc = GET_PREV_ALLOC(HDRP(bp));

    delete_free(bp);                                   // 헤더 크기를 바꾸기 전에 리스트에서 제거

    if (remain < MIN_BLOCK) {                          // 분할 불가: 블록 전체 사용
        PUT(HDRP(bp), PACK(csize, prev_alloc, 1));
        SET_PREV_ALLOC(HDRP(NEXT_BLKP(bp)), 1);        // 다음 블록의 "앞이 할당" 비트 갱신
        return bp;
    }

    if (!top) {                                        // 아랫쪽 할당, 위쪽이 가용 조각
        void *rest;

        PUT(HDRP(bp), PACK(asize, prev_alloc, 1));
        rest = NEXT_BLKP(bp);
        PUT(HDRP(rest), PACK(remain, 1, 0));
        PUT(FTRP(rest), PACK(remain, 1, 0));
        insert_free(rest);                             // 조각 뒤 블록의 prev_alloc 은 이미 0 이라 그대로
        return bp;
    } else {                                           // 윗쪽 할당, 아래쪽이 가용 조각
        void *blk;

        PUT(HDRP(bp), PACK(remain, prev_alloc, 0));    // 가용 조각으로 크기를 줄임
        PUT(FTRP(bp), PACK(remain, prev_alloc, 0));
        insert_free(bp);                               // 크기가 바뀌었으니 새 구간에 다시 삽입
        blk = NEXT_BLKP(bp);                           // 윗쪽에 만들어지는 할당 블록
        PUT(HDRP(blk), PACK(asize, 0, 1));             // 바로 앞은 가용 조각
        SET_PREV_ALLOC(HDRP(NEXT_BLKP(blk)), 1);       // 그 뒤 블록은 "앞이 할당"으로 갱신
        return blk;
    }
}

/* ------------------------------------------------------------------------
 * mm_malloc
 *   1) 맞는 가용 블록이 있으면 배치
 *   2) 없으면 힙을 늘린다: 힙 끝 블록이 가용이면 그 크기는 빼고 부족한 만큼만 늘린다.
 *      작은 요청은 한 덩어리에서 위/아래로 나눠 쓸 수 있게 확장 단위(chunk)로 넉넉히,
 *      큰 요청(EXACT_ABOVE 이상)은 필요한 만큼만 늘려서 힙 끝의 낭비를 없앤다.
 * ---------------------------------------------------------------------- */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t need, chunk;
    int top;
    char *bp;
    char *epilogue;

    if (size == 0 || size > MAX_REQUEST)
        return NULL;
    asize = adjust_size(size);
    top = decide_top(asize);

    if ((bp = find_fit(asize)) != NULL) {
        bp = place(bp, asize, top);
        CHECK("malloc");
        return bp;
    }

    need = asize;
    epilogue = (char *)mem_heap_hi() + 1;              // 에필로그 헤더 바로 뒤 = 힙 끝 + 1
    if (!GET_PREV_ALLOC(epilogue - WSIZE))             // 에필로그가 "마지막 블록은 가용"이라고 기억함
        need -= GET_SIZE(epilogue - DSIZE);            // 마지막 가용 블록의 푸터에서 크기를 읽음
    chunk = MIN((size_t)CHUNKSIZE, mem_heapsize() / CHUNK_DIV);   // 힙이 작을수록 작게 확장
    if ((bp = extend_heap(asize >= EXACT_ABOVE ? need : MAX(need, chunk))) == NULL)
        return NULL;
    bp = place(bp, asize, top);
    CHECK("malloc-extend");
    return bp;
}

/* ------------------------------------------------------------------------
 * mm_free - 가용으로 바꾸고 이웃과 합침 (푸터는 가용 블록에만 쓴다)
 * ---------------------------------------------------------------------- */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    unsigned int prev_alloc = GET_PREV_ALLOC(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, prev_alloc, 0));
    PUT(FTRP(ptr), PACK(size, prev_alloc, 0));
    coalesce(ptr);
    CHECK("free");
}

/* ------------------------------------------------------------------------
 * expand_inplace - ptr 블록을 asize 이상으로 제자리에서 늘린다. 성공하면 1, 불가능하면 0
 *   (a) 다음 블록이 가용이고 합치면 충분 → 합쳐서 사용
 *   (b) 다음이 힙 끝(에필로그)이거나, 다음 가용 블록이 힙 끝 블록 → 힙을 부족한 만큼 늘려서 사용
 *   실패했을 때는 힙/리스트 상태를 건드리지 않는다.
 * ---------------------------------------------------------------------- */
static int expand_inplace(void *ptr, size_t asize)
{
    size_t old = GET_SIZE(HDRP(ptr));
    unsigned int prev_alloc = GET_PREV_ALLOC(HDRP(ptr));
    void *next = NEXT_BLKP(ptr);
    size_t total, remain;

    if (GET_SIZE(HDRP(next)) == 0) {                   // (b) ptr 가 힙 끝 블록: 부족분만큼 힙 확장
        if (extend_heap(asize - old) == NULL)
            return 0;
        next = NEXT_BLKP(ptr);                         // 새로 생긴 가용 블록
    } else if (!GET_ALLOC(HDRP(next))) {               // (a) 다음 블록이 가용
        total = old + GET_SIZE(HDRP(next));
        if (total < asize) {
            if (GET_SIZE(HDRP(NEXT_BLKP(next))) != 0)  // 그 뒤가 힙 끝이 아니면 더 못 늘림
                return 0;
            if (extend_heap(asize - total) == NULL)    // 힙 끝 가용 블록이면 부족분만큼 확장하며 합쳐짐
                return 0;
            next = NEXT_BLKP(ptr);
        }
    } else {
        return 0;                                      // 다음 블록이 할당 상태: 제자리 확장 불가
    }

    // 여기서 next 는 가용이고 old + size(next) >= asize
    total = old + GET_SIZE(HDRP(next));
    delete_free(next);
    remain = total - asize;
    if (remain >= MIN_BLOCK) {                         // 남는 조각은 가용 블록으로 돌려줌
        void *rest;

        PUT(HDRP(ptr), PACK(asize, prev_alloc, 1));
        rest = NEXT_BLKP(ptr);
        PUT(HDRP(rest), PACK(remain, 1, 0));
        PUT(FTRP(rest), PACK(remain, 1, 0));
        insert_free(rest);
    } else {                                           // 조각이 너무 작으면 통째로 흡수
        PUT(HDRP(ptr), PACK(total, prev_alloc, 1));
        SET_PREV_ALLOC(HDRP(NEXT_BLKP(ptr)), 1);
    }
    return 1;
}

/* ------------------------------------------------------------------------
 * mm_realloc
 *   - ptr == NULL → malloc, size == 0 → free
 *   - 줄이거나 같을 때: 지금 블록보다 작은 가용 블록에 딱 맞게 들어가면 거기로 옮기고
 *     (작은 구멍을 메우고 큰 블록은 통째로 풀려서 단편화가 줄어든다),
 *     아니면 제자리에서 자른다 (남는 조각이 최소 블록 이상일 때만).
 *   - 늘릴 때: 제자리 확장을 먼저 시도, 안 되면 새로 할당 + 복사 + 해제
 *   - 실패(NULL 반환) 시 원래 블록은 그대로 유효하다.
 * ---------------------------------------------------------------------- */
void *mm_realloc(void *ptr, size_t size)
{
    size_t asize, old;
    void *newptr;

    if (ptr == NULL)
        return mm_malloc(size);
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    if (size > MAX_REQUEST)
        return NULL;

    asize = adjust_size(size);
    old = GET_SIZE(HDRP(ptr));

    if (asize <= old) {                                // 줄이거나 같은 경우
        if (old - asize >= MIN_BLOCK) {                // 남는 조각이 최소 블록 이상일 때만 의미가 있음
            unsigned int prev_alloc = GET_PREV_ALLOC(HDRP(ptr));
            void *fit = find_fit(asize);
            void *rest;

            if (fit != NULL && GET_SIZE(HDRP(fit)) < old) {   // 더 작은 구멍에 들어감 → 옮긴다
                newptr = place(fit, asize, decide_top(asize));
                memcpy(newptr, ptr, size);             // 줄이는 경우라 size 바이트가 옛 payload 안에 있음
                mm_free(ptr);
                CHECK("realloc-shrink-move");
                return newptr;
            }
            PUT(HDRP(ptr), PACK(asize, prev_alloc, 1));       // 제자리에서 자름
            rest = NEXT_BLKP(ptr);
            PUT(HDRP(rest), PACK(old - asize, 1, 0));
            PUT(FTRP(rest), PACK(old - asize, 1, 0));
            coalesce(rest);                            // 뒤가 가용이면 합치고 리스트에 등록
        }
        CHECK("realloc-shrink");
        return ptr;
    }

    if (expand_inplace(ptr, asize)) {                  // 늘리는 경우: 제자리 확장
        CHECK("realloc-inplace");
        return ptr;
    }

    newptr = mm_malloc(size);                          // 불가능하면 새로 할당해서 복사
    if (newptr == NULL)
        return NULL;
    memcpy(newptr, ptr, MIN(old - WSIZE, size));       // 옛 payload 용량(블록 크기 - 헤더)과 새 크기 중 작은 쪽
    mm_free(ptr);
    return newptr;
}

#ifdef MM_DEBUG
/* ------------------------------------------------------------------------
 * check_heap - 일관성 검사 (MM_DEBUG 빌드에서만, 어긋나면 메시지를 찍고 abort)
 *   1) 모든 블록: 8바이트 정렬, 크기 >= 최소, prev_alloc 비트가 실제 앞 블록 상태와 일치
 *   2) 가용 블록: 헤더 == 푸터, 바로 뒤가 가용이 아님(합치기 누락 없음)
 *   3) 가용 리스트: 모두 가용 상태, 올바른 구간, pred/succ 링크 일치
 *   4) 힙에서 센 가용 블록 수 == 리스트들에서 센 가용 블록 수
 * ---------------------------------------------------------------------- */
static void check_heap(const char *where)
{
    char *bp;
    int c;
    unsigned int prev_alloc = 1;                       // 첫 블록의 앞은 프롤로그(할당)
    size_t heap_free = 0, list_free = 0;

    for (bp = NEXT_BLKP(heap_listp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (((size_t)bp & 0x7) != 0) {
            fprintf(stderr, "[%s] misaligned block %p\n", where, (void *)bp);
            abort();
        }
        if (GET_SIZE(HDRP(bp)) < MIN_BLOCK) {
            fprintf(stderr, "[%s] block too small at %p\n", where, (void *)bp);
            abort();
        }
        if (GET_PREV_ALLOC(HDRP(bp)) != prev_alloc) {
            fprintf(stderr, "[%s] prev_alloc bit wrong at %p\n", where, (void *)bp);
            abort();
        }
        if (!GET_ALLOC(HDRP(bp))) {
            heap_free++;
            if (GET(HDRP(bp)) != GET(FTRP(bp))) {
                fprintf(stderr, "[%s] header/footer mismatch at %p\n", where, (void *)bp);
                abort();
            }
            if (!GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
                fprintf(stderr, "[%s] adjacent free blocks at %p\n", where, (void *)bp);
                abort();
            }
        }
        prev_alloc = GET_ALLOC(HDRP(bp));
    }
    if (GET_PREV_ALLOC(HDRP(bp)) != prev_alloc) {      // 에필로그 헤더의 prev_alloc 도 맞아야 함
        fprintf(stderr, "[%s] epilogue prev_alloc wrong\n", where);
        abort();
    }
    if ((char *)bp != (char *)mem_heap_hi() + 1) {     // 에필로그 "bp" 는 힙 끝 + 1 이어야 함
        fprintf(stderr, "[%s] epilogue not at heap end\n", where);
        abort();
    }

    for (c = 0; c < NLISTS; c++) {
        for (bp = seg_list[c]; bp != NULL; bp = GET_SUCC(bp)) {
            list_free++;
            if (GET_ALLOC(HDRP(bp))) {
                fprintf(stderr, "[%s] allocated block in free list %p (class %d)\n", where, (void *)bp, c);
                abort();
            }
            if (get_class(GET_SIZE(HDRP(bp))) != c) {
                fprintf(stderr, "[%s] block %p in wrong class %d\n", where, (void *)bp, c);
                abort();
            }
            if (GET_SUCC(bp) != NULL && GET_PRED(GET_SUCC(bp)) != (void *)bp) {
                fprintf(stderr, "[%s] broken pred/succ link at %p\n", where, (void *)bp);
                abort();
            }
            if (bp == seg_list[c] && GET_PRED(bp) != NULL) {
                fprintf(stderr, "[%s] head has pred at %p\n", where, (void *)bp);
                abort();
            }
        }
    }
    if (heap_free != list_free) {
        fprintf(stderr, "[%s] free count mismatch: heap=%zu lists=%zu\n", where, heap_free, list_free);
        abort();
    }
}
#endif
