# `mm_init()` 자세히 설명

## 1. `mm_init()`이 하는 일

`mm_init()`은 malloc 구현에서 **가장 먼저 호출되는 초기화 함수**이다.  
이 함수의 목적은 힙을 그냥 빈 메모리 덩어리로 두는 것이 아니라,  
**할당기가 이해할 수 있는 규칙 있는 구조**로 바꿔 놓는 것이다.

한 문장으로 요약하면:

> `mm_init()`은 힙의 시작 상태를 세팅하고, 첫 번째 가용 블록까지 만들어서 이후 `malloc`/`free`가 동작할 수 있게 준비하는 함수이다.

즉, 이 함수는 아직 사용자의 메모리 요청을 처리하는 단계가 아니라,  
그 요청을 처리할 수 있도록 **기본 뼈대**를 세우는 단계이다.

---

## 2. 코드

```c
int mm_init(void) {
    // 4워드짜리 초기 공간 확보
    heap_listp = mem_sbrk(4 * WSIZE);

    PUT(heap_listp,           0);              // 패딩
    PUT(heap_listp + WSIZE,   PACK(DSIZE, 1)); // 프롤로그 헤더
    PUT(heap_listp + 2*WSIZE, PACK(DSIZE, 1)); // 프롤로그 풋터
    PUT(heap_listp + 3*WSIZE, PACK(0, 1));     // 에필로그 헤더

    heap_listp += 2 * WSIZE; // 프롤로그 블록 안을 가리키도록

    extend_heap(CHUNKSIZE / WSIZE); // 첫 가용 블록 생성
}
