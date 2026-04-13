# `mm_free()` + `coalesce()` — 메모리 반납 및 병합 함수

---

## 함수 코드

```c
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));   // 헤더: 가용으로
    PUT(FTRP(bp), PACK(size, 0));   // 풋터: 가용으로
    coalesce(bp);                    // 즉시 연결
}

static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if      (prev_alloc && next_alloc)   { }                   // Case 1
    else if (prev_alloc && !next_alloc)  {                     // Case 2
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)  {                     // Case 3
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp),            PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {                                                      // Case 4
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))
              + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}
```

---

## 역할 요약

`mm_free()`와 `coalesce()`는 할당된 메모리를 반납하고 인접한 가용 블록과 합치는 한 쌍의 함수입니다.
핵심 역할은 딱 두 가지입니다.

1. **메모리 반납** — `mm_free()` : 헤더/풋터의 alloc 비트를 0으로 변경
2. **인접 블록 병합** — `coalesce()` : 앞뒤 블록 상태에 따라 4가지 케이스로 병합

---

## 두 함수의 협력 구조

```
mm_free(bp) 호출
      │
      ▼
  헤더/풋터 alloc=0 으로 변경    ← 블록을 "가용" 상태로 표시
      │
      ▼
  coalesce(bp)                   ← 인접 블록과 병합 시도
      │
      ├─ Case 1: 앞 alloc, 뒤 alloc  → 병합 없음
      ├─ Case 2: 앞 alloc, 뒤 free   → 뒤 블록과 병합
      ├─ Case 3: 앞 free,  뒤 alloc  → 앞 블록과 병합
      └─ Case 4: 앞 free,  뒤 free   → 앞뒤 모두 병합
            │
            ▼
        bp 반환 (병합된 가용 블록의 시작)
```

---

## 힙 상태 변화 레이아웃 (낮은 주소 → 높은 주소)

```
free 호출 전
┌────────┬────────┬──────────┬──────────┬──────────┬────────┐
│ 패딩   │ 프롤H  │  블록A   │  블록B   │  블록C   │ 에필H  │
│        │PACK8,1 │ alloc=1  │ alloc=1  │ alloc=1  │PACK0,1 │
└────────┴────────┴──────────┴──────────┴──────────┴────────┘

mm_free(블록B) 호출 후 → coalesce 실행
┌────────┬────────┬──────────┬──────────┬──────────┬────────┐
│ 패딩   │ 프롤H  │  블록A   │  블록B   │  블록C   │ 에필H  │
│        │PACK8,1 │ alloc=1  │ alloc=0  │ alloc=1  │PACK0,1 │
└────────┴────────┴──────────┴──────────┴──────────┴────────┘
                              ↑ bp
                         앞뒤 상태에 따라 병합 결정
```

> `coalesce()`가 이전 블록 상태를 알 수 있는 이유:
> 각 블록 끝에 **풋터**가 있기 때문에 `FTRP(PREV_BLKP(bp))`로 이전 블록의 alloc 비트를 바로 읽습니다.

---

## `mm_free()` 단계별 실행

### Step 1 — 헤더/풋터를 가용으로 변경

```c
size_t size = GET_SIZE(HDRP(bp));
PUT(HDRP(bp), PACK(size, 0));   // alloc 1 → 0
PUT(FTRP(bp), PACK(size, 0));   // alloc 1 → 0
```

```
Before: 할당된 블록
 bp-4        bp                    bp+size-8  bp+size-4
┌──────────┬──────────────────────┬──────────┐
│PACK(sz,1)│    페이로드 (사용중) │PACK(sz,1)│
│ alloc=1  │                      │ alloc=1  │
└──────────┴──────────────────────┴──────────┘

After: PUT 2회
┌──────────┬──────────────────────┬──────────┐
│PACK(sz,0)│    페이로드 (반납됨) │PACK(sz,0)│
│ alloc=0  │                      │ alloc=0  │
└──────────┴──────────────────────┴──────────┘
```

---

## `coalesce()` — 4가지 병합 케이스

coalesce는 먼저 앞뒤 블록의 상태를 읽습니다.

```c
size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));  // 이전 블록 풋터
size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));  // 다음 블록 헤더
size_t size = GET_SIZE(HDRP(bp));
```

---

### Case 1 — 앞 alloc, 뒤 alloc : 병합 없음

```
┌──────────┬──────────┬──────────┐
│  이전블록 │  현재 bp │  다음블록 │
│ alloc=1  │ alloc=0  │ alloc=1  │
└──────────┴──────────┴──────────┘

→ 아무것도 하지 않음, bp 그대로 반환
```

---

### Case 2 — 앞 alloc, 뒤 free : 뒤 블록과 병합

```c
size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
PUT(HDRP(bp), PACK(size, 0));
PUT(FTRP(bp), PACK(size, 0));
```

```
Before:
┌──────────┬──────────┬──────────┐
│  이전블록 │  현재 bp │  다음블록 │
│ alloc=1  │ 32B free │ 16B free │
└──────────┴──────────┴──────────┘
                ↑ bp

After: size = 32+16 = 48B
┌──────────┬──────────────────────┐
│  이전블록 │   병합된 가용 블록   │
│ alloc=1  │       48B free       │
└──────────┴──────────────────────┘
                ↑ bp 반환

PUT(HDRP(bp), PACK(48, 0))  ← 현재 블록 헤더 갱신
PUT(FTRP(bp), PACK(48, 0))  ← FTRP가 병합된 블록 끝을 자동으로 가리킴
```

---

### Case 3 — 앞 free, 뒤 alloc : 앞 블록과 병합

```c
size += GET_SIZE(HDRP(PREV_BLKP(bp)));
PUT(FTRP(bp),            PACK(size, 0));
PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
bp = PREV_BLKP(bp);
```

```
Before:
┌──────────┬──────────┬──────────┐
│  이전블록 │  현재 bp │  다음블록 │
│ 16B free │ 32B free │ alloc=1  │
└──────────┴──────────┴──────────┘
                ↑ bp

After: size = 16+32 = 48B
┌──────────────────────┬──────────┐
│   병합된 가용 블록   │  다음블록 │
│       48B free       │ alloc=1  │
└──────────────────────┴──────────┘
  ↑ bp (PREV_BLKP으로 이동)

PUT(FTRP(bp),            PACK(48, 0))  ← 현재 블록 풋터 갱신
PUT(HDRP(PREV_BLKP(bp)), PACK(48, 0)) ← 이전 블록 헤더 갱신
bp = PREV_BLKP(bp)                     ← bp를 앞 블록 시작으로 이동
```

---

### Case 4 — 앞 free, 뒤 free : 앞뒤 모두 병합

```c
size += GET_SIZE(HDRP(PREV_BLKP(bp)))
      + GET_SIZE(FTRP(NEXT_BLKP(bp)));
PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
bp = PREV_BLKP(bp);
```

```
Before:
┌──────────┬──────────┬──────────┐
│  이전블록 │  현재 bp │  다음블록 │
│ 16B free │ 32B free │ 16B free │
└──────────┴──────────┴──────────┘
                ↑ bp

After: size = 16+32+16 = 64B
┌──────────────────────────────────┐
│       병합된 가용 블록           │
│           64B free               │
└──────────────────────────────────┘
  ↑ bp (PREV_BLKP으로 이동)

PUT(HDRP(PREV_BLKP(bp)), PACK(64, 0))  ← 이전 블록 헤더 갱신
PUT(FTRP(NEXT_BLKP(bp)), PACK(64, 0))  ← 다음 블록 풋터 갱신
bp = PREV_BLKP(bp)                      ← bp를 앞 블록 시작으로 이동
```

---

## 4가지 케이스 비교표

| 케이스 | 이전 블록 | 다음 블록 | 병합 방향 | bp 위치 |
|--------|-----------|-----------|-----------|---------|
| Case 1 | alloc=1 | alloc=1 | 없음 | 현재 블록 |
| Case 2 | alloc=1 | alloc=0 | 뒤와 병합 | 현재 블록 |
| Case 3 | alloc=0 | alloc=1 | 앞과 병합 | 이전 블록으로 이동 |
| Case 4 | alloc=0 | alloc=0 | 앞뒤 모두 병합 | 이전 블록으로 이동 |

---

## 풋터가 필요한 이유

```
이전 블록의 상태를 알려면 이전 블록의 크기가 필요합니다.
그런데 헤더는 블록의 맨 앞에 있어서 bp에서 바로 접근할 수 없습니다.

풋터 없을 때:
  bp → PREV_BLKP(bp)를 구할 방법이 없음  ← 이전 블록 크기를 모름

풋터 있을 때:
  bp - DSIZE = 이전 블록의 풋터
  → 풋터에서 이전 블록 크기를 읽어 PREV_BLKP 계산 가능  ✓

  PREV_BLKP(bp) = bp - GET_SIZE(bp - DSIZE)
```

---

## 주요 매크로 정리

| 매크로 | 계산식 | 의미 |
|--------|--------|------|
| `HDRP(bp)` | `bp - WSIZE` | bp 블록의 헤더 주소 |
| `FTRP(bp)` | `bp + GET_SIZE(HDRP(bp)) - DSIZE` | bp 블록의 풋터 주소 |
| `NEXT_BLKP(bp)` | `bp + GET_SIZE(HDRP(bp))` | 다음 블록 페이로드 주소 |
| `PREV_BLKP(bp)` | `bp - GET_SIZE(bp - DSIZE)` | 이전 블록 페이로드 주소 |
| `GET_ALLOC(p)` | `*(p) & 0x1` | 헤더/풋터에서 alloc 비트 추출 |
| `GET_SIZE(p)` | `*(p) & ~0x7` | 헤더/풋터에서 크기 추출 |

---

## 요약

```
mm_free(bp)
  ├─ 1. PUT(HDRP, PACK(size, 0))  → 헤더 alloc=0 으로 변경
  ├─ 2. PUT(FTRP, PACK(size, 0))  → 풋터 alloc=0 으로 변경
  └─ 3. coalesce(bp)              → 인접 블록 병합

coalesce(bp)
  ├─ 앞1 뒤1  Case 1  → 병합 없음,       bp 그대로 반환
  ├─ 앞1 뒤0  Case 2  → 뒤 블록 병합,    bp 그대로 반환
  ├─ 앞0 뒤1  Case 3  → 앞 블록 병합,    bp = PREV_BLKP 반환
  └─ 앞0 뒤0  Case 4  → 앞뒤 모두 병합,  bp = PREV_BLKP 반환
```