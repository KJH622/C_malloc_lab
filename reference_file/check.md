# Malloc Lab 정리

---

## 1. 파일 구조

```
malloc-lab/
├── mm.c          ← 직접 구현 (malloc / free / realloc)
├── mm.h          ← 함수 선언 + team_t 구조체
├── mdriver.c     ← 테스트 드라이버 (trace 읽어 채점)
├── memlib.c/h    ← 가상 힙 제공, mem_sbrk() 로 확장
├── config.h      ← ALIGNMENT=8, 최대 힙 20MB
└── traces/
    ├── short1-bal.rep      디버깅용 소형
    ├── short2-bal.rep      디버깅용 소형
    ├── coalescing-bal.rep  coalescing 검증
    ├── realloc-bal.rep     realloc 품질
    ├── realloc2-bal.rep    realloc 품질
    ├── random-bal.rep      일반 견고성
    ├── random2-bal.rep     일반 견고성
    ├── binary-bal.rep      특정 할당 패턴
    └── binary2-bal.rep     특정 할당 패턴
```

trace 한 줄 형식: `a <id> <bytes>` | `r <id> <bytes>` | `f <id>`

---

## 2. 현재 구현 방식 — Segregated Explicit Free List + Footer Elision

```
┌─────────────────────────────────────────────────────────────┐
│  구현 방식 계층                                              │
│                                                             │
│  Implicit Free List          기본 (교재 시작점)              │
│       ↓                                                     │
│  Explicit Free List          free 블록에 pred/succ 포인터    │
│       ↓                                                     │
│  Segregated Free List   ←←  현재 구현 (20개 크기 클래스)    │
│       +                                                     │
│  Footer Elision              할당 블록은 footer 없음         │
│       +                                                     │
│  prev_alloc bit              header에 이전 블록 상태 저장    │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. 블록 구조

### 3-1. Header 비트 레이아웃

```
 31                    3   2          1          0
 ┌──────────────────────┬──┬──────────┬──────────┐
 │       size           │  │prev_alloc│  alloc   │
 │   (블록 전체 크기)    │  │ (이전블록 │ (현재블록 │
 │                      │  │  할당여부)│  할당여부)│
 └──────────────────────┴──┴──────────┴──────────┘
                        ↑
                      unused

  size      = GET(p) & ~0x7      // 하위 3비트 제외
  alloc     = GET(p) & 0x1       // bit 0
  prev_alloc= GET(p) & 0x2 >> 1  // bit 1
```

### 3-2. 할당 블록 (Allocated Block)

```
 bp-4       bp                           bp + size - 4
  ┌──────────┬────────────────────────────┐
  │  HEADER  │         payload            │  ← footer 없음!
  │size|P|1  │    (사용자 데이터)          │
  └──────────┴────────────────────────────┘
  ◀──────────────────── size ─────────────▶

  P = prev_alloc bit (이전 블록 할당 여부)
  footer 생략 → 공간 절약, 단 이전 블록이 free일 때만 footer 필요
```

### 3-3. 자유 블록 (Free Block)

```
 bp-4       bp          bp+4        bp+8      bp + size - 4
  ┌──────────┬───────────┬───────────┬──────────┐
  │  HEADER  │   PRED    │   SUCC    │  FOOTER  │
  │size|P|0  │ (이전 free│ (다음 free│size|P|0  │
  │          │  블록 ptr) │  블록 ptr) │          │
  └──────────┴───────────┴───────────┴──────────┘
  ◀──────────────────── size ──────────────────▶

  PRED/SUCC: 힙 시작점 기준 offset (4바이트) 으로 저장
             NULL → offset 0
  이유: 64비트 포인터 8바이트를 4바이트로 압축 가능
```

### 3-4. 최소 블록 크기

```
  할당 블록:  header(4) + payload(최소 4) = 8 bytes  (MIN_ALLOC_BLOCK_SIZE)
  자유 블록:  header(4) + pred(4) + succ(4) + footer(4) = 16 bytes  (MIN_FREE_BLOCK_SIZE)
```

---

## 4. 힙 전체 구조

```
 mem_heap_lo()                                      mem_heap_hi()
      ↓                                                   ↓
  ┌───────┬─────────────┬─────────────┬──── ··· ────┬───────┐
  │padding│  prologue   │  prologue   │             │epilo- │
  │ (4B)  │   header    │   footer    │   블록들     │ gue   │
  │  0x0  │ 8|1|1       │ 8|1|1       │             │ 0|1|1 │
  └───────┴─────────────┴─────────────┴──── ··· ────┴───────┘
      4B        4B            4B                         4B

  heap_listp ──────────────────↑ (prologue footer 직후, payload 시작점)

  prologue: 크기=8, alloc=1, prev_alloc=1  → coalesce 경계 역할
  epilogue: 크기=0, alloc=1               → 힙 끝 표시
```

---

## 5. Segregated Free List (20개 클래스)

```
  seg_free_lists[20]                         힙 안의 free 블록들
  ┌──────────┐
  │  [0]  ──────→ [16B]──→[16B]──→ NULL      크기: 16
  ├──────────┤
  │  [1]  ──────→ [32B]──→ NULL              크기: 17–32
  ├──────────┤
  │  [2]  ──────→ [64B]──→[48B]──→ NULL      크기: 33–64
  ├──────────┤
  │  [3]  ──────→ NULL                       크기: 65–128
  ├──────────┤
  │  [4]  ──────→ [256B]──→ NULL             크기: 129–256
  ├──────────┤
  │  ...  │
  ├──────────┤
  │  [19] ──────→ [huge]──→ NULL             크기: 매우 큰 블록
  └──────────┘

  index 결정: size > 16<<i 이면 i++ (최대 index 19)
  삽입:  LIFO (리스트 맨 앞에 삽입)
  삭제:  doubly linked list 방식으로 O(1) 제거
```

---

## 6. 핵심 함수 흐름

### 6-1. mm_init

```
mm_init()
  │
  ├─ seg_free_lists[0..19] = NULL  초기화
  │
  ├─ mem_sbrk(16)  →  4워드 확보
  │   ├─ [0]  padding  0x0
  │   ├─ [4]  prologue header  PACK(8, 1, 1)
  │   ├─ [8]  prologue footer  PACK(8, 1, 1)
  │   └─ [12] epilogue header  PACK(0, 1, 1)
  │
  └─ extend_heap(CHUNKSIZE/WSIZE)  →  초기 free 블록 생성
```

### 6-2. mm_malloc

```
mm_malloc(size)
  │
  ├─ size == 0 → return NULL
  │
  ├─ asize = ALIGN(size + WSIZE)   // header만 추가 (footer 없음)
  │         최소 MIN_ALLOC_BLOCK_SIZE(8) 보장
  │
  ├─ find_fit(asize)
  │   ├─ 성공 → place(bp, asize) → return bp
  │   └─ 실패 ↓
  │
  └─ extend_heap(MAX(asize, CHUNKSIZE))
      └─ place(bp, asize) → return bp
```

### 6-3. mm_free

```
mm_free(ptr)
  │
  ├─ ptr == NULL → return
  │
  ├─ size      = GET_SIZE(HDRP(ptr))
  ├─ prev_alloc= GET_PREV_ALLOC(HDRP(ptr))
  │
  ├─ write_free_block(ptr, size, prev_alloc)
  │   ├─ header 작성 (alloc=0)
  │   ├─ footer 작성
  │   ├─ SET_PRED/SUCC = NULL
  │   └─ 다음 블록 header의 prev_alloc bit = 0
  │
  └─ coalesce(ptr)
```

### 6-4. coalesce

```
coalesce(bp)

  prev_alloc = GET_PREV_ALLOC(HDRP(bp))   // header의 bit 1
  next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)))

  ┌─────────────┬─────────────┬──────────────────────────────────────┐
  │  prev_alloc │  next_alloc │  처리                                 │
  ├─────────────┼─────────────┼──────────────────────────────────────┤
  │      1      │      1      │  그냥 insert_block → return bp        │
  ├─────────────┼─────────────┼──────────────────────────────────────┤
  │      1      │      0      │  remove(next) → 합치기 → insert       │
  │             │             │  [bp | next] 로 병합                  │
  ├─────────────┼─────────────┼──────────────────────────────────────┤
  │      0      │      1      │  remove(prev) → 합치기 → insert       │
  │             │             │  [prev | bp] 로 병합 → return prevbp  │
  ├─────────────┼─────────────┼──────────────────────────────────────┤
  │      0      │      0      │  remove(prev), remove(next) → 3방향   │
  │             │             │  [prev | bp | next] 병합              │
  └─────────────┴─────────────┴──────────────────────────────────────┘

  prev_alloc 읽기: PREV_BLKP() 대신 header bit 사용 → O(1), footer 불필요
  단, prev가 free일 때 PREV_BLKP()는 이전 블록 footer를 읽어야 하므로
  free 블록은 여전히 footer 필요
```

### 6-5. find_fit (Best-fit with SEARCHLIMIT)

```
find_fit(asize)
  │
  ├─ index = get_list_index(asize)   // 최소 적합 클래스부터 탐색
  │
  └─ for index → LISTLIMIT-1:
      └─ 리스트 순회 (최대 SEARCHLIMIT=8개)
          ├─ block_size == asize       → 즉시 반환 (perfect fit)
          ├─ index > start_index       → 즉시 반환 (상위 클래스는 충분히 큼)
          └─ block_size < best_size   → best 갱신
      └─ best 발견 시 반환

  전략: 같은 클래스 내에서 best-fit, 상위 클래스는 first-fit
```

### 6-6. place

```
place(bp, asize)
  │
  ├─ csize  = GET_SIZE(HDRP(bp))
  ├─ remain = csize - asize
  │
  ├─ remove_block(bp)   // free list에서 제거
  │
  ├─ remain >= 16 (MIN_FREE_BLOCK_SIZE)?
  │   ├─ YES: split
  │   │   ├─ write_alloc_block(bp, asize)     // 앞부분 할당
  │   │   └─ write_free_block(next, remain)   // 나머지 free
  │   │       └─ insert_block(next, remain)
  │   └─ NO:  통째로 할당 (내부 단편화 허용)
  │       └─ write_alloc_block(bp, csize)
```

---

## 7. mm_realloc 전략 (5단계)

```
mm_realloc(ptr, size)
  │
  ├─ ptr == NULL  → mm_malloc(size)
  ├─ size == 0    → mm_free(ptr), return NULL
  │
  ├─ asize = adjust_block_size(size)
  ├─ oldsize = GET_SIZE(HDRP(ptr))
  │
  │  [전략 1] Shrink — 현재 블록이 이미 충분히 큰 경우
  ├─ oldsize >= asize
  │   └─ remain >= 16 → split 후 return ptr  (제자리)
  │
  │  [전략 2] 다음 블록이 free이고 합치면 충분한 경우
  ├─ !GET_ALLOC(next) && oldsize + nextsize >= asize
  │   └─ remove(next) → 합치기 → split? → return ptr  (제자리)
  │
  │  [전략 3] 이전 블록이 free이고 합치면 충분한 경우
  ├─ !GET_PREV_ALLOC && prevsize + oldsize (+ nextsize?) >= asize
  │   └─ remove(prev) → memmove(prevbp, ptr) → return prevbp  (제자리)
  │       ※ memmove 사용: prev와 ptr 영역 겹칠 수 있으므로
  │
  │  [전략 4] 현재 블록이 힙 끝이면 힙만 연장
  ├─ GET_SIZE(next) == 0  (next가 epilogue)
  │   └─ mem_sbrk(asize - oldsize) → return ptr  (제자리)
  │
  │  [전략 5] 완전 복사 fallback
  └─ mm_malloc(size) → memcpy → mm_free(ptr) → return newptr
```

---

## 8. 매크로 정리

```c
// 기본 상수
WSIZE      4          // word (header/footer 크기)
DSIZE      8          // double word (정렬 단위)
CHUNKSIZE  128        // 힙 확장 단위 (bytes)
LISTLIMIT  20         // segregated list 클래스 수
MIN_FREE_BLOCK_SIZE 16
MIN_ALLOC_BLOCK_SIZE 8
SEARCHLIMIT 8         // find_fit 내 최대 검사 블록 수

// 비트 마스크
ALLOC_MASK      0x1   // bit 0: 현재 블록 할당 여부
PREV_ALLOC_MASK 0x2   // bit 1: 이전 블록 할당 여부

// 값 읽기/쓰기
GET(p)        *(unsigned int *)(p)
PUT(p, val)   *(unsigned int *)(p) = (val)
PACK(size, prev_alloc, alloc)   size | (prev_alloc<<1) | alloc

// header에서 추출
GET_SIZE(p)       GET(p) & ~0x7
GET_ALLOC(p)      GET(p) & 0x1
GET_PREV_ALLOC(p) (GET(p) & 0x2) >> 1

// 블록 포인터 계산
HDRP(bp)      (char*)(bp) - WSIZE
FTRP(bp)      (char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE
NEXT_BLKP(bp) (char*)(bp) + GET_SIZE(HDRP(bp))
PREV_BLKP(bp) (char*)(bp) - GET_SIZE((char*)(bp) - DSIZE)  // 이전 블록 footer 참조

// explicit list (free 블록 내부)
PRED_FIELD(bp)  (char*)(bp)         // pred offset 저장 위치
SUCC_FIELD(bp)  (char*)(bp) + WSIZE // succ offset 저장 위치
PRED(bp)        offset → pointer 변환
SUCC(bp)        offset → pointer 변환

// offset ↔ pointer (힙 시작 기준 4바이트 offset)
PTR_TO_OFF(ptr)  ptr == NULL ? 0 : (unsigned int)(ptr - mem_heap_lo())
OFF_TO_PTR(off)  off == 0 ? NULL : mem_heap_lo() + off
```

---

## 9. 디버깅 순서

```bash
make

./mdriver -V -f short1-bal.rep        # 가장 단순한 trace부터
./mdriver -V -f short2-bal.rep
./mdriver -V -f coalescing-bal.rep    # coalesce 검증
./mdriver -V -f realloc-bal.rep       # realloc 제자리 확장 검증
./mdriver -V -f realloc2-bal.rep
./mdriver -V -f binary-bal.rep        # 할당 패턴 검증
./mdriver -V                          # 전체 채점
```

주요 `-V` 출력 해석:
```
util = 공간 활용도 (높을수록 좋음, 목표 85%+)
thru = 처리량 ops/sec (높을수록 좋음)
score = util × 60% + thru × 40%
```

---

## 10. 성능 포인트 요약

| 항목 | 현재 구현 | 효과 |
|------|-----------|------|
| Segregated free list (20클래스) | O(log N) find_fit | 처리량 향상 |
| Footer elision (할당 블록) | 내부 단편화 감소 | 공간 활용도 향상 |
| prev_alloc bit | PREV_BLKP 없이 coalesce | 안전성 + 속도 |
| LIFO 삽입 | O(1) insert | 처리량 향상 |
| Best-fit (SEARCHLIMIT=8) | 단편화 감소 | 공간 활용도 향상 |
| realloc 제자리 확장 (4전략) | memcpy 최소화 | realloc 점수 향상 |
| offset 압축 (4B) | 포인터 절반 크기 | 공간 절약 |
