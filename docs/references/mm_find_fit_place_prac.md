# `find_fit()` + `place()` — 가용 블록 탐색 및 배치 함수

---

## 함수 코드

```c
// first-fit: 앞에서부터 처음 맞는 블록 반환
static void *find_fit(size_t asize) {
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
        if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= asize))
            return bp;
    return NULL;  // 못 찾으면 NULL
}

// 블록 배치 + 필요 시 분할
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)) {     // 남는 공간 >= 최소 블록(16B)
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0)); // 나머지: 새 가용 블록
        PUT(FTRP(bp), PACK(csize-asize, 0));
    } else {                                 // 남는 공간 부족 → 통째로 할당
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}
```

---

## 역할 요약

`find_fit()`과 `place()`는 `mm_malloc()`이 내부적으로 호출하는 한 쌍의 함수입니다.
핵심 역할은 딱 두 가지입니다.

1. **자리 찾기** — `find_fit()` : 힙을 순회해 asize 이상인 첫 번째 가용 블록 반환
2. **자리 배치** — `place()` : 찾은 블록에 할당 표시, 남는 공간은 분할해 가용 블록으로

---

## 두 함수의 협력 구조

```
mm_malloc(size)
      │
      ▼
  find_fit(asize)      ← 힙 전체를 순회하며 맞는 자리 탐색
      │
      ├─ NULL     → extend_heap() 으로 힙 확장
      │
      └─ bp 반환
           │
           ▼
       place(bp, asize) ← 해당 자리에 할당 표시 + 분할 여부 결정
           │
           ▼
       bp 반환 (사용자에게)
```

---

## `find_fit()` 단계별 실행 과정

### 탐색 흐름

```c
for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= asize))
        return bp;
return NULL;
```

- `bp = heap_listp` — 프롤로그 풋터 다음 블록부터 시작
- `GET_SIZE(HDRP(bp)) > 0` — 에필로그 헤더(size=0)를 만나면 루프 종료
- `bp = NEXT_BLKP(bp)` — 다음 블록으로 이동
- 조건 두 가지를 **모두** 만족해야 선택:
  - `!GET_ALLOC(HDRP(bp))` — 가용 블록이어야 함 (alloc=0)
  - `GET_SIZE(HDRP(bp)) >= asize` — 요청 크기 이상이어야 함

### 탐색 예시 (asize = 24B)

```
낮은 주소 ──────────────────────────────────────────── 높은 주소

  heap_listp
      ↓
┌──────┬──────┬──────────┬──────────┬──────────┬──────────┬──────┐
│ 패딩 │프롤F │  블록A   │  블록B   │  블록C   │  블록D   │에필H │
│      │      │ alloc=1  │ alloc=0  │ alloc=0  │ alloc=0  │size=0│
│      │      │  32B     │  16B     │  64B     │  24B     │      │
└──────┴──────┴──────────┴──────────┴──────────┴──────────┴──────┘
                  (1)         (2)        (3)
               alloc=1    16 < 24    64 ≥ 24
                skip        skip     → 반환!

  (1) 블록A: alloc=1 → skip (할당됨)
  (2) 블록B: alloc=0, 16 < 24 → skip (크기 부족)
  (3) 블록C: alloc=0, 64 ≥ 24 → return bp  ✓
```

### 탐색 실패 시

```
┌──────┬──────┬──────────┬──────────┬──────┐
│ 패딩 │프롤F │  블록A   │  블록B   │에필H │
│      │      │ alloc=1  │ alloc=1  │size=0│
└──────┴──────┴──────────┴──────────┴──────┘
                 skip        skip    size=0 → 루프 종료

→ return NULL  (mm_malloc이 extend_heap 호출)
```

---

## `place()` 단계별 실행 과정

### 분할 여부 판단

```c
size_t csize = GET_SIZE(HDRP(bp));  // 현재 블록의 전체 크기

if ((csize - asize) >= (2*DSIZE))   // 남는 공간 ≥ 16B ?
```

- `csize` — 찾은 가용 블록의 실제 크기
- `csize - asize` — 할당 후 남는 공간
- `2*DSIZE = 16B` — 블록 최솟값: 헤더(4B) + 데이터(8B) + 풋터(4B)

```
남는 공간 판단 기준

  csize=64, asize=24
  64 - 24 = 40 ≥ 16  → 분할 O

  csize=24, asize=24
  24 - 24 =  0 < 16  → 분할 X (통째로)

  csize=32, asize=24
  32 - 24 =  8 < 16  → 분할 X (통째로, 8B 조각은 너무 작아 버림)
```

---

### Case 1 — 분할하는 경우 (남는 공간 ≥ 16B)

```c
PUT(HDRP(bp), PACK(asize, 1));   // 앞쪽: 할당 블록 헤더
PUT(FTRP(bp), PACK(asize, 1));   // 앞쪽: 할당 블록 풋터
bp = NEXT_BLKP(bp);              // bp를 나머지 블록으로 이동
PUT(HDRP(bp), PACK(csize-asize, 0)); // 뒤쪽: 새 가용 블록 헤더
PUT(FTRP(bp), PACK(csize-asize, 0)); // 뒤쪽: 새 가용 블록 풋터
```

```
Before: 가용 블록 (csize=64B)

 bp
  ↓
┌──────────┬──────────────────────────────────────┬──────────┐
│  헤더    │            페이로드 공간             │  풋터    │
│PACK(64,0)│              (free)                  │PACK(64,0)│
└──────────┴──────────────────────────────────────┴──────────┘


After: place(bp, asize=24)

 bp (원래 위치)        bp (NEXT_BLKP 이후)
  ↓                         ↓
┌──────────┬──────┬──────────┬──────────────────┬──────────┐
│  헤더    │페이로│  풋터    │  헤더            │  풋터    │
│PACK(24,1)│  드  │PACK(24,1)│ PACK(40,0)       │PACK(40,0)│
│ alloc=1  │ 24B  │ alloc=1  │  alloc=0         │ alloc=0  │
└──────────┴──────┴──────────┴──────────────────┴──────────┘
└─────── 할당 블록 (24B) ───────┘└──── 가용 블록 (40B) ────┘
↑ mm_malloc이 반환하는 bp
```

---

### Case 2 — 분할 안 하는 경우 (남는 공간 < 16B)

```c
PUT(HDRP(bp), PACK(csize, 1));
PUT(FTRP(bp), PACK(csize, 1));
```

```
Before: 가용 블록 (csize=24B)

 bp
  ↓
┌──────────┬──────────────────┬──────────┐
│  헤더    │   페이로드 공간  │  풋터    │
│PACK(24,0)│     (free)       │PACK(24,0)│
└──────────┴──────────────────┴──────────┘


After: place(bp, asize=24)  →  통째로 할당

 bp
  ↓
┌──────────┬──────────────────┬──────────┐
│  헤더    │   페이로드 공간  │  풋터    │
│PACK(24,1)│  (사용자 데이터) │PACK(24,1)│
│ alloc=1  │                  │ alloc=1  │
└──────────┴──────────────────┴──────────┘
└──────────── 할당 블록 (24B) ────────────┘
↑ mm_malloc이 반환하는 bp
```

> 남는 8B로는 헤더+풋터(8B)만 겨우 채우므로 데이터를 전혀 담을 수 없어 분할하지 않습니다.

---

## 주요 매크로 정리

| 매크로 | 계산식 | 의미 |
|--------|--------|------|
| `GET_SIZE(p)` | `*(p) & ~0x7` | 헤더/풋터에서 블록 크기 추출 |
| `GET_ALLOC(p)` | `*(p) & 0x1` | 헤더/풋터에서 할당 여부 추출 |
| `HDRP(bp)` | `bp - WSIZE` | bp 블록의 헤더 주소 |
| `FTRP(bp)` | `bp + GET_SIZE(HDRP(bp)) - DSIZE` | bp 블록의 풋터 주소 |
| `NEXT_BLKP(bp)` | `bp + GET_SIZE(HDRP(bp))` | 다음 블록의 페이로드 주소 |

---

## 요약

```
find_fit(asize)
  ├─ heap_listp부터 순회
  ├─ alloc=0 AND size ≥ asize → bp 반환  ✓
  └─ 에필로그(size=0) 도달    → NULL 반환

place(bp, asize)
  ├─ csize - asize ≥ 16B  → 분할
  │   ├─ 앞쪽: PACK(asize,   1)  할당 블록
  │   └─ 뒤쪽: PACK(csize-asize, 0)  새 가용 블록
  └─ csize - asize < 16B  → 통째로 할당
      └─ PACK(csize, 1)
```
