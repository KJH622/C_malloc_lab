# 🧠 Malloc Lab — mm.c 완전 정복

> CS:APP Malloc Lab 구현 코드(`mm.c`) 전체 분석 노트.
> **Next-fit + Implicit Free List + Boundary Tag** 방식으로 구현된 동적 메모리 할당기입니다.

---

## 📌 이 구현의 핵심 전략

| 항목 | 선택 |
|------|------|
| 탐색 방식 | **Next-fit** (rover 포인터로 마지막 위치 기억) |
| 리스트 구조 | **Implicit Free List** (모든 블록을 순서대로 순회) |
| 경계 태그 | **Header + Footer** (앞뒤 블록 alloc 상태 O(1) 확인) |
| 정렬 단위 | **8바이트 (DSIZE)** |
| 병합 방식 | **즉시 병합 (Immediate Coalescing)** |

---

## 🗂️ 힙 전체 구조

```
낮은 주소 ──────────────────────────────────────────→ 높은 주소

[ 패딩 4B ][ 프롤로그 HDR(8,1) ][ 프롤로그 FTR(8,1) ][ ... 일반 블록들 ... ][ 에필로그 HDR(0,1) ]
                                        ↑
                                   heap_listp
                                   (mm_init 후)
```

- **패딩(4B)**: 8바이트 정렬을 위한 더미. 아무 의미 없음.
- **프롤로그 블록(8B)**: HDR + FTR만 존재. 절대 해제 안 됨. 힙 순회의 경계 역할.
- **에필로그 블록(4B)**: size=0, alloc=1인 HDR만 존재. 힙 끝을 표시하는 sentinel.
- **heap_listp**: 프롤로그 FTR을 가리킴 → 힙 순회 시작점.
- **rover**: Next-fit 탐색 시 마지막 탐색 위치를 기억하는 포인터.

---

## 📦 일반 블록 구조

```
할당된 블록:
┌─────────────┬──────────────────────────┬─────────────┐
│  HDR (4B)   │      PAYLOAD (가변)       │  FTR (4B)   │
│  size | 1   │    실제 사용자 데이터      │  size | 1   │
└─────────────┴──────────────────────────┴─────────────┘
              ↑
             bp (block pointer = malloc이 반환하는 주소)

가용 블록:
┌─────────────┬──────────────────────────┬─────────────┐
│  HDR (4B)   │      (미사용 공간)        │  FTR (4B)   │
│  size | 0   │                          │  size | 0   │
└─────────────┴──────────────────────────┴─────────────┘
```

- **HDR/FTR 값**: `PACK(size, alloc)` = `size | alloc`
  - 상위 29비트: 블록 전체 크기 (헤더+페이로드+풋터 포함)
  - 하위 1비트: 0=가용, 1=할당
- **최소 블록 크기**: 16B (HDR 4B + 페이로드 최소 8B + FTR 4B)
- **bp**: `malloc()`이 반환하는 포인터. 헤더 바로 뒤 주소.

---

## 🔑 핵심 매크로 요약

| 매크로 | 설명 | 예시 |
|--------|------|------|
| `HDRP(bp)` | bp의 헤더 주소 | `bp - 4` |
| `FTRP(bp)` | bp의 풋터 주소 | `bp + size - 8` |
| `NEXT_BLKP(bp)` | 다음 블록 payload 시작 | `bp + GET_SIZE(HDR)` |
| `PREV_BLKP(bp)` | 이전 블록 payload 시작 | `bp - GET_SIZE(bp-8)` |
| `GET_SIZE(p)` | 헤더/풋터에서 크기 추출 | `*(p) & ~0x7` |
| `GET_ALLOC(p)` | 헤더/풋터에서 alloc 추출 | `*(p) & 0x1` |
| `PACK(size, alloc)` | 헤더/풋터 값 생성 | `size \| alloc` |
| `ALIGN(size)` | 8바이트 올림 정렬 | `(size+7) & ~7` |

---

## 🔄 함수 호출 흐름

```
mm_malloc(size)
    │
    ├─ find_fit(asize)  ← rover부터 탐색 (Next-fit)
    │       │
    │       └─ 성공 → place(bp, asize)
    │
    └─ 실패 → extend_heap(extendsize)
                    │
                    └─ mem_sbrk() → coalesce(bp) → place(bp, asize)


mm_free(bp)
    │
    └─ alloc 비트 0으로 변경 → coalesce(bp)
                                    │
                                    └─ 인접 가용 블록 병합 (4가지 case)


mm_realloc(ptr, size)
    │
    ├─ 다음 블록 free + 크기 충분 → 제자리 병합
    ├─ 힙 끝 블록 → mem_sbrk()로 힙만 확장
    └─ fallback → mm_malloc + memcpy + mm_free
```

---

## 📄 함수별 상세 페이지

| 함수 | 역할 |
|------|------|
| [📌 매크로 & 상수](./01_macros.md) | WSIZE, DSIZE, ALIGN, PACK, GET, PUT 등 |
| [🏗️ mm_init](./02_mm_init.md) | 힙 초기화 (프롤로그/에필로그 세팅) |
| [📈 extend_heap](./03_extend_heap.md) | 힙 확장 요청 |
| [🔍 find_fit](./04_find_fit.md) | Next-fit 가용 블록 탐색 |
| [✂️ place](./05_place.md) | 블록 배치 + 분할 |
| [🧩 mm_malloc](./06_mm_malloc.md) | 메모리 할당 |
| [🗑️ mm_free + coalesce](./07_mm_free_coalesce.md) | 해제 + 4-way 병합 |
| [🔄 mm_realloc](./08_mm_realloc.md) | 크기 재조정 |
