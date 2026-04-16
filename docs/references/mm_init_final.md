# `mm_init()` — 힙 초기화 함수

---

## 함수 코드

```c
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1) {
        return -1;
    }

    PUT(heap_listp, 0);                          // 정렬 패딩
    PUT(heap_listp + WSIZE,   PACK(DSIZE, 1));   // 프롤로그 헤더
    PUT(heap_listp + 2*WSIZE, PACK(DSIZE, 1));   // 프롤로그 풋터
    PUT(heap_listp + 3*WSIZE, PACK(0, 1));       // 에필로그 헤더

    heap_listp += 2*WSIZE;  // 프롤로그 풋터를 가리키도록
    rover = heap_listp;

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {  // 첫 가용 블록 생성
        return -1;
    }

    return 0;
}
```

---

## 역할 요약

`mm_init()`은 `malloc` 라이브러리가 사용할 힙을 초기 상태로 세팅하는 함수입니다.
핵심 역할은 딱 두 가지입니다.

1. **경계 구조물 만들기** — 프롤로그 블록 + 에필로그 헤더 (센티넬)
2. **첫 가용 공간 확보** — `extend_heap()`으로 실제 할당에 쓸 공간 생성

---

## `mem_sbrk()` — 힙 확장의 실제 구현

`mm_init`의 첫 번째 호출이 `mem_sbrk`입니다.
이 함수는 `memlib.c`에 정의되어 있으며, OS의 `sbrk()`를 **시뮬레이션**합니다.

### `memlib.c` 내부 구조

```
큰 고정 메모리 풀 (mem_heap)

┌──────────────────────────────────────────────────────────┐
│                    mem_heap                              │
│  ↑                    ↑                            ↑    │
│ mem_start_brk       mem_brk                  mem_max_addr│
│  (힙 시작, 고정)   (현재 힙 끝)               (힙 한계)  │
└──────────────────────────────────────────────────────────┘
```

`mem_start_brk` ~ `mem_brk` 사이가 현재 사용 중인 힙 영역입니다.

### `mem_sbrk()` 코드

```c
void *mem_sbrk(int incr)
{
    char *old_brk = mem_brk;                         // ① 현재 brk 저장

    if ((incr < 0) ||                                // ② 음수 요청 거부
        ((mem_brk + incr) > mem_max_addr)) {         // ③ 한계 초과 거부
        errno = ENOMEM;
        fprintf(stderr, "ERROR: mem_sbrk failed...\n");
        return (void *)-1;                           // ④ 실패 시 -1 반환
    }
    mem_brk += incr;                                 // ⑤ brk를 incr만큼 전진
    return (void *)old_brk;                          // ⑥ 확장 전 주소 반환
}
```

### 핵심 — 반환값은 확장 전 주소

```
호출 전:
[  사용 중인 힙  │          미사용              ]
↑               ↑                              ↑
mem_start_brk  mem_brk(= old_brk)        mem_max_addr

mem_sbrk(16) 호출 후:
[  사용 중인 힙  │  새로 확보(16B) │    미사용    ]
↑               ↑               ↑              ↑
mem_start_brk  old_brk       mem_brk(+16) mem_max_addr
               ↑
           반환값 = old_brk
           → 여기서부터 16B 사용 가능
```

실패 조건 두 가지:

```
incr < 0                          → 음수 요청 (비정상)
mem_brk + incr > mem_max_addr     → 힙 한계 초과 (메모리 부족)
```

---

## 단계별 실행 과정

### Step 1 — `mem_sbrk(4*WSIZE)` : 힙 16바이트 확보

```c
if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1)
    return -1;
```

이 한 줄에 세 가지 동작이 압축되어 있습니다.

```
① mem_sbrk(4 × 4 = 16B) 호출
         ↓
② 반환값(old_brk)을 heap_listp에 대입
         ↓
③ 반환값 == (void*)-1 ?
   YES → return -1  (힙 확보 실패)
   NO  → 계속 진행  (heap_listp = 새로 확보된 16B 시작 주소)
```

```
호출 후 힙 상태 (16B 확보, 아직 아무 값도 없음):

 heap_listp
  ↓
 +0      +4      +8      +12     +16
┌───────┬───────┬───────┬───────┐
│  ??   │  ??   │  ??   │  ??   │
└───────┴───────┴───────┴───────┘
```

---

### Step 2 — `PUT` × 4 : 더미 블록 기록

```c
PUT(heap_listp,           0);               // 정렬 패딩
PUT(heap_listp + WSIZE,   PACK(DSIZE, 1));  // 프롤로그 헤더
PUT(heap_listp + 2*WSIZE, PACK(DSIZE, 1));  // 프롤로그 풋터
PUT(heap_listp + 3*WSIZE, PACK(0, 1));      // 에필로그 헤더
```

```
 heap_listp
  ↓
 +0       +4            +8            +12
┌────────┬─────────────┬─────────────┬─────────────┐
│  0     │ PACK(8,1)=9 │ PACK(8,1)=9 │ PACK(0,1)=1 │
│ 패딩   │  프롤로그H  │  프롤로그F  │  에필로그H  │
└────────┴─────────────┴─────────────┴─────────────┘
```

#### 프롤로그/에필로그가 필요한 이유 (센티넬)

```
← coalesce 왼쪽 탐색     coalesce 오른쪽 탐색 →

┌──────────────────┐  ┌──────────┐  ┌──────────────────┐
│  프롤로그 블록   │  │  가용블록 │  │   에필로그 헤더  │
│  alloc=1         │  │          │  │  size=0, alloc=1  │
│  절대 free 안 됨 │  │          │  │  "끝" 신호       │
└──────────────────┘  └──────────┘  └──────────────────┘
         ↑                                    ↑
    시작 경계 센티넬                      끝 경계 센티넬
```

---

### Step 3 — 포인터 이동

```c
heap_listp += 2*WSIZE;  // 프롤로그 풋터 위치로 이동
rover = heap_listp;     // next-fit 탐색 시작점 초기화
```

```
이동 전:
 +0       +4            +8            +12
┌────────┬─────────────┬─────────────┬─────────────┐
│ 패딩   │  프롤로그H  │  프롤로그F  │  에필로그H  │
└────────┴─────────────┴─────────────┴─────────────┘
↑ heap_listp

이동 후:
┌────────┬─────────────┬─────────────┬─────────────┐
│ 패딩   │  프롤로그H  │  프롤로그F  │  에필로그H  │
└────────┴─────────────┴─────────────┴─────────────┘
                              ↑ heap_listp
                              ↑ rover
```

`heap_listp`가 프롤로그 풋터를 가리켜야 `find_fit`이 `NEXT_BLKP(heap_listp)`로
첫 번째 실제 블록부터 순회를 시작할 수 있습니다.

`rover = heap_listp`는 next-fit 전용입니다. first-fit을 사용한다면 이 줄은 필요 없습니다.

---

### Step 4 — `extend_heap(CHUNKSIZE/WSIZE)` : 첫 가용 블록 생성

```c
if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
    return -1;
```

#### 왜 `CHUNKSIZE/WSIZE`로 나눠서 넘기는가?

```c
static void *extend_heap(size_t words)  // 단위: 워드(4B)
```

`extend_heap`은 바이트가 아닌 **워드 수**를 인자로 받습니다.

```
CHUNKSIZE = 4096B → 4096/4 = 1024 words 전달

extend_heap(4096)  → 내부에서 4096 × 4 = 16384B 확장  ✗
extend_heap(1024)  → 내부에서 1024 × 4 = 4096B  확장  ✓
```

#### `extend_heap` 내부에서 8의 배수 보정

```c
// extend_heap 내부
size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
```

```
words = 1024  (짝수) → 그대로 × 4 = 4096B  ✓
words = 1023  (홀수) → (1023+1) × 4 = 4096B  ✓
```

주석의 `// 8의 배수로 맞춰주기 위해`는 이 보정을 가리킵니다.
정확히는 `extend_heap` 내부에서 처리하는 로직입니다.

#### 호출 후 최종 힙 상태

```
낮은 주소 ─────────────────────────────────────────── 높은 주소

 +0       +4       +8       +12
┌────────┬────────┬────────┬────────┬──────────────────────────────┬────────┐
│        │        │        │(구)에필│                              │        │
│ 패딩   │ 프롤H  │ 프롤F  │재활용  │     첫 가용 블록 (4096B)     │ 에필H  │
│        │PACK8,1 │PACK8,1 │        │  헤더 + 페이로드 + 풋터      │PACK0,1 │
└────────┴────────┴────────┴────────┴──────────────────────────────┴────────┘
                       ↑                 ↑                          ↑
                  heap_listp         새 블록 시작               새 힙 끝
                  rover
```

---

## 코드 검토 — 수정/추가 여부

| 항목 | 판단 | 이유 |
|------|------|------|
| 전체 로직 | 올바름, 수정 불필요 | 완성된 구현 |
| `rover = heap_listp` | next-fit이면 필수 | first-fit이면 삭제 가능 |
| `extend_heap` 주석 | 보완 권장 | "8의 배수" 보다 "워드 단위 변환"이 더 정확 |
| `PUT(heap_listp, 0)` | 유지 | `mem_sbrk` 초기화 여부는 구현마다 달라 명시적 초기화가 안전 |

---

## 전체 흐름

```
mm_init()
  │
  ├─ mem_sbrk(16B)                → old_brk 반환 → heap_listp
  │    ├─ old_brk = mem_brk 저장
  │    ├─ mem_brk += 16
  │    └─ old_brk 반환
  │
  ├─ PUT × 4                      → 패딩 + 프롤로그 + 에필로그 기록
  │
  ├─ heap_listp += 8              → 프롤로그 풋터 위치로 이동
  ├─ rover = heap_listp           → next-fit 탐색 시작점
  │
  └─ extend_heap(1024 words)      → 첫 가용 블록(4096B) 생성
       ├─ mem_sbrk(4096B) 내부 호출
       ├─ 가용 블록 헤더/풋터 기록
       └─ coalesce(bp) → bp 반환
                              → malloc 호출 가능 상태  ✓
```