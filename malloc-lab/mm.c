/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */

/* 한국어 번역
 * mm-naive.c - 가장 빠르지만 메모리 효율은 가장 낮은 malloc 패키지.
 *
 * 이 단순한 방식에서는 brk 포인터를 단순히 증가시키는 방식으로
 * 블록을 할당한다. 블록은 순수한 payload로만 구성된다.
 * header나 footer는 존재하지 않는다.
 * 블록들은 병합(coalesce)되거나 재사용되지 않는다.
 * realloc은 mm_malloc과 mm_free를 직접 사용하여 구현된다.
 *
 * 학생 참고: 이 헤더 주석을 여러분이 작성한 해결 방법을
 * 전체적으로 설명하는 자신만의 헤더 주석으로 교체하시오.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
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

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8 // 8바이트 정렬

/* rounds up to the nearest multiple of ALIGNMENT */
// 사용자가 요청한 size를 넣고 +7해서 8의 배수로 만들어줌. ~0x7로 비트 연산해서 하위 3비트 버림
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7) // malloc 크기 세기

#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) // 사이즈 정보 저장 32bit -> 4byte, 64bit -> 8byte

#define WSIZE 4 // 워드, 헤더/푸터 크기(바이트)
#define DSIZE 8 // 이중 워드 크기(바이트)
#define CHUNKSIZE (1<<12) // 힙을 이 크기(4KB)만큼 확장 - 비트 연산 << 오른쪽 12비트(4096)만큼 이동 4KB
#define MAX(x, y) ((x) > (y)? (x) : (y)) // X, Y중 더 큰쪽을 돌려주는 매크로

#define PACK(size, alloc) ((size) | (alloc)) // 크기와 할당 비트를 한 워드로 합침

#define GET(p) (*(unsigned int *)(p)) // 주소 p의 워드 읽기 - 포인터 선언후 바로 참조
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // 주소 p에 워드 쓰기 - 포인터 선언후 바로 참조

#define GET_SIZE(p) (GET(p) & ~0x7) // 크기 필드
#define GET_ALLOC(p) (GET(p) & 0x1) // 할당 비트

// bp는 블록 포인터인데 얘가 페이로드의 시작주소를 가리키는 이유 : 페이로드에 데이터를 써야해서 사용자에게 페이로드 시작주소를 반환해줌

#define HDRP(bp)  ((char *)(bp) - WSIZE)                      // 헤더 주소
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 푸터 주소

#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) // 다음 블록의 블록 포인터 - 현재블록의 헤더를 읽은후 크기만큼 더함
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // 이전 블록의 블록 포인터 - 푸터를 읽어서 이전 블록 크기 구함

/*
    이중 연결 리스트 형태로 할껀데 
    헤더 | pred(이전블록 포인터) | succ(다음블록 포인터) | 패딩 | 푸터 
    처럼 쓸꺼여서 아래와 같은 매크로 사용함
*/
#define MINBLOCK ALIGN(2*WSIZE + 2* sizeof(void *)) // 최소 블록 크기 (header+footer) + (PRED, SUCC 포인터 2개) = 8 + 16

#define PUT_PRED(bp, val) (*(void **)(bp)) = val // void* 로 캐스팅된 bp에 포인터를 담을꺼니까 이중 포인터로 캐스팅 함. 그리고 참조로 담은 주소 데이터가 반환됨
#define PUT_SUCC(bp, val) (*(void **)((char *)(bp)+sizeof(void *))) = val

#define READ_PRED(bp) (*(void **)(bp)) // 이전 블록 포인터
#define READ_SUCC(bp) (*(void **)((char *)(bp) + sizeof(void *)))

static char *heap_listp;   // 블록 포인터 - 프롤로그의 bp
static char *free_listp = NULL; // 가용 리스트 헤더

// 함수들 정의
int mm_init(void);
static void *extend_heap(size_t words);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);
static void *coalesce(void *bp);
static void place(void *bp, size_t size);
static void *find_fit(size_t asize);

// 가용 연결 리스트용
static void delete_free(void *bp);
static void insert_free(void *bp);
/*
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{
    // printf("SIZE_T_SIZE : %zu\n", SIZE_T_SIZE);
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) // heap 공간 초기 세팅 패딩, 프롤로그, 에필로그 세팅 -> 다만 안될경우에는 -1반환
        return -1;
    PUT(heap_listp, 0);                            // 정렬 패딩
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));   // 프롤로그 헤더
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));   // 프롤로그 푸터
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));       // 에필로그 헤더
    heap_listp += (2*WSIZE);                       // 프롤로그 블록의 payload 위치로 이동
    free_listp = NULL; // trace 마다 초기화
    
    if(extend_heap(CHUNKSIZE/WSIZE)==NULL) // 힙 공간 늘리기 (4KB의 워드 개수 만큼)
        return -1; // 실패시 반환
    return 0;
}

// 새 가용블록으로 힙 확장
static void *extend_heap(size_t words) { // 워드 개수 인자로 들어감
    char *bp; // block pointer
    size_t size; // 

    size = (words % 2) ? (words+1)*WSIZE : words*WSIZE; // 방어코드인데 쓸모없음 어짜피 여기서는 짝수만 나와서
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL; // 힙공간 늘리기 실패시 NULL 반환

    // 새 가용 블록
    PUT(HDRP(bp), PACK(size, 0)); 
    PUT(FTRP(bp), PACK(size, 0));

    // 에필로그
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    return coalesce(bp);
}

// 가용 연결 리스트
static void insert_free(void *bp)
{
    PUT_PRED(bp, NULL);
    PUT_SUCC(bp, free_listp);
    // 가용 연결 리스트 헤더 업데이트
    if(free_listp != NULL)
        PUT_PRED(free_listp, bp);
    free_listp = bp;
}

static void delete_free(void *bp)
{
    // bp가 헤드가 아닐떄
    if(READ_PRED(bp)!=NULL) 
        // PUT_PRED(READ_PRED(bp), READ_SUCC(bp)); 왜 아닌지 찾아보셈
        PUT_SUCC(READ_PRED(bp), READ_SUCC(bp));
    else
        // PUT_PRED(free_listp, READ_SUCC(bp)); 왜 아닌지 찾아보셈
        free_listp = READ_SUCC(bp);
    if(READ_SUCC(bp) != NULL)
        // PUT_SUCC(READ_SUCC(bp), READ_PRED(bp)); 얘도
        PUT_PRED(READ_SUCC(bp), READ_PRED(bp));
}

// mm_free 블록을 반환하고 경계태그 연결을 사용해서 상수 시간에 인접 가용 블록들과 통합함
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 할당 여부 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 할당 여부 확인
    size_t size = GET_SIZE(HDRP(bp)); // bp 블록 크기
    void *start = bp; // 병합후 블록 포인터

    if(!prev_alloc){
        start = PREV_BLKP(bp);
        delete_free(start);
        size += GET_SIZE(HDRP(start));
    }
    if(!next_alloc){
        delete_free(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    }
    PUT(HDRP(start), PACK(size, 0));
    PUT(FTRP(start), PACK(size, 0));
    insert_free(start);
    return start;
}

// 블록 분활 판단후 배치함
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp)); // 찾은 블록의 크기
    delete_free(bp); // 할당되는 블록은 리스트에서 제거 (헤더 크기를 바꾸기 전에)

    if((csize - asize)>=(MINBLOCK)){ // 찾은 블록 크기 - 요청한 크기를 했을때 남는 양이 최소블록 크기보다 크면 분활
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        insert_free(bp); // 남은 조각의 다음 블록은 항상 할당 상태라 coalesce 불필요
    }else{ // 최소 블록 크기안되면 그냥 통채로 할당
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

// best-fit 방식
static void *find_fit(size_t asize)
{
    void *bp;
    void *find_bp = NULL;
    for(bp = free_listp; bp!=NULL; bp=READ_SUCC(bp)){
        size_t cursize = GET_SIZE(HDRP(bp));
        if(cursize>=asize){
            if(cursize==asize) // 딱맞는 크기
                return bp;
            if(find_bp==NULL || (GET_SIZE(HDRP(find_bp))>GET_SIZE(HDRP(bp)))){
                find_bp = bp;  // 지금까지 가장 작은 후보 갱신
            }
        }
    }
    return find_bp;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

 // double word align
void *mm_malloc(size_t size)
{
    // int newsize = ALIGN(size + SIZE_T_SIZE); // 정렬
    // void *p = mem_sbrk(newsize); // 크기 늘려줌
    // if (p == (void *)-1)
    //     return NULL;
    // else
    // {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }

    size_t asize; // allocated size (header + payload + footer)
    size_t extendsize; // 
    char *bp;

    if(size == 0){
        return NULL;
    }
    asize = DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE); 
    asize = MAX(asize, MINBLOCK); // MAX(블록 크기 최소 크기, 요청 사이즈)

    // free 목록중에서 알맞는 곳 찾아서 할당함
    if((bp = find_fit(asize)) != NULL){ 
        place(bp, asize);
        return bp;
    }

    // 알맞는 곳을 못찾았을때 더 많은 메모리를 요청함
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE))==NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) // free할 위치가 인자로 들어옴
{
    size_t size = GET_SIZE(HDRP(ptr));  // 블록 사이즈

    PUT(HDRP(ptr), PACK(size, 0)); // 가용블록으로 만듦
    PUT(FTRP(ptr), PACK(size, 0)); // 가용블록으로 만듦
    coalesce(ptr); // 주위 블록 확인
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = GET_SIZE(HDRP(ptr)) - DSIZE;
    if (size < copySize)
        copySize = size;
    memcpy(newptr, ptr, copySize);
    mm_free(ptr);
    
    return newptr;
}

// copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    // 1. 줄이는 경우(size가 더 작음) - 새로할당 X, 그 블록을 그대로 두거나 남는 부분만 분활
    // 2. 늘리는 경우, 뒤 블록이 가용이고, 합치면 충분 - 뒤 블록과 합쳐서 같은 주소 유지
    // 3. 늘리는 경우, 이 블록이 힙 끝 - 힙을 늘려서 같은 주소 유지
    // 4. 그외 - 지금처럼 새로 할당, 복사, 해제