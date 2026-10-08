/*
 * mm-segregated.c - 분리 가용 리스트(segregated fits) 기반 malloc 패키지
 *
 * ======================================================================
 * [개념]
 *   명시적 리스트(mm-explicit.c)는 가용 블록 전체를 리스트 하나에 담아서 best-fit 을 하려면
 *   모든 가용 블록을 훑어야 한다. 분리 리스트는 가용 블록을 "크기 구간별"로 여러 리스트에 나눠 담아서
 *   요청 크기에 맞는 구간부터만 탐색한다. 작은 구간의 블록들은 아예 보지 않는다.
 *
 * [구조]
 *   seg_list[c] = 구간 c 의 이중 연결 리스트 head
 *     class 0 : ~32바이트
 *     class 1 : 33~64
 *     class 2 : 65~128
 *     class 3 : 129~256 ... (2배씩 커짐, 마지막 구간은 그 이상 전부)
 *   블록 구조는 mm-explicit.c 와 같다 (헤더 | payload | 푸터, 가용 블록은 payload 에 pred/succ 저장).
 *
 * [정책]
 *   - 삽입: 블록 크기에 맞는 구간 리스트의 맨 앞 (LIFO). 구간 안은 정렬하지 않는다.
 *   - 탐색: 요청 크기가 속한 구간부터 큰 구간 순으로, 구간 안은 best-fit
 *   - 중요한 규칙: 블록 크기가 바뀌면 속한 구간(리스트)도 바뀐다.
 *     delete_free 는 헤더의 크기로 구간을 찾으므로 반드시 헤더 크기를 바꾸기 *전에* 호출해야 한다.
 *     (coalesce 와 place 가 이 순서를 지킨다)
 *
 * mm.c 는 건드리지 않고 별도로 테스트한다:  ./test mm-segregated.c
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
 * 기본 상수와 매크로 (mm-explicit.c 와 동일)
 * ---------------------------------------------------------------------- */
#define ALIGNMENT 8                                       // 8바이트 정렬
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)   // 8의 배수로 올림

#define WSIZE 4             // 워드 = 헤더/푸터 하나의 크기 (바이트)
#define DSIZE 8             // 더블 워드
#define CHUNKSIZE (1 << 12) // 힙을 늘릴 때의 기본 단위 (4KB)
#define MAX(x, y) ((x) > (y) ? (x) : (y))

// 최소 블록 크기: 헤더 + 푸터 + pred + succ (64비트면 24, 32비트면 16)
#define MINBLOCK ALIGN(2 * WSIZE + 2 * sizeof(void *))

#define PACK(size, alloc) ((size) | (alloc))              // 크기와 할당 비트를 한 워드로 합침

#define GET(p) (*(unsigned int *)(p))                     // 주소 p 의 워드 읽기
#define PUT(p, val) (*(unsigned int *)(p) = (val))        // 주소 p 에 워드 쓰기

#define GET_SIZE(p) (GET(p) & ~0x7)                       // 크기 필드
#define GET_ALLOC(p) (GET(p) & 0x1)                       // 할당 비트

/* bp = payload 시작 주소 */
#define HDRP(bp) ((char *)(bp) - WSIZE)                        // 헤더 주소
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)   // 푸터 주소

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) // 힙에서 바로 다음 블록
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // 힙에서 바로 이전 블록

// 가용 블록 payload 에 저장된 리스트 포인터. 읽기/쓰기 둘 다 가능 (PRED(bp) = x;)
#define PRED(bp) (*(void **)(bp))                              // 같은 구간 리스트에서 이전 가용 블록
#define SUCC(bp) (*(void **)((char *)(bp) + sizeof(void *)))   // 같은 구간 리스트에서 다음 가용 블록

#define SEG_COUNT 20      // 구간 개수. 마지막 구간은 (32 * 2^18)바이트 초과 전부
#define SEG_BASE 32       // class 0 의 상한

static char *heap_listp;              // 프롤로그 블록의 bp
static void *seg_list[SEG_COUNT];     // 구간별 가용 리스트 head (비어 있으면 NULL)

/* ------------------------------------------------------------------------
 * 함수 선언
 * ---------------------------------------------------------------------- */
int mm_init(void);
static void *extend_heap(size_t words);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static int get_class(size_t size);
static void insert_free(void *bp);
static void delete_free(void *bp);

#ifdef MM_DEBUG
static void check_heap(const char *where);
#define CHECK(where) check_heap(where)
#else
#define CHECK(where) ((void)0)
#endif

/* ------------------------------------------------------------------------
 * mm_init - 힙 초기화: [패딩][프롤로그 헤더][프롤로그 푸터][에필로그 헤더] + 첫 가용 블록
 * ---------------------------------------------------------------------- */
int mm_init(void)
{
    int i;

    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                            // 정렬 패딩
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // 프롤로그 헤더 (할당 상태: 합치기가 힙 앞으로 넘어가지 않게 함)
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // 프롤로그 푸터
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     // 에필로그 헤더 (크기 0, 할당 상태: 힙 끝 표시)
    heap_listp += (2 * WSIZE);
    for (i = 0; i < SEG_COUNT; i++) // 트레이스마다 mm_init이 다시 불리므로 리스트도 초기화
        seg_list[i] = NULL;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/* ------------------------------------------------------------------------
 * extend_heap - 힙을 words 워드만큼 늘려 새 가용 블록으로 만든다 (리스트 등록은 coalesce 가 담당)
 * ---------------------------------------------------------------------- */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // 8바이트 정렬을 위해 짝수 워드로 맞춤
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));         // 가용 블록 헤더 (기존 에필로그 자리)
    PUT(FTRP(bp), PACK(size, 0));         // 가용 블록 푸터
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 새 에필로그

    return coalesce(bp);                  // 이전 블록이 가용이면 합치고 리스트에 등록
}

/* ------------------------------------------------------------------------
 * get_class - 블록 크기가 속한 구간 번호. class c 는 (SEG_BASE*2^(c-1), SEG_BASE*2^c] 범위
 *   예) 24→0, 40→1, 100→2, 200→3 ...
 * ---------------------------------------------------------------------- */
static int get_class(size_t size)
{
    int c = 0;
    size_t limit = SEG_BASE;

    while (size > limit && c < SEG_COUNT - 1) {   // 상한을 2배씩 키우며 구간 찾기
        limit <<= 1;
        c++;
    }
    return c;
}

/* ------------------------------------------------------------------------
 * insert_free - 블록 크기에 맞는 구간 리스트의 맨 앞에 삽입 (LIFO)
 *   mm-explicit.c 와 달리 head 가 구간마다 따로 있다 (seg_list[c]).
 * ---------------------------------------------------------------------- */
static void insert_free(void *bp)
{
    int c = get_class(GET_SIZE(HDRP(bp)));   // 이 블록이 속할 구간

    PRED(bp) = NULL;                         // 새 head 는 앞이 없음
    SUCC(bp) = seg_list[c];                  // 새 head 의 다음은 원래 head
    if (seg_list[c] != NULL)
        PRED(seg_list[c]) = bp;              // 원래 head 의 이전은 새 head
    seg_list[c] = bp;
}

/* ------------------------------------------------------------------------
 * delete_free - 구간 리스트에서 제거. head 였으면 구간 head 를 갱신
 *   구간은 헤더의 크기로 찾으므로, 헤더 크기를 바꾸기 *전에* 호출해야 한다.
 * ---------------------------------------------------------------------- */
static void delete_free(void *bp)
{
    if (PRED(bp) != NULL)
        SUCC(PRED(bp)) = SUCC(bp);                         // 앞 블록의 succ 가 내 다음을 가리키게
    else
        seg_list[get_class(GET_SIZE(HDRP(bp)))] = SUCC(bp); // bp 가 head 였으므로 구간 head 를 다음 블록으로
    if (SUCC(bp) != NULL)
        PRED(SUCC(bp)) = PRED(bp);                         // 다음 블록의 pred 가 내 이전을 가리키게
}

/* ------------------------------------------------------------------------
 * coalesce - 인접 가용 블록과 합치고 결과 블록을 (합친 크기의) 구간 리스트에 등록
 *   이웃 블록 delete 는 헤더/푸터를 바꾸기 전에 해야 한다 (구간을 헤더 크기로 찾기 때문).
 *   합쳐진 블록은 크기가 커졌으니 다른 구간에 들어갈 수 있다.
 * ---------------------------------------------------------------------- */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록의 푸터로 할당 여부 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록의 헤더로 할당 여부 확인
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { // 케이스 1: 합칠 것 없음
    }
    else if (prev_alloc && !next_alloc) { // 케이스 2: 다음 블록과 합침
        delete_free(NEXT_BLKP(bp));                 // 다음 블록은 합쳐서 사라지므로 원래 구간에서 제거
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) { // 케이스 3: 이전 블록과 합침
        delete_free(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);                         // 합쳐진 블록의 시작은 이전 블록
    }
    else { // 케이스 4: 이전 + 현재 + 다음 합침
        delete_free(PREV_BLKP(bp));
        delete_free(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    insert_free(bp);                                // 합친 크기에 맞는 구간에 한 번만 삽입
    return bp;
}

/* ------------------------------------------------------------------------
 * place - 가용 블록 bp 를 구간 리스트에서 빼고 asize 만큼 할당. 남는 양이 최소 블록 이상이면 분할
 * ---------------------------------------------------------------------- */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    delete_free(bp); // 헤더 크기를 바꾸기 전에 (원래 구간에서 제거)

    if ((csize - asize) >= MINBLOCK) {   // 남는 조각이 가용 블록으로 쓸 수 있는 크기면 분할
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);              // 남는 조각
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        insert_free(bp); // 나머지 크기의 구간에 삽입. 다음 블록은 항상 할당 상태라 coalesce 불필요
    }
    else {                               // 너무 작으면 쪼개지 않고 통째로 할당
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* ------------------------------------------------------------------------
 * find_fit - asize 가 속한 구간부터 큰 구간 순으로 탐색
 *   처음으로 맞는 블록이 나온 구간에서 best-fit 을 고른다.
 *   (더 큰 구간의 블록은 항상 더 크므로, 작은 구간에서 찾았다면 그쪽이 낭비가 적다)
 * ---------------------------------------------------------------------- */
static void *find_fit(size_t asize)
{
    int c;
    void *bp;
    void *find_bp;

    for (c = get_class(asize); c < SEG_COUNT; c++) {
        find_bp = NULL;
        for (bp = seg_list[c]; bp != NULL; bp = SUCC(bp)) {
            size_t cursize = GET_SIZE(HDRP(bp));
            if (cursize >= asize) {          // 같은 구간에도 asize 보다 작은 블록이 있을 수 있어서 크기 확인 필요
                if (cursize == asize)
                    return bp;               // 딱 맞는 크기
                if (find_bp == NULL || GET_SIZE(HDRP(find_bp)) > cursize)
                    find_bp = bp;            // 이 구간에서 가장 작은 후보
            }
        }
        if (find_bp != NULL)
            return find_bp;                  // 이 구간에서 찾았으면 더 큰 구간은 볼 필요 없음
    }
    return NULL;
}

/* ------------------------------------------------------------------------
 * mm_malloc - 요청 크기를 블록 크기로 바꾸고, 맞는 블록을 찾아 배치. 없으면 힙을 늘린다
 * ---------------------------------------------------------------------- */
void *mm_malloc(size_t size)
{
    size_t asize;      // 실제 블록 크기 (헤더 + payload + 푸터, 8의 배수)
    size_t extendsize; // 맞는 블록이 없을 때 늘릴 크기
    char *bp;

    if (size == 0)
        return NULL;
    // 헤더+푸터 포함 8의 배수로 올림. 가용 시 pred/succ가 들어가야 해서 최소 MINBLOCK
    asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
    asize = MAX(asize, MINBLOCK);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        CHECK("malloc");
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    CHECK("malloc-extend");
    return bp;
}

/* ------------------------------------------------------------------------
 * mm_free - 가용으로 표시하고 이웃과 합침
 * ---------------------------------------------------------------------- */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);                   // 합치고 구간 리스트에 등록까지 수행
    CHECK("free");
}

/* ------------------------------------------------------------------------
 * mm_realloc - 새로 할당 + 복사 + 해제 (가장 단순한 방식)
 *   기존 payload 와 새 크기 중 작은 쪽만큼 복사한다.
 * ---------------------------------------------------------------------- */
void *mm_realloc(void *ptr, size_t size)
{
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = GET_SIZE(HDRP(ptr)) - DSIZE;   // 옛 payload 용량 = 블록 크기 - (헤더 + 푸터)
    if (size < copySize)
        copySize = size;
    memcpy(newptr, ptr, copySize);
    mm_free(ptr);

    return newptr;
}

#ifdef MM_DEBUG
/* ------------------------------------------------------------------------
 * check_heap - 일관성 검사 (MM_DEBUG 빌드에서만, 어긋나면 메시지를 찍고 abort)
 *   1) 힙을 훑으며 헤더 == 푸터, 가용 블록 바로 뒤가 가용이 아님(합치기 누락 없음)
 *   2) 각 구간 리스트를 훑으며 가용 상태인지, 올바른 구간에 있는지, pred/succ 링크가 맞물리는지
 *   3) 힙에서 센 가용 블록 수 == 모든 리스트에서 센 수
 * ---------------------------------------------------------------------- */
static void check_heap(const char *where)
{
    void *bp;
    int c;
    size_t heap_free = 0, list_free = 0;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (GET(HDRP(bp)) != GET(FTRP(bp))) {
            fprintf(stderr, "[%s] header/footer mismatch at %p\n", where, bp);
            abort();
        }
        if (!GET_ALLOC(HDRP(bp))) {
            heap_free++;
            if (!GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
                fprintf(stderr, "[%s] adjacent free blocks at %p\n", where, bp);
                abort();
            }
        }
    }
    for (c = 0; c < SEG_COUNT; c++) {
        for (bp = seg_list[c]; bp != NULL; bp = SUCC(bp)) {
            list_free++;
            if (GET_ALLOC(HDRP(bp))) {
                fprintf(stderr, "[%s] allocated block in free list %p (class %d)\n", where, bp, c);
                abort();
            }
            if (get_class(GET_SIZE(HDRP(bp))) != c) {
                fprintf(stderr, "[%s] block %p in wrong class %d (size %u)\n", where, bp, c, GET_SIZE(HDRP(bp)));
                abort();
            }
            if (SUCC(bp) != NULL && PRED(SUCC(bp)) != bp) {
                fprintf(stderr, "[%s] broken pred/succ link at %p\n", where, bp);
                abort();
            }
            if (bp == seg_list[c] && PRED(bp) != NULL) {
                fprintf(stderr, "[%s] head has pred at %p (class %d)\n", where, bp, c);
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
