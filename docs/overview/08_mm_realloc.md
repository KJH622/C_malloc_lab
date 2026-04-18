# 🔄 mm_realloc — 블록 크기 재조정

> 이미 할당된 블록의 크기를 변경하는 함수.
> 기존 데이터는 유지하면서 크기만 조정합니다.
> 이 구현에는 **제자리 확장 최적화**가 포함되어 있습니다.

---

## 코드

```c
void *mm_realloc(void *ptr, size_t size)
{
    // ① 특수 케이스 처리
    if (ptr == NULL)
        return mm_malloc(size);
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t old_size = GET_SIZE(HDRP(ptr));
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));

    // asize 계산 (mm_malloc과 동일)
    size_t asize;
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE);

    // ② 제자리 확장: 다음 블록이 가용 + 합쳐서 충분한 경우
    if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) && old_size + next_size >= asize) {
        size_t total = old_size + next_size;
        PUT(HDRP(ptr), PACK(total, 1));
        PUT(FTRP(ptr), PACK(total, 1));
        return ptr;
    }

    // ③ 힙 끝 블록: 에필로그 다음이므로 힙만 늘리기
    if (next_size == 0) {
        size_t extend = asize - old_size;
        if (mem_sbrk(extend) != (void *)-1) {
            PUT(HDRP(ptr), PACK(old_size + extend, 1));
            PUT(FTRP(ptr), PACK(old_size + extend, 1));
            PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));  // 새 에필로그
            return ptr;
        }
    }

    // ④ fallback: 새 블록 할당 + 복사 + 해제
    void *new_bp = mm_malloc(size);
    if (new_bp == NULL)
        return NULL;

    size_t copy_size = old_size - WSIZE;  // 페이로드 크기 (풋터 제외 후 헤더 제외)
    if (size < copy_size)
        copy_size = size;
    memcpy(new_bp, ptr, copy_size);
    mm_free(ptr);
    return new_bp;
}
```

---

## ① 특수 케이스

```
realloc(NULL, size)  ≡  malloc(size)
realloc(ptr,  0)     ≡  free(ptr)

C 표준에서 정의된 동작
```

---

## ② 제자리 확장 — 다음 블록 흡수

```
[현재 상황]
┌──────────────────┬──────────────────┐
│   ptr 블록       │  다음 가용 블록   │
│   old_size=48    │  next_size=64    │
│   (할당됨)       │  (가용)          │
└──────────────────┴──────────────────┘

realloc(ptr, 80) 요청 → asize = 88

old_size + next_size = 48 + 64 = 112 ≥ 88  →  제자리 확장 가능!

[결과]
┌────────────────────────────────────────┐
│   ptr 블록                             │
│   total = 112 (할당됨)                 │
└────────────────────────────────────────┘

장점: memcpy 불필요! ptr 주소 그대로 반환 ✓
단점: 남는 공간(112-88=24B)을 분할 안 함 → 약간의 낭비
```

---

## ③ 힙 끝 블록 확장

```
ptr이 힙의 마지막 할당 블록인 경우:
NEXT_BLKP(ptr) = 에필로그 → next_size = 0

[현재]
... │ ptr 블록(48B) │ 에필로그(0,1) │
         ↑
        ptr

realloc(ptr, 80) → asize = 88
extend = 88 - 48 = 40B만 힙 확장

mem_sbrk(40)
→ OS로부터 40B 추가

[결과]
... │ ptr 블록(88B)             │ 새 에필로그(0,1) │

장점: 최소한의 힙 확장. ptr 주소 유지 ✓
```

---

## ④ fallback — 새 블록 할당 + memcpy

```
앞의 두 최적화가 모두 불가능한 경우:
(다음 블록이 할당됐고, 힙 끝도 아닌 경우)

[현재]
... │ ptr 블록(48B) │ 다음 할당 블록 │ ...

realloc(ptr, 200) → asize = 208

→ 제자리 불가, 힙 끝도 아님

1. mm_malloc(200) → 새 공간 확보
2. memcpy(new_bp, ptr, copy_size) → 기존 데이터 복사
3. mm_free(ptr) → 기존 블록 해제

[결과]
new_bp = 새로운 위치의 208B 블록
ptr 위치 = 가용 블록

반환: new_bp (주소가 바뀜!)
```

---

## copy_size 계산 설명

```c
size_t copy_size = old_size - WSIZE;
```

```
왜 WSIZE를 빼나?

old_size = 헤더 + 페이로드 + 풋터 = 4 + payload + 4

사용자가 실제로 쓴 데이터 = payload = old_size - 4(HDR) - 4(FTR)
                                    = old_size - WSIZE  ← 풋터만 제외

실제로는 old_size - DSIZE가 더 정확하지만,
풋터는 페이로드 공간에 겹치지 않으므로 WSIZE 제외도 동작함.

if (size < copy_size) copy_size = size;
→ 새 크기가 더 작으면 새 크기만큼만 복사 (초과 데이터 잘림)
```

---

## 3가지 전략 비교

| 전략 | 조건 | ptr 변화 | 비용 |
|------|------|----------|------|
| 다음 블록 흡수 | 다음이 가용 + 합산 충분 | 유지 ✓ | 매우 낮음 |
| 힙 끝 확장 | 힙의 마지막 블록 | 유지 ✓ | 낮음 (sbrk만) |
| fallback | 위 둘 다 불가 | 변경 ✗ | 높음 (malloc+memcpy+free) |

---

## 전체 흐름 요약

```
realloc(ptr, size)
    │
    ├─ ptr == NULL  →  malloc(size)
    ├─ size == 0   →  free(ptr), return NULL
    │
    ├─ 다음 블록 가용 + 합산 >= asize
    │       →  헤더/풋터만 갱신, return ptr  ✓
    │
    ├─ 다음 블록 = 에필로그 (힙 끝)
    │       →  mem_sbrk(부족분), return ptr  ✓
    │
    └─ fallback
            →  malloc → memcpy → free → return new_bp
```
