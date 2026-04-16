# 📌 매크로 & 상수 완전 분석

> mm.c의 모든 `#define`을 시각적으로 이해하기.
> 이 매크로들이 전체 코드의 뼈대입니다.

---

## 1. 기본 상수

```c
#define WSIZE     4          // Word size: 헤더/풋터 1개 크기 (4바이트)
#define DSIZE     8          // Double word: 정렬 단위 (8바이트)
#define CHUNKSIZE (1 << 10)  // 힙 최소 확장 단위 = 1024바이트 (1KB)
```

### 왜 8바이트 정렬인가?

```
64비트 시스템에서 double, pointer = 8바이트
→ 8바이트 경계에 맞춰야 CPU가 한 번에 읽을 수 있음
→ 잘못된 정렬 = 성능 저하 or 하드웨어 예외

예시:
size=9  → ALIGN(9)  = 16  (8의 배수로 올림)
size=16 → ALIGN(16) = 16  (이미 8의 배수)
size=17 → ALIGN(17) = 24
```

---

## 2. 정렬 매크로

```c
#define ALIGN(size) (((size) + (DSIZE-1)) & ~(DSIZE-1))
```

### 동작 원리 (비트 연산)

```
DSIZE = 8 = 0b00001000
DSIZE-1   = 7 = 0b00000111
~(DSIZE-1)    = 0b11111000  ← 하위 3비트를 0으로 만드는 마스크

ALIGN(9):
  9  = 0b00001001
  +7 = 0b00010000  (= 16)
  & ~7 = 0b00010000  = 16 ✓

ALIGN(16):
  16 = 0b00010000
  +7 = 0b00010111  (= 23)
  & ~7 = 0b00010000  = 16 ✓
```

**핵심**: `+7`로 올림 처리 후 `& ~7`로 하위 3비트를 0으로 잘라냄.

---

## 3. 블록 정보 읽기/쓰기

```c
#define PACK(size, alloc)  ((size) | (alloc))
#define GET(p)             (*(unsigned int *)(p))
#define PUT(p, val)        (*(unsigned int *)(p) = val)
```

### PACK: 헤더/풋터 값 생성

```
블록 크기 = 24바이트, 할당됨:
  size  = 24 = 0b...00011000
  alloc =  1 = 0b...00000001
  PACK  = 25 = 0b...00011001  ← OR 연산

나중에 읽을 때:
  GET_SIZE:  25 & ~0x7 = 25 & 0b...11111000 = 24  ✓
  GET_ALLOC: 25 & 0x1  = 1                        ✓
```

### GET/PUT: 4바이트 메모리 접근

```
GET(p):  void* p를 unsigned int*로 캐스팅 → 4바이트 읽기
PUT(p):  void* p를 unsigned int*로 캐스팅 → 4바이트 쓰기

왜 unsigned int? → 4바이트 보장 + 부호 없는 비트 연산
```

---

## 4. 크기/할당 비트 추출

```c
#define GET_SIZE(p)   (GET(p) & ~0x7)   // 하위 3비트 제거 → 크기만 남김
#define GET_ALLOC(p)  (GET(p) & 0x1)    // 최하위 비트만 → alloc 비트
```

```
헤더 값 = 0x00000019 = 25

GET_SIZE:  25 & ~7 = 25 & 0xFFFFFFF8 = 24  (크기)
GET_ALLOC: 25 &  1 = 1                     (할당됨)
```

---

## 5. 블록 포인터 이동 매크로

```c
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))
```

### 메모리 레이아웃으로 이해하기

```
주소:  100   104        124   128
       ┌─────┬──────────┬─────┐
       │ HDR │ PAYLOAD  │ FTR │   ← 블록 크기 = 28B (HDR+PAYLOAD+FTR)
       │ 28|1│ (20바이트)│ 28|1│
       └─────┴──────────┴─────┘
              ↑                ↑
              bp=104           bp+GET_SIZE(HDR)-8 = 104+28-8 = 124

HDRP(bp)       = bp - 4      = 100  ✓ (헤더)
FTRP(bp)       = bp + 28 - 8 = 124  ✓ (풋터)
NEXT_BLKP(bp)  = bp + 28     = 132  ✓ (다음 블록 payload 시작)
PREV_BLKP(bp)  = bp - GET_SIZE(bp-8)
               = bp - 이전블록크기    ✓ (이전 블록 payload 시작)
```

### PREV_BLKP의 동작 원리

```
bp - DSIZE = bp - 8 → 이 주소에는 이전 블록의 FTR이 있음
GET_SIZE(이전 FTR) → 이전 블록의 크기 획득
bp - 이전크기 → 이전 블록의 payload 시작

※ Footer가 있어야 이전 블록을 O(1)에 찾을 수 있음!
  (이것이 Boundary Tag의 핵심 이유)
```

---

## 6. 전역 변수

```c
static char *heap_listp;  // 힙 순회 시작점 (프롤로그 FTR 위치)
static char *rover;       // Next-fit 탐색용: 마지막 탐색 위치
```

| 변수 | 역할 | 초기값 |
|------|------|--------|
| `heap_listp` | 힙 순회 시작. 절대 변하지 않음 | 프롤로그 FTR |
| `rover` | Next-fit 탐색 재개 위치. 계속 바뀜 | `heap_listp` |

---

## 💡 매크로 연쇄 사용 예시

```c
// "현재 블록의 다음 블록이 할당됐는가?"
GET_ALLOC(HDRP(NEXT_BLKP(bp)))

// "이전 블록의 크기는?"
GET_SIZE(HDRP(PREV_BLKP(bp)))

// "다음 블록 헤더에 새 값 쓰기"
PUT(HDRP(NEXT_BLKP(bp)), PACK(size, 0))
```
