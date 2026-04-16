# `mm_realloc()` 개선 — naive 복사 → 제자리 확장

---

## ① 기존 방식 (naive 복사)

```c
void *mm_realloc(void *ptr, size_t size)
{
    void *new_bp = mm_malloc(size);
    if (new_bp == NULL)
        return NULL;

    size_t copy_size = GET_SIZE(HDRP(ptr)) - DSIZE;
    if (size < copy_size)
        copy_size = size;

    memcpy(new_bp, ptr, copy_size);
    mm_free(ptr);
    return new_bp;
}
```

### 동작 방식

```
realloc(ptr, new_size) 호출 시:

  [기존 블록 ptr]   [새 블록 new_bp]
  ┌────────────┐   ┌────────────────┐
  │   data     │ → │   data (복사)  │
  └────────────┘   └────────────────┘
        ↑                  ↑
     mm_free()          mm_malloc()
     (해제됨)           (새로 할당)
```

### 문제점

realloc을 호출할 때마다:
1. 새 블록 할당 (`mm_malloc`)
2. 데이터 복사 (`memcpy`)
3. 기존 블록 해제 (`mm_free`)

인접한 free 블록이 있어도 활용하지 못하고 힙 끝에 새 블록을 계속 추가합니다.
힙 공간이 빠르게 소진됩니다.

---

## ② 개선 방식 (제자리 확장 우선)

### 핵심 아이디어

```
realloc(ptr, new_size) 호출 시:

  상황 1: 다음 블록이 free → 합쳐서 제자리 확장
  ┌────────────┬──────────────┐
  │   ptr      │  next(free)  │  →  ┌──────────────────────┐
  └────────────┴──────────────┘     │  ptr (확장됨, 복사 X) │
                                    └──────────────────────┘

  상황 2: 다음 블록이 epilogue → 힙만 살짝 늘리기
  ┌────────────┬─────────┐
  │   ptr      │ epilogue│  →  ┌────────────┬─────┬─────────┐
  └────────────┴─────────┘     │   ptr      │확장 │ epilogue│
                                └────────────┴─────┴─────────┘

  상황 3: 둘 다 불가능 → 기존 방식 fallback
```

---

## 변경 사항

### 변경 전 — 항상 malloc + memcpy + free

```c
void *new_bp = mm_malloc(size);   // 항상 새 블록 할당
...
memcpy(new_bp, ptr, copy_size);
mm_free(ptr);
return new_bp;
```

### 변경 후 — 제자리 가능 여부 먼저 확인

```c
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) return mm_malloc(size);
    if (size == 0)  { mm_free(ptr); return NULL; }

    size_t old_size  = GET_SIZE(HDRP(ptr));
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
        return ptr;                      // memcpy 없이 반환
    }

    // 제자리 확장 ②: 다음 블록이 epilogue → 힙만 늘리기
    if (next_size == 0) {
        size_t extend = asize - old_size;
        if (mem_sbrk(extend) != (void *)-1) {
            PUT(HDRP(ptr), PACK(old_size + extend, 1));
            PUT(FTRP(ptr), PACK(old_size + extend, 1));
            PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));  // 새 epilogue
            return ptr;                              // memcpy 없이 반환
        }
    }

    // fallback: 새 블록 할당 후 복사
    void *new_bp = mm_malloc(size);
    if (new_bp == NULL) return NULL;

    size_t copy_size = old_size - WSIZE;
    if (size < copy_size) copy_size = size;
    memcpy(new_bp, ptr, copy_size);
    mm_free(ptr);
    return new_bp;
}
```

---

## 제자리 확장 ① 상세

```
조건:  !GET_ALLOC(HDRP(NEXT_BLKP(ptr)))   // 다음 블록이 free
    && old_size + next_size >= asize        // 합치면 충분

Before:
  ┌──────┬────────────┬──────┬──────────────┬──────┐
  │ HDR  │  payload   │ FTR  │ next(free)   │ ...  │
  │ old  │            │ old  │              │      │
  └──────┴────────────┴──────┴──────────────┴──────┘
     ↑ ptr-4    ↑ ptr                ↑ NEXT_BLKP(ptr)

After:
  ┌──────┬────────────────────────────────┬──────┐
  │ HDR  │         payload (확장)          │ FTR  │
  │total │                                │total │
  └──────┴────────────────────────────────┴──────┘
```

- `total = old_size + next_size`
- 헤더/풋터만 갱신, 데이터 복사 없음
- `ptr` 그대로 반환

---

## 제자리 확장 ② 상세

```
조건: GET_SIZE(HDRP(NEXT_BLKP(ptr))) == 0   // 다음이 epilogue (힙 끝)

Before:
  ┌──────┬────────────┬──────┬───────┐
  │ HDR  │  payload   │ FTR  │ epilo │
  │ old  │            │ old  │ gue   │
  └──────┴────────────┴──────┴───────┘

mem_sbrk(extend) 호출 → 힙 확장

After:
  ┌──────┬──────────────────────┬──────┬───────┐
  │ HDR  │  payload (확장)       │ FTR  │ epilo │
  │ new  │                      │ new  │ gue   │
  └──────┴──────────────────────┴──────┴───────┘
```

- `extend = asize - old_size` (부족한 만큼만 확장)
- 힙 끝에 새 epilogue 설정
- 데이터 복사 없음

---

## asize 계산이 필요한 이유

```c
// 잘못된 방식 (raw size 사용)
if (old_size + next_size >= size + DSIZE)   // size가 정렬 안 됨
    extend = size + DSIZE - old_size;       // 블록 크기가 8B 정렬 아님

// 올바른 방식 (asize로 정렬 후 사용)
size_t asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
if (old_size + next_size >= asize)          // 정렬된 크기로 비교
    extend = asize - old_size;             // 정렬된 크기만큼 확장
```

raw `size`를 그대로 쓰면 블록 크기가 8바이트 정렬이 안 되어
`NEXT_BLKP` 계산이 어긋나고 힙 구조가 붕괴됩니다.

---

## 실행 흐름 요약

```
mm_realloc(ptr, size)
  │
  ├─ ptr == NULL  → mm_malloc(size)
  ├─ size == 0    → mm_free(ptr), return NULL
  │
  ├─ asize 계산 (8B 정렬)
  │
  ├─ [제자리 ①] 다음 블록 free + 합치면 asize 이상
  │      → PUT header/footer → return ptr  (복사 없음)
  │
  ├─ [제자리 ②] 다음 블록 = epilogue
  │      → mem_sbrk(asize - old_size)
  │      → PUT header/footer/new epilogue → return ptr  (복사 없음)
  │
  └─ [fallback] 위 두 경우 모두 불가
         → mm_malloc → memcpy → mm_free → return new_bp
```

---

## 성능 비교

| 항목 | 기존 (naive 복사) | 개선 (제자리 확장) |
|------|------------------|------------------|
| 항상 새 블록 할당 | O | X (제자리 가능 시) |
| memcpy 비용 | 매번 발생 | 제자리 시 없음 |
| 힙 소비 | 빠르게 증가 | 필요한 만큼만 |
| trace 10 util | 34% | 82% (+48%) |
| trace 9 util | 27% | 27% (인접 free 없어 미효과) |

---

## 요약

```
naive 방식
  └─ mm_malloc → memcpy → mm_free  (항상)

제자리 확장 방식
  ├─ 다음 블록 free?    → 합쳐서 return ptr      (복사 없음)
  ├─ 다음 블록 epilogue? → 힙 늘려서 return ptr   (복사 없음)
  └─ 둘 다 불가         → malloc + memcpy + free (fallback)
```
