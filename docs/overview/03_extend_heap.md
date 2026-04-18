# 📈 extend_heap — 힙 확장

> 힙 공간이 부족할 때 OS에게 추가 메모리를 요청하는 함수.
> `mm_init`과 `mm_malloc`에서 호출됩니다.

---

## 코드

```c
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    // ① 홀수 words면 +1 해서 8바이트 정렬 맞춤
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    // ② mem_sbrk로 힙 확장
    if ((bp = mem_sbrk(size)) == (void*)-1)
        return NULL;

    // ③ 새 가용 블록의 헤더/풋터 설정
    PUT(HDRP(bp), PACK(size, 0));           // 새 가용 블록 헤더
    PUT(FTRP(bp), PACK(size, 0));           // 새 가용 블록 풋터

    // ④ 에필로그를 힙 끝으로 이동
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  // 새 에필로그 헤더

    // ⑤ 인접 가용 블록과 병합 후 반환
    return coalesce(bp);
}
```

---

## 동작 과정 시각화

### ② mem_sbrk(size) 호출 전후

```
[호출 전]
... │ 마지막 블록 │ 에필로그(0,1) │
                   ↑
                  mem_brk (힙 끝)

[호출 후: mem_sbrk(1024)]
... │ 마지막 블록 │    새로운 영역(1024B)   │ 에필로그(?) │
                   ↑                        ↑
                  bp = 이전 mem_brk       새 mem_brk
```

### ③ 헤더/풋터 설정 + ④ 에필로그 이동

```
bp가 새로운 영역 시작을 가리킴

주소:       bp-4    bp       bp+1020  bp+1024
            ┌───────┬────────────────┬───────┐
            │HDR    │  가용 공간      │  FTR  │
            │1024|0 │  (1016 bytes)  │1024|0 │
            └───────┴────────────────┴───────┘
                                              ↑
                                       HDRP(NEXT_BLKP(bp))
                                       여기에 PACK(0,1) 쓰기
                                       → 새 에필로그
```

---

## ① 홀수 words 처리 — 왜 짝수로 맞추나?

```
WSIZE = 4바이트, DSIZE = 8바이트

words = 3이면:
  3 * WSIZE = 12바이트  ← 8의 배수 아님! ✗

(3+1) * WSIZE = 16바이트  ← 8의 배수 ✓

규칙: words를 항상 짝수로 맞춰서 8바이트 정렬 보장
```

---

## ⑤ coalesce 호출 이유

```
extend_heap 직전 상황:

... │ 가용 블록(X) │ 에필로그(0,1) │
                    ↑ 새 블록이 여기 붙음

extend_heap 직후:

... │ 가용 블록(X) │ 새 가용 블록(1024) │ 에필로그 │

→ 앞에 가용 블록이 있으면 병합 가능!
→ coalesce(bp)로 즉시 처리
```

---

## 호출 위치 & 인자

| 호출 위치 | 인자 | 설명 |
|-----------|------|------|
| `mm_init` | `CHUNKSIZE/WSIZE` = 256 | 초기 힙 구성 |
| `mm_malloc` | `MAX(asize, CHUNKSIZE)/WSIZE` | 요청 크기와 최소 청크 중 큰 것 |

```c
// mm_malloc에서의 호출
extendsize = MAX(asize, CHUNKSIZE);
bp = extend_heap(extendsize / WSIZE);

// 예: malloc(100) → asize=112
//   MAX(112, 1024) = 1024
//   extend_heap(1024/4) = extend_heap(256)
//   → 실제 확장: 256 words = 1024 bytes
```

---

## 반환값

| 반환값 | 의미 |
|--------|------|
| `bp` | 새 가용 블록의 payload 시작 주소 (coalesce 결과) |
| `NULL` | `mem_sbrk` 실패 |
