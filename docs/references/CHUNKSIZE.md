# `CHUNKSIZE` — 힙 확장 단위

---

## 매크로 정의

```c
#define CHUNKSIZE (1<<12)  // 4096B = 4KB
```

`CHUNKSIZE`는 `extend_heap`이 OS에게 메모리를 요청할 때 사용하는 **최소 확장 단위**입니다.

---

## 역할

```c
// mm_malloc에서 사용
extendsize = MAX(asize, CHUNKSIZE);
if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
    return NULL;
```

- `asize`가 `CHUNKSIZE`보다 작으면 → `CHUNKSIZE(4096B)` 단위로 확장
- `asize`가 `CHUNKSIZE`보다 크면 → `asize` 만큼 확장

너무 잦은 `extend_heap` 호출을 막기 위한 **최솟값 보장** 역할입니다.

---

## `1<<12` 인 이유 — OS 페이지 크기와 일치

```
1 << 12 = 2^12 = 4096B = 4KB
```

OS는 메모리를 **페이지(page)** 단위로 관리하고, 일반적인 페이지 크기는 4KB입니다.
`mem_sbrk()`로 힙을 늘릴 때 OS는 어차피 4KB 단위로 페이지를 할당합니다.

```
CHUNKSIZE = 4KB 요청
  → OS가 딱 1페이지(4KB) 줌  ← 정확히 맞아떨어짐

CHUNKSIZE = 3KB 요청
  → OS가 1페이지(4KB) 줌     ← 1KB 낭비 발생
  → 내부 단편화 증가
```

`CHUNKSIZE`를 4KB로 맞추면 OS 내부 처리 단위와 일치해 가장 효율적입니다.

---

## CHUNKSIZE 크기와 성능 관계

### 너무 작을 때 — `sbrk` 시스템 콜 폭증

```
CHUNKSIZE = 8B

요청 1(16B): extend_heap → sbrk() 호출  ← OS 시스템 콜
요청 2(16B): extend_heap → sbrk() 호출  ← OS 시스템 콜
요청 3(16B): extend_heap → sbrk() 호출  ← OS 시스템 콜
...
```

`sbrk()`는 OS 시스템 콜이므로 일반 함수 호출보다 **수백 배** 비쌉니다.
`CHUNKSIZE`가 작으면 매 할당마다 시스템 콜이 발생해 급격히 느려집니다.

### 너무 클 때 — 힙 비대, find_fit 느려짐

```
CHUNKSIZE = 1MB

요청: 16B 할당
  → 1MB 확보 후 16B만 사용
  → 나머지 ~1MB가 가용 블록으로 남음
  → find_fit이 순회해야 할 블록 수 증가 → 탐색 느려짐
```

### 적당할 때 — 균형점

```
처리 속도
   ↑
   │         ★ 최적점 (4KB 부근)
   │       /    \
   │      /      \
   │     /        \
   │    /          \  ← 너무 크면: 힙 비대, find_fit 느림
   │   /
   │  ↑ 너무 작으면: sbrk 시스템 콜 폭증
   └──────────────────────────────→ CHUNKSIZE
      8B  64B  256B  1KB  4KB  1MB
```

---

## 크기별 비교

| CHUNKSIZE | 힙 크기 | sbrk 호출 횟수 | find_fit 속도 | 전체 성능 |
|-----------|---------|---------------|--------------|----------|
| 너무 작음 (8B~64B) | 작음 | 매우 많음 | 빠름 | 느림 (sbrk 병목) |
| 적당함 (4KB) | 보통 | 적당함 | 보통 | 빠름 |
| 너무 큼 (1MB~) | 큼 | 매우 적음 | 느림 | 느림 (find_fit 병목) |

---

## 요약

```
CHUNKSIZE = 1<<12 (4KB)
  ├─ OS 페이지 크기(4KB)와 일치  → sbrk 요청이 페이지 단위와 맞아떨어짐
  ├─ 너무 작지 않음              → sbrk 시스템 콜 과다 호출 방지
  └─ 너무 크지 않음              → 힙 비대 및 find_fit 탐색 비용 방지
```