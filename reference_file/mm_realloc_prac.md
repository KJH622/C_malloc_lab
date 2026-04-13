# `mm_realloc()` — 힙 메모리 재할당 함수

---

## 함수 코드

```c
void *mm_realloc(void *ptr, size_t size) // ptr : mm_malloc이 반환했던 페이로드 시작 주소
{
}
```

---

## 역할 요약

`mm_realloc()`은 이미 할당된 블록의 크기를 바꾸는 함수입니다.
핵심 역할은 딱 두 가지입니다.

1. **예외 처리** — `ptr == NULL`이면 malloc, `size == 0`이면 free와 동일하게 처리
2. **재할당** — 새 블록 확보 → 데이터 복사 → 기존 블록 반납 → 새 주소 반환

---

## `ptr`이 가리키는 위치

```
         ptr
          ↓
┌────────┬──────────────────────┬────────┐
│  헤더  │      페이로드        │  풋터  │
│PACK(sz,1)   (사용자 데이터)  │PACK(sz,1)│
└────────┴──────────────────────┴────────┘
  ptr-4        ptr = 페이로드 시작
```

> `ptr`은 `mm_malloc`이 반환한 페이로드 시작 주소입니다.
> 블록 크기를 알려면 `GET_SIZE(HDRP(ptr))`로 읽으면 됩니다.

---

## 단계별 실행 과정

### Step 1 — 예외 처리 : `ptr == NULL`

```c
if (ptr == NULL)
    return mm_malloc(size);
```

- `ptr`이 `NULL`이면 이전에 할당한 블록이 없다는 뜻입니다.
- 새로 할당하면 되므로 `mm_malloc(size)`와 동일하게 처리합니다.

```
ptr == NULL ?
    YES → return mm_malloc(size)
    NO  → Step 2로
```

---

### Step 2 — 예외 처리 : `size == 0`

```c
if (size == 0) {
    mm_free(ptr);
    return NULL;
}
```

- `size`가 0이면 0바이트짜리 블록은 필요 없다는 뜻입니다.
- 기존 블록을 반납하면 되므로 `mm_free(ptr)`과 동일하게 처리합니다.

```
size == 0 ?
    YES → mm_free(ptr), return NULL
    NO  → Step 3으로
```

---

### Step 3 — 새 블록 할당

```c
void *new_bp = mm_malloc(size);
if (new_bp == NULL)
    return NULL;
```

- 요청 크기 `size`로 새 블록을 할당합니다.
- `mm_malloc`이 실패해 `NULL`을 반환하면 기존 `ptr`을 건드리지 않고 `NULL`을 반환합니다.

> `mm_malloc` 실패 시 기존 `ptr`을 `free`하면 데이터가 날아갑니다.
> 반드시 성공을 확인한 뒤에만 다음 단계로 넘어가야 합니다.

---

### Step 4 — 데이터 복사

```c
size_t old_size = GET_SIZE(HDRP(ptr)) - DSIZE;  // 기존 페이로드 크기
size_t copy_size = (old_size < size) ? old_size : size;  // MIN(기존, 새)
memcpy(new_bp, ptr, copy_size);
```

- `GET_SIZE(HDRP(ptr))`는 헤더+풋터(8B)가 포함된 블록 전체 크기이므로 `DSIZE`를 빼야 실제 페이로드 크기가 됩니다.
- 복사할 바이트 수는 반드시 `MIN(기존 크기, 새 크기)`여야 합니다.

```
크기를 늘릴 때 (size > 기존)
┌──────────────────┐         ┌──────────────────────────────┐
│  기존 블록 (32B) │  복사   │       새 블록 (64B)          │
│  데이터 32B      │ ──────► │  32B 복사 + 나머지 32B 미사용 │
└──────────────────┘         └──────────────────────────────┘
  32B만 복사 (old_size)

크기를 줄일 때 (size < 기존)
┌──────────────────────────┐         ┌──────────────┐
│      기존 블록 (64B)     │  복사   │  새 블록(32B)│
│      데이터 64B          │ ──────► │  32B만 복사  │
└──────────────────────────┘         └──────────────┘
  32B만 복사 (size) — 더 복사하면 새 블록 범위 초과!
```

---

### Step 5 — 기존 블록 반납 및 반환

```c
mm_free(ptr);
return new_bp;
```

- 데이터 복사가 끝났으므로 기존 블록을 반납합니다.
- 새 블록의 페이로드 주소 `new_bp`를 반환합니다.

---

## 전체 흐름

```
mm_realloc(ptr, size)
  ├─ ptr == NULL    → return mm_malloc(size)
  ├─ size == 0      → mm_free(ptr), return NULL
  │
  └─ 일반 케이스
       ├─ 1. new_bp = mm_malloc(size)
       │       └─ 실패 → return NULL  (ptr 건드리지 않음)
       ├─ 2. copy_size = MIN(old_size, size)
       ├─ 3. memcpy(new_bp, ptr, copy_size)
       ├─ 4. mm_free(ptr)
       └─ 5. return new_bp
```

---

## 복사 크기 계산 정리

| 상황 | copy_size | 이유 |
|------|-----------|------|
| `size > old_size` (크게) | `old_size` | 새 블록이 더 크므로 기존 데이터만큼만 복사 |
| `size < old_size` (작게) | `size` | 새 블록이 더 작으므로 넘치면 범위 초과 |
| `size == old_size` (같게) | `size` (= `old_size`) | 동일하므로 어느 쪽이든 무관 |

> `old_size = GET_SIZE(HDRP(ptr)) - DSIZE`
> — 헤더+풋터(8B) 제외한 실제 페이로드 크기

---

## 요약

```
mm_realloc(ptr, size)
  ├─ 예외 1. ptr == NULL   → malloc(size)
  ├─ 예외 2. size == 0     → free(ptr), NULL 반환
  ├─ 1. mm_malloc(size)    → 새 블록 확보
  ├─ 2. MIN(old, new)      → 복사 크기 결정
  ├─ 3. memcpy             → 데이터 복사
  ├─ 4. mm_free(ptr)       → 기존 블록 반납
  └─ 5. return new_bp      → 새 주소 반환
```