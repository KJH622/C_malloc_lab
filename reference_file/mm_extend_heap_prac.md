# `extend_heap()` 자세히 설명

## 1. `extend_heap()`이 하는 일

`extend_heap()`은 힙 공간이 부족할 때 **힙을 더 늘려서 새로운 free block을 만드는 함수**이다.  
즉, 기존 힙 안에서 더 이상 적당한 가용 블록을 찾지 못했을 때,  
OS 쪽에서 메모리를 더 받아와 힙 뒤에 붙이는 역할을 한다.

한 문장으로 요약하면:

> `extend_heap()`은 힙을 확장하고, 새로 확보한 공간을 free block으로 등록한 뒤, 필요하면 앞쪽 free block과 합쳐 주는 함수이다.

즉 이 함수는 단순히 메모리만 늘리는 것이 아니라,  
그 새 공간을 **할당기가 이해할 수 있는 블록 구조**로 바꾸는 단계까지 포함한다.

---

## 2. 코드

```c
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    // ① 더블워드 정렬: 홀수 words면 +1 해서 짝수로
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1)      // ② OS에게 메모리 요청
        return NULL;

    PUT(HDRP(bp),            PACK(size, 0));     // ③ 새 가용 블록 헤더
    PUT(FTRP(bp),            PACK(size, 0));     // ③ 새 가용 블록 풋터
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));       // ③ 새 에필로그 헤더

    return coalesce(bp);                          // ④ 인접 블록 연결
}