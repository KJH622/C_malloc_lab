# 🧩 mm_malloc — 메모리 할당

> `malloc(size)`의 핵심 함수.
> 요청 크기에 맞는 블록을 찾고, 없으면 힙을 늘려서 할당합니다.

---

## 코드

```c
void *mm_malloc(size_t size)
{
    size_t asize;       // 정렬된 실제 할당 크기
    size_t extendsize;  // 힙 확장 크기
    char *bp;

    // ① 크기 0 요청 무시
    if (size == 0)
        return NULL;

    // ② 크기 조정 (헤더+풋터 포함, 8바이트 정렬)
    if (size <= DSIZE)
        asize = 2 * DSIZE;      // 최소 16B
    else
        asize = DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE);

    // ③ 가용 블록 탐색
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // ④ 탐색 실패 → 힙 확장
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;
}
```

---

## ② asize 계산 — 핵심 로직

```
사용자가 요청한 size에 헤더(4B) + 풋터(4B) = 8B를 더하고
8의 배수로 올림해야 합니다.

공식: asize = DSIZE * ceil((size + DSIZE) / DSIZE)
     = 8 * ceil((size + 8) / 8)

구현: DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE)
     여기서 (DSIZE-1)을 더하는 이유 = 올림 나눗셈 트릭
     (C의 정수 나눗셈은 내림이므로, 분자에 (분모-1)을 더해 올림 효과)
```

### 계산 예시

```
size=1:   1 ≤ 8  → asize = 16   (최소 블록)
size=8:   8 ≤ 8  → asize = 16   (최소 블록)
size=9:   9 > 8  → 8 * ceil(17/8) = 8 * 3 = 24
size=16:  → 8 * ceil(24/8) = 8 * 3 = 24
size=17:  → 8 * ceil(25/8) = 8 * 4 = 32
size=100: → 8 * ceil(108/8) = 8 * 14 = 112
```

### 왜 최소 16B?

```
┌─────────┬──────────┬─────────┐
│ HDR(4B) │ 페이로드  │ FTR(4B) │
│         │ 최소 8B  │         │
└─────────┴──────────┴─────────┘
    4   +     8    +    4   = 16B

페이로드가 8B인 이유: 8바이트 정렬 단위이므로 최소 8B 보장
```

---

## ③ ④ 전체 흐름 시각화

```
mm_malloc(100)
    ↓
asize = 112

find_fit(112) 탐색
    ├─ 112B 이상 가용 블록 발견 (bp = 0x5000)
    │       ↓
    │   place(0x5000, 112)
    │       ↓
    │   return 0x5000  ✓
    │
    └─ 가용 블록 없음
            ↓
        extendsize = MAX(112, 1024) = 1024
        extend_heap(1024/4 = 256 words)
            ↓
        mem_sbrk(1024) → 힙 1KB 확장
            ↓
        place(bp, 112)  [1024B 블록에서 112B 할당, 912B 가용으로 분할]
            ↓
        return bp  ✓
```

---

## extendsize = MAX(asize, CHUNKSIZE) 의미

```
asize = 112, CHUNKSIZE = 1024

→ extendsize = MAX(112, 1024) = 1024

이유: 매번 딱 필요한 만큼만 늘리면
     시스템 콜(mem_sbrk) 호출이 너무 잦아짐 → 성능 저하

최소 CHUNKSIZE(1KB) 단위로 늘려서
→ 다음 malloc 요청도 추가 확장 없이 처리 가능 (지역성 활용)
```

---

## 반환값

| 반환값 | 조건 |
|--------|------|
| `bp` | 할당된 payload의 시작 주소 |
| `NULL` | `size == 0` 또는 `extend_heap` 실패 |

---

## 사용자 관점

```c
// 사용자 코드
int *arr = (int *)mm_malloc(40);  // 40바이트 요청

// 내부 처리:
// size=40 → asize=48 (8*ceil(48/8)=8*6=48)
// find_fit(48) → 가용 블록 찾기
// place(bp, 48) → 헤더/풋터에 alloc=1 기록
// return bp  →  arr는 payload 시작 주소

arr[0] = 1;  // bp+0 에 쓰기
arr[9] = 9;  // bp+36 에 쓰기 (40바이트 내)
```
