# 🧠 Malloc Lab - mm.c 개념 정리

이 페이지는 **Implicit Free List** 방식으로 구현한 동적 메모리 할당기(`mm.c`)의 모든 매크로와 함수를 개념부터 코드까지 상세하게 정리한 문서입니다.

탐색 방식: **Next-Fit** | 정렬 단위: **8바이트 (DSIZE)** | 블록 구조: **Header + Payload + Footer**

---

# 📦 1. 기본 상수와 블록 구조

## 🔢 기본 상수

| 상수 | 값 | 의미 |
|------|-----|------|
| `WSIZE` | 4 | 워드 크기 (바이트). 헤더/풋터 하나의 크기 |
| `DSIZE` | 8 | 더블 워드 크기. 8바이트 정렬 단위 |
| `CHUNKSIZE` | 1024 | 힙을 확장할 때 최소 단위 (1KB) |

```c
#define WSIZE     4         // 헤더/풋터 하나의 크기(4B)
#define DSIZE     8         // 8바이트 정렬 단위
#define CHUNKSIZE (1<<10)   // 힙 확장 최소 크기 = 1024바이트
```

**왜 8바이트 정렬?**
현대 64비트 아키텍처에서 double, pointer 등은 8바이트 경계에 정렬되어야 최적 성능이 나옵니다. 8바이트로 정렬하면 모든 기본 타입을 안전하게 담을 수 있습니다.

## 🧱 블록 메모리 레이아웃

모든 블록은 Header + Payload + Footer 구조입니다.

```
주소 낮음                                   주소 높음
   ┌──────────┬────────────────────┬──────────┐
   │  HEADER  │     PAYLOAD        │  FOOTER  │
   │  (4 byte)│   (사용자 데이터)  │  (4 byte)│
   └──────────┴────────────────────┴──────────┘
        ↑               ↑
   HDRP(bp)            bp (블록 포인터, malloc이 반환하는 주소)
```

- **Header/Footer**: 4바이트. `size | alloc` 비트를 저장
- **bp (block pointer)**: payload 시작 주소. malloc()이 반환하는 주소
- **alloc 비트**: 최하위 비트 (0 = free, 1 = allocated)
- **size 비트**: 상위 29비트에 블록 전체 크기 저장

**헤더 값 예시**: 블록 크기 24바이트, 할당됨 → `24 | 1 = 0x00000019`

## 🏗️ 힙 전체 구조

```
┌─────────┬──────────────────┬──────────────────┬───────────┬─────────┐
│  패딩   │  프롤로그 블록   │  일반 블록들...  │  에필로그 │
│  (4B)  │  H(8,1) F(8,1)  │  H+Payload+F...  │  H(0,1)   │
└─────────┴──────────────────┴──────────────────┴───────────┘
              ↑
          heap_listp (프롤로그 블록 payload 시작, 순회 기준점)
```

- **정렬 패딩**: 더블워드 정렬을 위한 4바이트 더미
- **프롤로그 블록**: 크기 8, 항상 할당됨. 순회 중 경계 검사용
- **에필로그 블록**: 크기 0, 항상 할당됨. 힙 끝 표시
- **heap_listp**: 항상 프롤로그 블록 payload를 가리킴 (순회 시작점)

---

# ⚙️ 2. 매크로 상세 설명

## ALIGN - 8바이트 정렬

```c
#define ALIGN(size) (((size) + (DSIZE-1)) & ~(DSIZE-1))
```

요청한 크기를 8의 배수로 올림(ceiling) 합니다.

**작동 원리 (비트 연산)**:
```
DSIZE - 1 = 7 = 0b00000111
~(DSIZE - 1) = ~7 = 0b11111000  ← 하위 3비트를 0으로 만드는 마스크

예: size = 9
9 + 7 = 16 = 0b00010000
16 & ~7 = 16 & 0b11111000 = 16 ✅

예: size = 17
17 + 7 = 24 = 0b00011000
24 & ~7 = 24 & 0b11111000 = 24 ✅
```

**핵심**: `+7`로 올림 처리 후 `& ~7`로 하위 3비트를 0으로 만들어 8의 배수를 보장합니다.

> mm.c에서는 ALIGN 대신 직접 계산 사용: `DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE)` → 헤더+풋터 오버헤드(DSIZE)를 포함해서 정렬

## MAX - 두 값 중 큰 값

```c
#define MAX(x, y) ((x) > (y) ? (x) : (y))
```

`mm_malloc`에서 힙을 확장할 크기를 결정할 때 사용합니다:

```c
extendsize = MAX(asize, CHUNKSIZE);
// asize가 CHUNKSIZE(1024)보다 크면 asize만큼, 아니면 최소 1024바이트 확장
```

## PACK - 헤더/풋터 값 생성

```c
#define PACK(size, alloc) ((size) | (alloc))
```

블록 크기와 할당 비트를 OR 연산으로 합쳐 헤더/풋터에 저장할 4바이트 값을 만듭니다.

```
size = 24 = 0x00000018
alloc = 1  = 0x00000001
PACK(24, 1) = 0x00000019  ← 이 값을 헤더/풋터에 씁니다
```

**가능한 이유**: 블록 크기는 항상 8의 배수 → 하위 3비트는 항상 0 → 하위 3비트를 플래그로 재사용 가능!

## GET / PUT - 메모리 읽기/쓰기

```c
#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
```

- `GET(p)`: 주소 p를 `unsigned int *`로 캐스팅해 4바이트를 읽음
- `PUT(p, val)`: 주소 p에 4바이트 값 val을 씀

**왜 unsigned int?** 헤더/풋터는 부호 없는 4바이트 정수. 부호 있는 int를 쓰면 sign extension 문제가 생길 수 있습니다.

---

# 🔍 3. 크기/할당 비트 추출

## GET_SIZE - 블록 크기 추출

```c
#define GET_SIZE(p) (GET(p) & ~0x7)
```

```
0x7  = 0b00000111
~0x7 = 0b11111000  ← 하위 3비트를 0으로 만드는 마스크

예: 헤더 값 0x19 (24바이트, 할당됨)
0x19 = 0b00011001
0b00011001 & 0b11111000 = 0b00011000 = 24 ✅
```

하위 3비트(플래그 비트들)를 제거하고 순수한 크기만 추출합니다.

## GET_ALLOC - 할당 비트 추출

```c
#define GET_ALLOC(p) (GET(p) & 0x1)
```

```
0x1 = 0b00000001  ← 최하위 비트만 남기는 마스크

헤더 값 0x19 (할당됨):
0b00011001 & 0b00000001 = 1 ✅ (할당됨)

헤더 값 0x18 (가용):
0b00011000 & 0b00000001 = 0 ✅ (가용)
```

---

# 🗺️ 4. 힙 순회 매크로

## HDRP - 헤더 주소 계산

```c
#define HDRP(bp) ((char *)(bp) - WSIZE)
```

```
bp (payload 시작)에서 4바이트 뒤로 → 헤더 위치

메모리:  [HEADER(4B)] [PAYLOAD...] [FOOTER(4B)]
주소:    HDRP(bp)      bp
```

## FTRP - 풋터 주소 계산

```c
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
```

```
풋터 = bp + (블록 전체 크기) - 8

예: bp = 0x100, 블록 크기 = 24
풋터 주소 = 0x100 + 24 - 8 = 0x118

메모리:  [HDR(4B)] [PAYLOAD(16B)] [FTR(4B)]
주소:    0x0fc      0x100          0x118
```

**핵심**: FTRP는 HDRP(bp)에서 크기를 읽어 계산 → 헤더가 정확해야 FTRP도 정확!

## NEXT_BLKP - 다음 블록으로 이동

```c
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
```

```
다음 블록 bp = 현재 bp + 현재 블록 크기

현재 bp = 0x100, 현재 블록 크기 = 24
다음 bp = 0x100 + 24 = 0x118
```

## PREV_BLKP - 이전 블록으로 이동

```c
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))
```

```
이전 블록 bp = 현재 bp - 이전 블록 풋터의 크기

(char *)(bp) - DSIZE = 현재 블록 바로 앞 = 이전 블록의 풋터
GET_SIZE(이전 블록 풋터) = 이전 블록 전체 크기
이전 bp = 현재 bp - 이전 블록 크기
```

**풋터가 필요한 이유**: 이전 블록으로 O(1) 역방향 탐색을 가능하게 합니다. (풋터 없으면 역방향 탐색 O(n))

---

# 🚀 5. mm_init - 힙 초기화

```c
int mm_init(void) {
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1)
        return -1;

    PUT(heap_listp, 0);                         // 정렬 패딩 (4B)
    PUT(heap_listp + WSIZE,   PACK(DSIZE, 1));  // 프롤로그 헤더
    PUT(heap_listp + 2*WSIZE, PACK(DSIZE, 1));  // 프롤로그 풋터
    PUT(heap_listp + 3*WSIZE, PACK(0, 1));      // 에필로그 헤더

    heap_listp += 2*WSIZE;  // 프롤로그 payload를 가리키도록
    rover = heap_listp;     // next-fit 시작 위치

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)   // 첫 가용 블록 생성
        return -1;
    return 0;
}
```

**초기화 순서**:
1. `mem_sbrk(16B)`: 패딩(4) + 프롤로그(8) + 에필로그(4) = 16바이트 할당
2. 패딩, 프롤로그, 에필로그 설정
3. `heap_listp`를 프롤로그 payload로 설정
4. `extend_heap(256 words = 1024B)` 호출해 첫 가용 블록 생성

**프롤로그와 에필로그의 역할**: 힙 순회 시 `prev_alloc`/`next_alloc` 체크에서 경계 조건(boundary condition) 예외 처리를 없애줍니다.

> `extend_heap`에 `CHUNKSIZE/WSIZE`를 넘기는 이유: extend_heap 내부에서 `words * WSIZE`로 바이트로 변환하기 때문.

---

# 📈 6. extend_heap - 힙 확장

```c
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    // 더블워드 정렬: 홀수 words면 +1해서 짝수 (8바이트 배수)로 맞춤
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((bp = mem_sbrk(size)) == (void*)-1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));           // 새 가용 블록 헤더
    PUT(FTRP(bp), PACK(size, 0));           // 새 가용 블록 풋터
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 새 에필로그 헤더

    return coalesce(bp); // 인접 블록과 병합 시도
}
```

**흐름**:
1. words를 8바이트 배수로 맞춤 (홀수면 +1)
2. `mem_sbrk(size)`: OS에 메모리 요청. 기존 에필로그 위치에서 시작
3. 새 가용 블록의 헤더/풋터 설정
4. 에필로그 헤더를 힙 끝으로 이동
5. `coalesce(bp)`: 이전에 해제된 블록이 있으면 병합

**왜 마지막에 coalesce?**: extend_heap 이전에 힙 끝 블록이 가용 상태였다면 병합해야 단편화를 줄일 수 있습니다.

---

# 🔎 7. find_fit - Next-Fit 탐색

```c
// first-fit: 앞에서부터 처음 맞는 블록 반환 (주석 처리됨)
// next-fit: 마지막 탐색 위치(rover)부터 시작

static void *find_fit(size_t asize) {
    void *bp;

    // 루프 1: rover부터 힙 끝까지 탐색
    for (bp = rover; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            rover = bp;
            return bp;
        }
    }

    // 루프 2: 처음부터 rover까지 탐색 (wrap-around)
    for (bp = heap_listp; bp < (void *)rover; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            rover = bp;
            return bp;
        }
    }

    return NULL; // 실패 → mm_malloc이 extend_heap 호출
}
```

**탐색 전략 비교**:

| 전략 | 방식 | 장점 | 단점 |
|------|------|------|------|
| First-Fit | 항상 처음부터 탐색 | 구현 단순 | 앞쪽 단편화 심화 |
| **Next-Fit** | **마지막 위치부터 탐색** | **단편화 분산, 속도 빠름** | rover 관리 필요 |
| Best-Fit | 전체 탐색 후 최적 선택 | 단편화 최소 | 탐색 느림 O(n) |

**rover의 역할**: 마지막으로 할당에 성공한 블록을 기억. 다음 탐색은 그 위치부터 시작해 불필요한 재탐색을 줄입니다.

---

# 📍 8. place - 블록 배치와 분할

```c
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp)); // 찾은 블록의 실제 크기

    if ((csize - asize) >= (2*DSIZE)) {  // 남는 공간 >= 16B(최소 블록)
        // 분할: 앞부분은 할당, 뒷부분은 새 가용 블록
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    } else {
        // 분할 불가: 통째로 할당 (내부 단편화 발생)
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}
```

**분할 조건**: 남는 공간이 `2*DSIZE = 16바이트` 이상이어야 분할 가능.

**최소 블록 크기가 16바이트인 이유**: 헤더(4) + 페이로드(최소 1 → 실제 8) + 풋터(4) = 16바이트

```
분할 전:
[HDR(csize,0)] [...가용 공간...] [FTR(csize,0)]

분할 후:
[HDR(asize,1)] [PAYLOAD(asize-8B)] [FTR(asize,1)]  | [HDR(csize-asize,0)] [...남은 공간...] [FTR(csize-asize,0)]
```

---

# 🔗 9. coalesce - 블록 병합

```c
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록 alloc
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록 alloc
    size_t size = GET_SIZE(HDRP(bp));                    // 현재 블록 크기

    if (prev_alloc && next_alloc) {        /* Case 1: 앞뒤 모두 할당 */
        // 병합 없음

    } else if (prev_alloc && !next_alloc) { /* Case 2: 뒤만 가용 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));       // FTRP가 자동으로 새 끝을 가리킴
        if (rover > (char *)bp && rover < (char *)NEXT_BLKP(bp))
            rover = bp;

    } else if (!prev_alloc && next_alloc) { /* Case 3: 앞만 가용 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        if (rover >= (char *)bp && rover < (char *)NEXT_BLKP(bp))
            rover = bp;

    } else {                               /* Case 4: 앞뒤 모두 가용 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        if (rover >= (char *)bp && rover < (char *)NEXT_BLKP(bp))
            rover = bp;
    }
    return bp;
}
```

**4가지 케이스 시각화**:

```
Case 1: [할당][현재(해제)][할당]  → 변화 없음

Case 2: [할당][현재][가용]       → [할당][병합된 큰 가용 블록]

Case 3: [가용][현재][할당]       → [병합된 큰 가용 블록][할당]

Case 4: [가용][현재][가용]       → [병합된 큰 가용 블록]
```

**rover 업데이트 이유**: 병합으로 인해 rover가 가리키는 블록이 사라질 수 있습니다. rover가 병합 범위 안에 있으면 새 병합 블록의 시작으로 리셋합니다.

**즉시 병합 (Immediate Coalescing)**: 블록이 해제되는 즉시 coalesce 호출. 지연 병합(Deferred)보다 구현이 단순하고 항상 최대 병합 상태를 유지합니다.

---

# 🎯 10. mm_malloc, mm_free, mm_realloc

## mm_malloc - 메모리 할당

```c
void *mm_malloc(size_t size) {
    size_t asize;      // 조정된 블록 크기
    size_t extendsize; // 힙 확장 크기
    char *bp;

    if (size == 0) return NULL;

    // 크기 조정: 헤더+풋터 포함, 8바이트 정렬
    if (size <= DSIZE)
        asize = 2 * DSIZE;   // 최소 16바이트
    else
        asize = DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL) {   // 가용 블록 탐색
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);     // 힙 확장
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}
```

**asize 계산 상세**:
- `size ≤ 8`: asize = 16 (헤더 4 + 최소 페이로드 8 + 풋터 4)
- `size > 8`: `8 * ceil((size + 8) / 8)` → 오버헤드(헤더+풋터=8B) 포함해서 올림

**실행 흐름**:
```
mm_malloc(size)
    ↓
asize 계산 (정렬 + 오버헤드 포함)
    ↓
find_fit(asize) → 성공: place() → return bp
    ↓ 실패
extend_heap(MAX(asize, CHUNKSIZE))
    ↓
place() → return bp
```

## mm_free - 메모리 해제

```c
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));   // 헤더 alloc 비트 0으로
    PUT(FTRP(bp), PACK(size, 0));   // 풋터 alloc 비트 0으로
    coalesce(bp);                    // 즉시 인접 블록 병합
}
```

간단하지만 강력합니다: 헤더/풋터의 alloc 비트를 0으로 바꾸고 즉시 병합.

## mm_realloc - 크기 재조정

```c
void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) return mm_malloc(size);
    if (size == 0) { mm_free(ptr); return NULL; }

    // asize 계산 (mm_malloc과 동일)
    size_t asize = (size <= DSIZE) ? 2*DSIZE : DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE);
    size_t old_size = GET_SIZE(HDRP(ptr));
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));

    // 전략 1: 다음 블록이 가용이고 합치면 충분한 경우 → 제자리 확장
    if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) && old_size + next_size >= asize) {
        size_t total = old_size + next_size;
        PUT(HDRP(ptr), PACK(total, 1));
        PUT(FTRP(ptr), PACK(total, 1));
        return ptr;
    }

    // 전략 2: 다음이 에필로그 (힙 끝) → 힙만 늘리기
    if (next_size == 0) {
        size_t extend = asize - old_size;
        if (mem_sbrk(extend) != (void *)-1) {
            PUT(HDRP(ptr), PACK(old_size + extend, 1));
            PUT(FTRP(ptr), PACK(old_size + extend, 1));
            PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1)); // 새 에필로그
            return ptr;
        }
    }

    // 전략 3: 새 블록 할당 후 복사 (fallback)
    void *new_bp = mm_malloc(size);
    if (new_bp == NULL) return NULL;
    size_t copy_size = old_size - WSIZE; // 헤더 제외 payload 크기
    if (size < copy_size) copy_size = size;
    memcpy(new_bp, ptr, copy_size);
    mm_free(ptr);
    return new_bp;
}
```

**3가지 전략 (성능 최적화 순)**:

| 전략 | 조건 | 복사 여부 | 효율 |
|------|------|----------|------|
| 제자리 확장 | 다음 블록이 가용 + 합치면 충분 | 불필요 | ⭐⭐⭐ |
| 힙 끝 확장 | 다음이 에필로그 | 불필요 | ⭐⭐⭐ |
| fallback | 위 둘 다 불가 | 필요 (memcpy) | ⭐ |

---

# 🔑 전체 흐름 요약

```
mm_malloc(32)
  → asize = 40 (32 + 8 오버헤드, 8 배수)
  → find_fit(40): rover부터 탐색 → 40B 이상 가용 블록 찾기
  → 없으면 extend_heap(1024/4=256 words) → coalesce → 새 1024B 블록
  → place(bp, 40): 40B 할당 + 나머지 984B 분할 후 가용
  → return bp

mm_free(bp)
  → alloc 비트 0으로
  → coalesce: 인접 가용 블록과 병합

mm_realloc(ptr, 64)
  → asize = 72
  → 다음 블록 확인 → 제자리 확장 or fallback
```

**핵심 설계 원칙**:
- 모든 블록은 8바이트 정렬 → 하위 3비트를 플래그로 재사용
- 헤더+풋터 구조 → O(1) 양방향 인접 블록 접근
- Next-Fit → First-Fit 대비 단편화 분산
- 즉시 병합 → 항상 최대 크기 가용 블록 유지
