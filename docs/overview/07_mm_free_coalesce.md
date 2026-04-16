# 🗑️ mm_free + coalesce — 해제 & 4-way 병합

> `free(bp)` 호출 시 블록을 가용 상태로 전환하고,
> 인접한 가용 블록들과 즉시 병합(Immediate Coalescing)합니다.

---

## mm_free 코드

```c
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));  // 헤더 alloc 비트 0으로
    PUT(FTRP(bp), PACK(size, 0));  // 풋터 alloc 비트 0으로
    coalesce(bp);                   // 인접 블록 병합
}
```

---

## coalesce 코드

```c
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));  // 이전 블록 상태
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));  // 다음 블록 상태
    size_t size = GET_SIZE(HDRP(bp));

    /* Case 1: 앞 할당, 뒤 할당 → 병합 없음 */
    if (prev_alloc && next_alloc) { }

    /* Case 2: 앞 할당, 뒤 가용 → 뒤와 병합 */
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        if (rover > (char *)bp && rover < (char *)NEXT_BLKP(bp))
            rover = bp;
    }

    /* Case 3: 앞 가용, 뒤 할당 → 앞과 병합 */
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        if (rover >= (char *)bp && rover < (char *)NEXT_BLKP(bp))
            rover = bp;
    }

    /* Case 4: 앞 가용, 뒤 가용 → 앞뒤 모두 병합 */
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))
              + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        if (rover >= (char *)bp && rover < (char *)NEXT_BLKP(bp))
            rover = bp;
    }

    return bp;
}
```

---

## 4가지 Case 시각화

### Case 1 — 앞 할당 / 뒤 할당 (병합 없음)

```
[이전:할당] [현재:해제] [다음:할당]

─────────────────────────────────────────
│ HDR(A,1) │ HDR(B,0) │ HDR(C,1) │
│ PAYLOAD  │ PAYLOAD  │ PAYLOAD  │
│ FTR(A,1) │ FTR(B,0) │ FTR(C,1) │
─────────────────────────────────────────

결과: 현재 블록만 가용 상태 유지, 병합 없음
      return bp (변화 없음)
```

---

### Case 2 — 앞 할당 / 뒤 가용 (뒤와 병합)

```
[이전:할당] [현재:해제] [다음:가용]

before:
─────────────────────────────────────────────
│ HDR(A,1) │ HDR(B,0) │ HDR(C,0) │
│ PAYLOAD  │ PAYLOAD  │ PAYLOAD  │
│ FTR(A,1) │ FTR(B,0) │ FTR(C,0) │
─────────────────────────────────────────────
                ↑bp

after: B+C 병합 (size = B+C)
─────────────────────────────────────────────
│ HDR(A,1) │ HDR(B+C,0)          │
│ PAYLOAD  │                      │
│ FTR(A,1) │ FTR(B+C,0)          │
─────────────────────────────────────────────
                ↑bp (변화 없음)

코드:
  size += GET_SIZE(HDRP(NEXT_BLKP(bp)));  // size = B + C
  PUT(HDRP(bp), PACK(size, 0));           // 현재 헤더 갱신
  PUT(FTRP(bp), PACK(size, 0));           // FTRP가 자동으로 C의 풋터 위치 계산
```

---

### Case 3 — 앞 가용 / 뒤 할당 (앞과 병합)

```
[이전:가용] [현재:해제] [다음:할당]

before:
─────────────────────────────────────────────
│ HDR(A,0) │ HDR(B,0) │ HDR(C,1) │
│ PAYLOAD  │ PAYLOAD  │ PAYLOAD  │
│ FTR(A,0) │ FTR(B,0) │ FTR(C,1) │
─────────────────────────────────────────────
                ↑bp

after: A+B 병합 (size = A+B)
─────────────────────────────────────────────
│ HDR(A+B,0)          │ HDR(C,1) │
│                      │ PAYLOAD  │
│ FTR(A+B,0)          │ FTR(C,1) │
─────────────────────────────────────────────
  ↑bp = PREV_BLKP(bp) (이전 블록으로 이동)

코드:
  size += GET_SIZE(HDRP(PREV_BLKP(bp)));  // size = A + B
  PUT(FTRP(bp), PACK(size, 0));           // 현재 블록의 풋터 갱신 (B의 풋터)
  PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블록의 헤더 갱신 (A의 헤더)
  bp = PREV_BLKP(bp);                     // bp를 병합된 블록 시작으로 이동
```

---

### Case 4 — 앞 가용 / 뒤 가용 (앞뒤 모두 병합)

```
[이전:가용] [현재:해제] [다음:가용]

before:
──────────────────────────────────────────────────────
│ HDR(A,0) │ HDR(B,0) │ HDR(C,0) │
│ PAYLOAD  │ PAYLOAD  │ PAYLOAD  │
│ FTR(A,0) │ FTR(B,0) │ FTR(C,0) │
──────────────────────────────────────────────────────
                ↑bp

after: A+B+C 병합 (size = A+B+C)
──────────────────────────────────────────────────────
│ HDR(A+B+C,0)                          │
│                                        │
│ FTR(A+B+C,0)                          │
──────────────────────────────────────────────────────
  ↑bp = PREV_BLKP(bp)

코드:
  size += GET_SIZE(HDRP(PREV_BLKP(bp)))  // size = A+B
        + GET_SIZE(HDRP(NEXT_BLKP(bp))); // size = A+B+C
  PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // A의 헤더 갱신
  PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // C의 풋터 갱신
  bp = PREV_BLKP(bp);
```

---

## Boundary Tag가 필요한 이유

```
Case 3, 4에서 이전 블록의 크기를 알아야 함:
  PREV_BLKP(bp) = bp - GET_SIZE(bp - DSIZE)
                = bp - GET_SIZE(이전블록의 FTR)

→ 이전 블록의 FTR(= Footer = Boundary Tag)이 없으면
  이전 블록의 크기를 O(1)에 알 수 없음!
  전체 힙을 처음부터 순회해야 함 → O(n) 비효율

Footer가 있으면 모든 Case를 O(1)에 처리 가능 ✓
```

---

## rover 보정 (Next-fit 연동)

```
병합 후 rover가 병합된 영역 내부를 가리키면
그 주소는 더 이상 유효한 블록 경계가 아님!

→ 병합된 블록의 시작(bp)으로 rover 리셋
→ find_fit이 올바른 블록 경계에서 탐색하도록 보장

if (rover >= bp && rover < NEXT_BLKP(bp))
    rover = bp;
```

---

## coalesce 호출 시점

| 호출 위치 | 이유 |
|-----------|------|
| `mm_free` | 블록 해제 즉시 병합 (Immediate Coalescing) |
| `extend_heap` | 새 가용 블록이 직전 가용 블록과 인접할 수 있음 |
