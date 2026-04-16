/*
 * mm-naive.c - 가장 빠르지만 메모리 효율은 가장 낮은 malloc 패키지.
 *
 * 이 단순한 방식에서는 brk 포인터를 단순히 증가시켜 블록을 할당합니다.
 * 블록은 순수 payload만 가지며, header와 footer는 없습니다.
 * 블록들은 병합(coalesce)되거나 재사용되지 않습니다. realloc은
 * mm_malloc과 mm_free를 직접 이용해 구현됩니다.
 *
 * 학생 안내: 이 헤더 주석은 여러분이 구현한 방식에 대한 상위 수준 설명으로
 * 바꿔서 사용하세요.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * 학생 안내: 다른 작업을 하기 전에 아래 struct에 팀 정보를 먼저
 * 입력하세요.
 ********************************************************/
team_t team = {
    /* 팀 이름 */
    "ateam",
    /* 첫 번째 팀원 이름 **/
    "Harry Bovik",
    /* 첫 번째 팀원 이메일 */
    "bovik@cs.cmu.edu",
    /* 두 번째 팀원 이름 (없으면 빈칸) */
    "",
    /* 두 번째 팀원 이메일 (없으면 빈칸) */
    ""};

// 기본 상수 - CHUNKSIZE.md 확인
#define WSIZE 4 // 헤더/풋터 하나의 크기(4B). - HDRP, FTRP 계산에 쓰임
#define DSIZE 8 // 8바이트 정렬 단위이자 최소 페이로드 크기. - ALIGN, FTRP 등에 쓰임
#define CHUNKSIZE  (1<<10) // extend_heap 호출 시 최소 확장 크기.

// 정렬 관련 - ALIGN.md 확인
#define ALIGN(size) (((size) + (DSIZE - 1)) & ~(DSIZE - 1)) // 주어진 크기를 8B 배수로 올림  (쓸 때)

// 블록 정보 저장/읽기 - GET_PUT.md 확인
#define MAX(x, y) ((x) > (y) ? (x) : (y)) // mm_malloc에서 extendsize = MAX(asize, CHUNKSIZE)에서 사용.
#define PACK(size, alloc) ((size) | (alloc)) // 크기와 alloc 비트를 OR 연산으로 합쳐 헤더/풋터 값을 만듭니다.
#define GET(p) (*(unsigned int *)(p)) // 주소를 unsigned int *로 캐스팅해 4바이트를 읽습니다.
#define PUT(p, val) (*(unsigned int *)(p) = val) // 주소에 4바이트를 씁니다.


// 크기/alloc 추출 - GET_SIZE_GET_ALLOC.md 확인
#define GET_SIZE(p) (GET(p) & ~0x7) // 크기 추출 (하위 3비트 제거)
#define GET_ALLOC(p) (GET(p) & 0x1) // 할당 비트 추출

// 힙 순회
#define HDRP(bp) ((char *)(bp) - WSIZE) // 헤더 주소 -> bp - 4
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 풋터 주소 -> bp + size - 8
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp))) // 다음 블록 -> bp + GET_SIZE(헤더)
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)) // 이전 블록 -> bp - GET_SIZE(bp - 8의 값)


/* 프롤로그 블록의 payload를 가리키는 포인터 (힙 순회 시작점) */
static char *heap_listp; // 힙 순회의 시작점입니다. mm_init()에서 프롤로그 풋터를 가리키도록 초기화
static char *rover; // next-fit 탐색용: 마지막 탐색 위치 기억

static void *extend_heap(size_t words); // 힙이 부족할 때 OS에게 메모리를 더 요청
static void *coalesce(void *bp); // 인접한 가용 블록을 병합. mm_free와 extend_heap 모두 반납/확장 직후 호출
static void *find_fit(size_t asize); // 가용 리스트에서 asize 이상인 블록을 탐색
static void place(void *bp, size_t asize); // 찾은 블록에 할당 표시를 하고, 남는 공간이 충분하면 분할


// mm_init - malloc 패키지를 초기화한다.

int mm_init(void)
{
    /* TODO: 초기 heap 만들기.
     * - prologue/epilogue 세팅
     * - 필요하면 extend_heap() 호출해서 첫 free block 만들기
     */

    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1) {
        return -1;
    }

    PUT(heap_listp, 0); // 정렬 패딩
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1)); // 프롤로그 헤더
    PUT(heap_listp + 2*WSIZE, PACK(DSIZE, 1)); // 프롤로그 풋터
    PUT(heap_listp + 3*WSIZE, PACK(0, 1)); // 에필로그 헤더

    heap_listp += 2*WSIZE; // 프롤로그 블록 안을 가리키도록
    rover = heap_listp;

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) { // 첫 가용 블록 생성 (WSIZE로 나눠 words 단위로 전달)
        return -1;
    }

    return 0;
}

// extend_heap - 힙을 words 워드만큼 확장하고 새 가용 블록 반환
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    // 더블워드 정렬
    // words가 홀수면 +1 해서 짝수로 맞춤 (8바이트 정렬 유지)
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    // mem_sbrk()로 힙 확장 → header/footer 설정 → 에필로그 이동
    if ((bp = mem_sbrk(size)) == (void*)-1) {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0)); // 새 가용 블록 헤더
    PUT(FTRP(bp), PACK(size, 0)); // 새 가용 블록 풋터
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 새 에필로그 헤더

    return coalesce(bp); // 인접 블록 연결
}

/*
// first-fit: 앞에서부터 처음 맞는 블록 반환
static void *find_fit(size_t asize) {
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= asize)) {
            return bp;
        }
    }
    return NULL;
}
*/


// find_fit - next-fit 방식으로 asize 이상인 가용 블록 탐색
// 찾을 때마다 rover 갱신 → 다음 탐색은 여기서 시작

static void *find_fit(size_t asize) {
    void *bp;

    // 루프 1: rover부터 힙 끝까지 탐색
    for (bp = rover; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            rover = bp;
            return bp;
        }
    }

    // 루프 2: 못 찾으면 힙 처음부터 rover까지 탐색
    for (bp = heap_listp; bp < (void *)rover; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            rover = bp;
            return bp;
        }
    }
    
    // 둘 다 실패 → NULL (mm_malloc이 extend_heap 호출)
    return NULL;
}


/* 블록 배치 + 필요 시 분할 */
// place - bp 블록에 asize 할당, 남는 공간이 충분하면 split
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp)); // 찾은 블록의 실제 크기

    if ((csize - asize) >= (2*DSIZE)) { // 남는 공간 >= 최소 블록(16B)
        PUT(HDRP(bp), PACK(asize, 1)); // 앞쪽 할당 블록 헤더
        PUT(FTRP(bp), PACK(asize, 1)); // 앞쪽 할당 블록 풋터
        bp = NEXT_BLKP(bp); // bp를 나머지 공간으로 이동
        PUT(HDRP(bp), PACK(csize - asize, 0)); // 뒤쪽 가용 블록 헤더
        PUT(FTRP(bp), PACK(csize - asize, 0)); // 뒤쪽 가용 블록 풋터
    } else { // 남는 공간 부족 → 통째로 할당
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1)); 
    }
}

/*
 * mm_malloc - brk 포인터를 증가시켜 블록을 할당한다.
 *     항상 정렬 단위의 배수 크기로 블록을 할당한다.
 */
// mm_malloc - size 바이트 할당 후 payload 시작 주소 반환
// 흐름: find_fit → 없으면 extend_heap → place
void *mm_malloc(size_t size)
{
    /* TODO: 요청 크기에 맞는 block 할당하기.
     * - size == 0 처리
     * - asize 계산
     * - find_fit()으로 free block 찾기
     * - 없으면 extend_heap()
     * - place()로 block 배치
     */

    // header(4) + footer(4) 오버헤드 포함, 8의 배수로 올림
    size_t asize; // 조정된 블록 크기
    size_t extendsize; // 힙 확장 크기
    char *bp;

    if (size == 0) {
        return NULL;
    }

    /* 크기 조정 (헤더 + 풋터 포함, 8B 정렬) */
    if (size <= DSIZE) { // size <= 8  → asize = 16 (최소 블록)
        asize = 2 * DSIZE; // 최소 16B
    } else { // size > 8   → asize = 8 * ceil((size+8) / 8)
        asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
    }

    /* 가용 리스트에서 탐색 */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* 못 찾으면 힙 확장 */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) { // 실패인가?
        return NULL;
    }

    place(bp, asize);
    return bp;
}

/*
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
*/

/*
 * mm_free - 현재 템플릿에서는 free가 아무 일도 하지 않는다.
 */
// mm_free - bp 블록을 가용 상태로 전환하고 인접 블록과 병합

void mm_free(void *bp)
{
    /* TODO: block 반납 처리하기.
     * - header/footer를 free 상태로 바꾸기
     * - 인접 free block과 coalesce() 하기
     */
    // header와 footer의 alloc 비트를 0으로 바꾼 뒤 coalesce 호출

    size_t size = GET_SIZE(HDRP(bp)); // 블록 크기 읽기
    PUT(HDRP(bp), PACK(size, 0)); // 헤더 (1->0)
    PUT(FTRP(bp), PACK(size, 0)); // 풋터 (1->0)
    coalesce(bp); // 즉시 연결
}

// coalesce - bp 블록과 인접 가용 블록 병합
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록 풋터에서 alloc 읽기
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록 헤더에서 alloc 읽기
    size_t size = GET_SIZE(HDRP(bp)); // 현재 블록 크기

    /* case 1. 앞 할당, 뒤 할당 (병합 없음) - 생략 가능 */
    if (prev_alloc && next_alloc) { } // 아무것도 하지 않음, bp 그대로 반환

    /* case 2. 앞 할당, 뒤 가용 (뒤와 병합) */
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0)); // 현재 블록 헤더 갱신
        PUT(FTRP(bp), PACK(size, 0)); // FTRP가 병합된 블록 끝을 자동으로 가리킴
        if (rover > (char *)bp && rover < (char *)NEXT_BLKP(bp))
            rover = bp;
    }

    /* case 3. 앞 가용, 뒤 할당 (앞과 병합) */
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0)); // 현재 블록 풋터 갱신
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블록 헤더 갱신
        bp = PREV_BLKP(bp); // bp를 앞 블록 시작으로 이동
        if (rover >= (char *)bp && rover < (char *)NEXT_BLKP(bp))
            rover = bp;  // rover가 병합 범위 안 → 리셋
    }

    /* 앞 가용, 뒤 가용 (앞뒤 모두 병합) */
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블록 헤더 갱신
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // 다음 블록 풋터 갱신
        bp = PREV_BLKP(bp); // bp를 앞 블록 시작으로 이동
        if (rover >= (char *)bp && rover < (char *)NEXT_BLKP(bp))
            rover = bp;
        
    }
    return bp;
}

/*
 * mm_realloc - mm_malloc과 mm_free를 이용해 단순하게 구현되어 있다.
 * realloc은 이미 할당된 블록의 크기를 바꾸는 함수. 기존 데이터는 유지하면서 크기만 조정한다.
 */
// mm_realloc - 기존 블록 크기를 size로 재조정
void *mm_realloc(void *ptr, size_t size) // ptr : mm_malloc이 반환했던 페이로드 시작 주소
{
    /* TODO: block 크기 재조정하기.
     * - ptr == NULL 이면 mm_malloc(size)
     * - size == 0 이면 mm_free(ptr)
     * - 가능하면 제자리 확장/축소
     * - 안 되면 새 block 할당 후 memcpy
     */

    if (ptr == NULL) {
        return mm_malloc(size);
    }

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t old_size = GET_SIZE(HDRP(ptr));
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));

    // size → asize (mm_malloc과 동일한 정렬 계산)
    size_t asize;
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);

    // 제자리 확장 ①: 다음 블록이 free이고 합치면 충분한 경우
    if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) && old_size + next_size >= asize) {
        size_t total = old_size + next_size;
        PUT(HDRP(ptr), PACK(total, 1));
        PUT(FTRP(ptr), PACK(total, 1));
        return ptr;
    }

    // 제자리 확장 ②: 다음 블록이 epilogue (힙 끝) → 힙만 늘리기
    if (next_size == 0) {
        size_t extend = asize - old_size; // 부족한 만큼만 (정렬된 크기 기준)
        if (mem_sbrk(extend) != (void *)-1) {
            PUT(HDRP(ptr), PACK(old_size + extend, 1));
            PUT(FTRP(ptr), PACK(old_size + extend, 1));
            PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1)); // 새 epilogue
            return ptr;
        }
    }

    // fallback: 새 블록 할당 후 복사 (위 둘 다 불가한 경우)
    void *new_bp = mm_malloc(size);
    if (new_bp == NULL)
        return NULL;

    size_t copy_size = old_size - WSIZE; // payload 크기 (헤더 제외)
    if (size < copy_size)
        copy_size = size;
    memcpy(new_bp, ptr, copy_size);
    mm_free(ptr);
    return new_bp;
}

/*
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
*/