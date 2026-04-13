# `find_fit()` 개선 — first-fit → next-fit

---

## ① first-fit (기존)

```c
// first-fit: 앞에서부터 처음 맞는 블록 반환
static void *find_fit(size_t asize) {
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= asize)) {
            return bp;
        }
    }
    return NULL;
}
```

### 동작 방식

```
요청 1:  heap_listp ──────────────► 블록B (free) → 반환
요청 2:  heap_listp ──────────────► 블록B (free) → 반환
요청 3:  heap_listp ──────────────► 블록B (free) → 반환
                 ↑
         매번 처음부터 다시 탐색
```

### 문제점

힙 앞쪽에 이미 할당된 블록이 많아질수록, 매 요청마다 그 블록들을 처음부터 다시 건너뛰어야 합니다. 쓸모없는 반복 탐색이 누적됩니다.

---

## ② next-fit (개선)

```
현재: 매번 heap_listp 처음부터
개선: 전역 변수 rover 유지 → 마지막 위치부터 재개
구현 난이도 낮음, 처리량 빠르게 개선 가능
```

### 핵심 아이디어

```
요청 1:  heap_listp ──────────────► 블록B → 반환  (rover = 블록B)
요청 2:              rover(블록B) ──────────► 블록D → 반환  (rover = 블록D)
요청 3:                            rover(블록D) ──► 블록E → 반환  (rover = 블록E)
                                        ↑
                                이전 위치부터 이어서 탐색
```

앞쪽을 반복 탐색하지 않아도 됩니다.

---

## 변경 사항 3곳

### 변경 1 — 전역 변수 `rover` 추가

```c
static char *heap_listp;
static char *rover;  // ← 추가: 마지막 탐색 위치 기억
```

- `rover`는 마지막으로 블록을 찾은 위치를 기억하는 포인터입니다.
- 전역 변수이므로 `find_fit` 호출 사이에도 값이 유지됩니다.

---

### 변경 2 — `mm_init`에서 `rover` 초기화

```c
heap_listp += 2*WSIZE;
rover = heap_listp;  // ← 추가: 처음엔 heap_listp와 동일하게 시작
```

- 힙 초기화 시점에 `rover`를 `heap_listp`로 초기화합니다.
- 첫 번째 요청은 first-fit과 동일하게 처음부터 탐색합니다.

---

### 변경 3 — `find_fit` 함수 교체

```c
static void *find_fit(size_t asize) {
    void *bp;

    // 루프 1: rover부터 힙 끝까지 탐색
    for (bp = rover; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            rover = bp;
            return bp;
        }
    }

    // 루프 2: 못 찾으면 힙 처음부터 rover까지 탐색
    for (bp = heap_listp; bp < (void *)rover; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            rover = bp;
            return bp;
        }
    }

    return NULL;  // 힙 전체에 맞는 블록 없음
}
```

---

## 두 루프의 역할

```
힙 구조:
[ 프롤H ][ 블록A ][ 블록B ][ 블록C ][ 블록D ][ 블록E ][ 에필H ]
                              ↑
                           rover

루프 1 탐색 범위: rover ───────────────────────────► 에필H
루프 2 탐색 범위: heap_listp ─────────► rover 직전

합치면 힙 전체를 한 바퀴 순환하는 효과
```

### 루프 1 — `rover → 에필로그`

```c
for (bp = rover; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
```

- `rover` 위치에서 오른쪽으로 탐색합니다.
- `GET_SIZE(HDRP(bp)) > 0` — 에필로그(size=0)에 도달하면 루프 종료.
- 찾으면 즉시 `rover = bp` 갱신 후 반환합니다.

### 루프 2 — `heap_listp → rover`

```c
for (bp = heap_listp; bp < (void *)rover; bp = NEXT_BLKP(bp))
```

- 루프 1에서 못 찾았을 때 힙 처음부터 `rover` 직전까지 탐색합니다.
- `rover` 이전에 free된 블록(앞쪽에서 해제된 블록)을 재활용합니다.
- 찾으면 `rover = bp` 갱신 후 반환합니다.
- 여기서도 못 찾으면 `NULL` 반환 → `mm_malloc`이 `extend_heap` 호출.

---

## `rover` 갱신 시점

```c
rover = bp;   // 블록을 찾는 순간 즉시 갱신
return bp;
```

```
1회차 할당: 블록B 찾음 → rover = 블록B
             [ 프롤H ][ A ][ B* ][ C ][ D ][ E ][ 에필H ]
                           ↑ rover

2회차 할당: 블록B에서 시작 → 블록D 찾음 → rover = 블록D
             [ 프롤H ][ A ][ B ][ C ][ D* ][ E ][ 에필H ]
                                       ↑ rover

3회차 할당: 블록D에서 시작 → 블록E 찾음 → rover = 블록E
             [ 프롤H ][ A ][ B ][ C ][ D ][ E* ][ 에필H ]
                                           ↑ rover
```

`rover`가 앞으로 나아가므로, 이미 할당된 앞쪽 블록들을 반복 확인하지 않습니다.

---

## first-fit vs next-fit 비교

| 항목 | first-fit | next-fit |
|------|-----------|----------|
| 탐색 시작점 | 항상 `heap_listp` | 마지막 위치 `rover` |
| 추가 전역 변수 | 없음 | `rover` 1개 |
| 구현 난이도 | 낮음 | 낮음 |
| 반복 탐색 | 앞쪽 블록 매번 재확인 | 이전 위치부터 이어서 탐색 |
| 처리량 | 힙이 커질수록 느려짐 | 비교적 빠르게 유지 |
| wrap-around | 없음 (에필로그에서 종료) | 루프 2로 처음부터 재탐색 |

---

## 요약

```
first-fit
  └─ for (bp = heap_listp; ...) → 항상 처음부터

next-fit
  ├─ 전역 변수 rover 추가          → 마지막 위치 기억
  ├─ mm_init에서 rover = heap_listp → 초기화
  └─ find_fit 두 루프
       ├─ 루프 1: rover → 에필H    → 오른쪽 탐색
       ├─ 루프 2: heap_listp → rover → 나머지 탐색 (wrap-around)
       └─ 둘 다 실패               → NULL 반환
```