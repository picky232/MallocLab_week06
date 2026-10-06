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


static char *heap_listp;   // 블록 포인터(다음 블록의 시작위치)
static char *start_listp; 

// 함수들 정의
int mm_init(void);
static void *extend_heap(size_t words);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);

/*
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) // heap 공간 초기 세팅 패딩, 프롤로그, 에필로그 세팅 -> 다만 안될경우에는 -1반환
        return -1;
    PUT(heap_listp, 0);                            // 정렬 패딩
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));   // 프롤로그 헤더
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));   // 프롤로그 푸터
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));       // 에필로그 헤더
    heap_listp += (2*WSIZE);                       // 프롤로그 블록의 payload 위치로 이동

    if(extend_heap(CHUNKSIZE/WSIZE)==NULL) // 힙 공간 늘리기 (4KB의 워드 개수 만큼)
        return -1; // 실패시 반환
    return 0;
}

static void *extend_heap(size_t words) { // 워드 개수 인자로 들어감
    char *bp; // block pointer
    size_t size; // 

    size = (words % 2) ? (words+1)*WSIZE : words*WSIZE; // 방어코드인데 쓸모없음 어짜피 여기서는 짝수만 나와서
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL; // 힙공간 늘리기 실패시 NULL 반환

    PUT(HDRP(bp), PACK(size, 0)); 
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    return coalesce(bp);
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
        return NULL;
    else
    {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}