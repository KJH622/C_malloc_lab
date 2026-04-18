# 🏗️ mm_init — 힙 초기화

> malloc 패키지를 처음 사용하기 전에 딱 한 번 호출되는 초기화 함수.
> 프롤로그/에필로그 블록을 세팅하고 첫 번째 가용 블록을 만듭니다.

---

## 코드

```c
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1)
        return -1;

    PUT(heap_listp,          0);              // ① 정렬 패딩
    PUT(heap_listp + WSIZE,  PACK(DSIZE, 1)); // ② 프롤로그 헤더
    PUT(heap_listp + 2*WSIZE, PACK(DSIZE, 1)); // ③ 프롤로그 풋터
    PUT(heap_listp + 3*WSIZE, PACK(0, 1));    // ④ 에필로그 헤더

    heap_listp += 2*WSIZE;  // ⑤ heap_listp를 프롤로그 FTR로 이동
    rover = heap_listp;     // ⑥ rover 초기화

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) // ⑦ 첫 가용 블록 생성
        return -1;

    return 0;
}
```

---

## 초기화 과정 시각화

### ① ~ ④: mem_sbrk(16) 후 초기 힙 세팅

```
mem_sbrk(16) 반환값 = 힙 시작 주소 (여기서는 1000이라 가정)

주소:  1000   1004          1008          1012
       ┌──────┬─────────────┬─────────────┬────────────┐
       │  0   │  PACK(8,1)  │  PACK(8,1)  │  PACK(0,1) │
       │패딩  │ 프롤로그 HDR │ 프롤로그 FTR │ 에필로그 HDR│
       └──────┴─────────────┴─────────────┴────────────┘
heap_listp ──→ 1000 (mem_sbrk 반환값)
```

### ⑤: heap_listp += 2*WSIZE (= +8)

```
주소:  1000   1004          1008          1012
       ┌──────┬─────────────┬─────────────┬────────────┐
       │  0   │  PACK(8,1)  │  PACK(8,1)  │  PACK(0,1) │
       └──────┴─────────────┴─────────────┴────────────┘
                                    ↑
                              heap_listp = 1008
                              (프롤로그 FTR 위치)
```

### ⑦: extend_heap(256) 후 — 첫 가용 블록 생성

```
주소:  1000   1004   1008          1012    ...    2036          2040
       ┌──────┬──────┬─────────────┬───────────────┬─────────────┐
       │ 패딩 │ P.HDR│  P.FTR      │  가용 블록     │ 에필로그 HDR │
       │  0   │ 8|1  │  8|1        │  HDR+PAYLOAD  │    0|1      │
       │      │      │             │    +FTR        │             │
       └──────┴──────┴─────────────┴───────────────┴─────────────┘
                           ↑                              
                      heap_listp                    
                      rover                         
```

---

## 각 요소의 역할

| 요소 | 값 | 목적 |
|------|-----|------|
| 정렬 패딩 | `0` | 이후 8바이트 정렬 맞추기 위한 더미 |
| 프롤로그 HDR | `PACK(8, 1)` | size=8, alloc=1. 순회 시 경계 역할 |
| 프롤로그 FTR | `PACK(8, 1)` | `PREV_BLKP(첫블록)`이 이 FTR을 읽음 |
| 에필로그 HDR | `PACK(0, 1)` | size=0 → 힙 순회 종료 조건 |

### 프롤로그가 필요한 이유

```
가용 블록이 힙의 맨 앞에 있을 때 coalesce 시도:

PREV_BLKP(첫블록) → 프롤로그 FTR을 읽음
GET_ALLOC(프롤로그 FTR) = 1  →  "이전 블록은 할당됨"
→ 앞쪽으로 병합 시도 안 함 ✓

프롤로그가 없으면? → 힙 바깥 메모리를 읽는 undefined behavior 발생!
```

### 에필로그가 필요한 이유

```
힙 순회 루프:
for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))

에필로그 HDR의 size = 0 → 루프 종료 조건 충족
에필로그 HDR의 alloc = 1 → coalesce 시 뒤쪽 병합 방지

에필로그가 없으면? → 힙 끝을 지나서 무한 순회!
```

---

## extend_heap 첫 호출 이유

```
mm_init 직후엔 가용 블록이 없음.
첫 mm_malloc 호출 시 find_fit이 항상 NULL을 반환하게 됨.
→ 미리 CHUNKSIZE(1KB)만큼 가용 블록을 만들어 두는 것!

CHUNKSIZE/WSIZE = 1024/4 = 256 words → 256 * 4 = 1024 bytes
```

---

## 반환값

| 반환값 | 의미 |
|--------|------|
| `0` | 초기화 성공 |
| `-1` | `mem_sbrk` 실패 (메모리 부족) |
