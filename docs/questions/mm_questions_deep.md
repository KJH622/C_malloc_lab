# mm.c 심화 예상 질문 리스트

> 실제 코드 라인을 기반으로 한 질문입니다. "왜 이렇게 썼나요?", "이 값을 넣으면 어떻게 되나요?" 에 초점을 맞췄습니다.

---

## 1. 상수 / 매크로 선택 이유

**Q1. `CHUNKSIZE (1<<10)`을 1024바이트로 정한 이유가 뭔가요? 512나 4096이면 어떻게 달라지나요?**

<details>
<summary>정답 보기</summary>

1024(1KB)는 일반적인 malloc 구현에서 sbrk 호출 빈도와 메모리 낭비 사이의 균형점으로 널리 쓰이는 값입니다.

- **CHUNKSIZE를 줄이면 (예: 512)**: sbrk 호출 횟수가 늘어나 시스템 콜 오버헤드 증가 → throughput 저하
- **CHUNKSIZE를 늘리면 (예: 4096)**: sbrk 호출은 줄지만 초반에 많은 메모리를 미리 확보해 utilization 저하

`mm_malloc`에서 `extendsize = MAX(asize, CHUNKSIZE)` 로 사용되므로, 작은 요청이 많은 trace에서는 CHUNKSIZE가 클수록 한번에 넉넉하게 확보해 이후 탐색 없이 빠르게 할당할 수 있습니다.

</details>

---

**Q2. `extend_heap`을 호출할 때 `CHUNKSIZE`를 바로 넘기지 않고 `CHUNKSIZE/WSIZE`로 나눠서 넘기는 이유가 뭔가요? (94번 줄)**

```c
extend_heap(CHUNKSIZE/WSIZE)
```

<details>
<summary>정답 보기</summary>

`extend_heap`의 인자 단위가 **바이트가 아니라 워드(words)** 이기 때문입니다. 함수 내부에서 `words * WSIZE`로 변환해 바이트 크기를 구합니다.

```c
size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
```

`CHUNKSIZE = 1024`를 그대로 넘기면 `1024 * 4 = 4096바이트`를 확장하게 되어 의도와 다릅니다.  
`CHUNKSIZE/WSIZE = 256 words` → `256 * 4 = 1024바이트` 가 정상입니다.

만약 바이트 단위로 바로 넘긴다면 내부 로직을 바꾸거나 인터페이스 단위를 통일해야 합니다.

</details>

---

**Q3. `ALIGN` 매크로를 정의해 뒀는데 `mm_malloc`에서는 이걸 쓰지 않고 직접 계산합니다. 왜 그랬나요?**

```c
#define ALIGN(size) (((size) + (DSIZE - 1)) & ~(DSIZE - 1))
// vs
asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
```

<details>
<summary>정답 보기</summary>

`ALIGN(size)`는 오버헤드(헤더+풋터 8바이트)를 포함하지 않고 순수하게 크기만 올림합니다. `mm_malloc`에서는 **오버헤드를 먼저 더한 뒤 정렬**해야 하기 때문에 `ALIGN(size + DSIZE)`와 동일한 식을 직접 전개한 것입니다.

즉, `DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE)` = `ALIGN(size + DSIZE)` 와 같습니다.

`ALIGN(size + DSIZE)`로 써도 결과는 동일하지만, 명시적으로 전개해서 각 항의 의미(오버헤드 포함, ceiling 나눗셈)를 주석으로 설명하는 방식을 선택한 것으로 볼 수 있습니다.

</details>

---

## 2. mm_init 트레이싱

**Q4. `mm_init`에서 `heap_listp += 2*WSIZE` 후 `rover = heap_listp`로 초기화합니다. 이 시점에 `rover`는 힙의 어디를 가리키나요? 이후 `extend_heap`이 불리면 rover는 어떻게 변하나요?**

<details>
<summary>정답 보기</summary>

`heap_listp += 2*WSIZE` 이후 heap_listp는 **프롤로그 풋터**를 가리킵니다. 이 시점의 힙 레이아웃:

```
[패딩(0)] [프롤로그 헤더(8|1)] [프롤로그 풋터(8|1)] [에필로그 헤더(0|1)]
                                       ↑
                              heap_listp = rover
```

이후 `extend_heap(256)`이 호출되면 1024바이트 가용 블록이 에필로그 자리에 생깁니다:

```
[패딩] [프롤로그H] [프롤로그F] [가용블록 헤더(1024|0)] ... [가용블록 풋터(1024|0)] [새 에필로그(0|1)]
                                ↑ 새 블록 bp
```

`coalesce`에서 이전 블록(프롤로그, alloc=1)이 할당 상태이므로 병합 없이 bp만 반환됩니다. `rover`는 그대로 heap_listp(프롤로그 풋터)를 가리킵니다. 첫 `find_fit` 호출 시 rover부터 순회하면서 이 가용 블록을 찾게 됩니다.

</details>

---

**Q5. `mm_init`에서 `rover = heap_listp`로 초기화하는데, 왜 프롤로그 블록을 가리키도록 설정했나요? 처음부터 가용 블록 시작점으로 설정하면 안 되나요?**

<details>
<summary>정답 보기</summary>

`mm_init` 시점에는 아직 가용 블록이 없습니다. `extend_heap`이 호출되기 전이므로 rover가 가리킬 유효한 가용 블록이 없는 상태입니다.

`heap_listp`(프롤로그 풋터)는 힙 순회의 시작 기준점입니다. `find_fit`의 루프 1은 rover부터, 루프 2는 `heap_listp`부터 시작하는데, rover = heap_listp이면 루프 1이 힙 전체를 처음부터 스캔하게 됩니다. `extend_heap` 호출 직후 rover가 갱신되지 않으므로 첫 `mm_malloc` 시 루프 1에서 새로 생성된 가용 블록을 자연스럽게 찾습니다.

</details>

---

## 3. mm_malloc 숫자 트레이싱

**Q6. `mm_malloc(1)`을 호출하면 `asize`가 얼마가 되나요? 왜 1바이트 요청에 16바이트 블록이 할당되나요?**

<details>
<summary>정답 보기</summary>

`size = 1 <= DSIZE(8)` 이므로 `asize = 2 * DSIZE = 16`.

이유는 두 가지입니다:
1. **헤더(4) + 풋터(4) = 8바이트** 오버헤드가 반드시 필요합니다.
2. **8바이트 정렬** 조건에 의해 payload는 최소 8바이트여야 합니다.

따라서 1바이트를 요청해도 실제 블록 구조는 다음과 같습니다:

```
[헤더(16|1)] [payload 8바이트] [풋터(16|1)]
              ↑ 1바이트만 사용, 나머지 7바이트는 내부 단편화
```

</details>

---

**Q7. `mm_malloc(9)`를 호출하면 `asize`를 계산하는 과정을 단계별로 설명해주세요.**

<details>
<summary>정답 보기</summary>

`size = 9 > DSIZE(8)` 이므로 두 번째 분기 실행:

```
asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE)
      = 8     * ((9    + 8     + 7          ) / 8     )
      = 8     * (24 / 8)
      = 8     * 3
      = 24
```

블록 구조:
```
[헤더(24|1)] [payload 16바이트] [풋터(24|1)]
              ↑ 9바이트만 사용, 7바이트 내부 단편화
```

헤더(4) + 9바이트 + 풋터(4) = 17바이트를 8의 배수로 올리면 24바이트가 됩니다.

</details>

---

**Q8. `mm_malloc(1016)`을 호출했을 때 힙에 가용 블록이 없다면 `extendsize`는 얼마가 되나요?**

```c
extendsize = MAX(asize, CHUNKSIZE);
```

<details>
<summary>정답 보기</summary>

먼저 asize 계산:
```
asize = 8 * ((1016 + 8 + 7) / 8) = 8 * (1031 / 8) = 8 * 128 = 1024
```

`MAX(1024, 1024) = 1024` → `extendsize = 1024`

`extend_heap(1024/4)` = `extend_heap(256 words)` → 256은 짝수이므로 `size = 256 * 4 = 1024바이트` 확장.

할당 후 남는 가용 공간이 없으므로 `place`에서 분할 없이 전체 할당됩니다(`csize - asize = 0 < 2*DSIZE`).

</details>

---

**Q9. `mm_malloc(1017)`을 호출하면 `asize`는 얼마인가요? 이 경우 `extendsize`는 어떻게 결정되나요?**

<details>
<summary>정답 보기</summary>

```
asize = 8 * ((1017 + 8 + 7) / 8) = 8 * (1032 / 8) = 8 * 129 = 1032
```

`MAX(1032, 1024) = 1032` → `extendsize = 1032`

`extend_heap(1032/4)` = `extend_heap(258 words)` → 258은 짝수이므로 `size = 258 * 4 = 1032바이트` 확장.

이 경우 1017바이트 요청이 CHUNKSIZE(1024)보다 큰 asize를 만들어내므로, CHUNKSIZE가 아닌 asize에 맞춰 힙을 확장합니다.

</details>

---

## 4. find_fit 코드 선택 이유

**Q10. `find_fit` 루프 2에서 종료 조건이 `bp < (void *)rover`인데, `bp <= rover`가 아닌 이유가 뭔가요?**

```c
for (bp = heap_listp; bp < (void *)rover; bp = NEXT_BLKP(bp))
```

<details>
<summary>정답 보기</summary>

`rover` 위치의 블록은 루프 1에서 이미 탐색했거나, 조건을 만족하지 않아서 건너뛴 블록입니다. 루프 2에서 `bp == rover`까지 포함하면 rover 블록을 다시 검사하게 됩니다. 이는 중복 탐색으로 의미가 없고, 루프 1과 루프 2 사이에 힙 전체를 정확히 한 번 씩 탐색하는 원칙에도 어긋납니다. `bp < rover`로 제한해 힙 처음 ~ rover 직전까지만 탐색하면, 루프 1(rover ~ 힙 끝) + 루프 2(힙 처음 ~ rover 직전) = 힙 전체를 빠짐없이 한 번 순회합니다.

</details>

---

**Q11. `find_fit`에서 `GET_SIZE(HDRP(bp)) > 0`이 루프 종료 조건인데, 이 조건이 `false`가 되는 순간은 언제인가요?**

```c
for (bp = rover; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
```

<details>
<summary>정답 보기</summary>

에필로그 헤더에 도달했을 때입니다. 에필로그 헤더는 `PACK(0, 1) = 0|1 = 1`로 초기화됩니다. `GET_SIZE(에필로그 헤더) = 1 & ~0x7 = 0` 이므로 조건이 false가 되어 루프가 종료됩니다.

에필로그는 힙의 마지막 블록임을 알리는 sentinel 역할을 합니다. `NEXT_BLKP`로 에필로그를 넘어가면 힙 밖의 메모리를 읽게 되므로, 이 조건이 없으면 무한루프 또는 segfault가 발생합니다.

</details>

---

## 5. place 코드 선택 이유

**Q12. `place`에서 할당 블록을 앞에, 남은 가용 블록을 뒤에 배치합니다. 반대로 가용 블록을 앞에 두면 어떻게 되나요?**

```c
PUT(HDRP(bp), PACK(asize, 1)); // 앞쪽 = 할당
bp = NEXT_BLKP(bp);
PUT(HDRP(bp), PACK(csize - asize, 0)); // 뒤쪽 = 가용
```

<details>
<summary>정답 보기</summary>

가용 블록을 앞에 두면 `find_fit`이 반환한 `bp`가 가용 블록의 시작이 되고, 할당 블록은 bp + (csize-asize) 위치가 됩니다. 이 경우 `mm_malloc`이 반환하는 주소가 가용 블록의 payload 주소가 되어 **할당 블록의 주소를 별도로 계산해 반환해야 합니다**.

현재 구현처럼 앞쪽에 할당 블록을 두면 `find_fit`이 반환한 `bp`를 그대로 caller에게 반환할 수 있어 코드가 단순해집니다. 또한 이 방식은 Next-fit의 `rover`를 자연스럽게 분할된 가용 블록 쪽으로 위치시키는 부수 효과도 있습니다.

</details>

---

**Q13. `place`에서 분할 후 남은 가용 블록에 대해 `rover`를 업데이트하지 않습니다. 이것이 Next-fit 성능에 영향을 줄 수 있나요?**

<details>
<summary>정답 보기</summary>

영향을 줄 수 있습니다. `find_fit`에서 rover = bp (할당된 블록)으로 설정됩니다. 이후 `place`에서 bp 뒤쪽에 새 가용 블록이 생기지만 rover는 이미 할당된 블록을 가리킵니다.

다음 `find_fit` 호출 시 루프 1은 rover(할당된 블록)부터 시작하므로 즉시 NEXT_BLKP로 분할된 가용 블록을 만나게 됩니다. 결과적으로 한 번의 NEXT_BLKP 이동만 더 발생할 뿐, 탐색 정확도에는 문제가 없습니다. 다만 이론적으로 `place`에서 `rover = 분할된 가용 블록 주소`로 갱신하면 다음 탐색의 첫 번째 블록이 바로 가용 블록이어서 더 최적화됩니다.

</details>

---

## 6. coalesce 코드 선택 이유

**Q14. coalesce case 2와 case 3에서 rover 조정 조건이 미묘하게 다릅니다. 왜 case 2는 `rover > bp`이고 case 3은 `rover >= bp`인가요?**

```c
// case 2
if (rover > (char *)bp && rover < (char *)NEXT_BLKP(bp))
    rover = bp;

// case 3
if (rover >= (char *)bp && rover < (char *)NEXT_BLKP(bp))
    rover = bp;
```

<details>
<summary>정답 보기</summary>

두 case에서 `bp`가 의미하는 블록이 다릅니다.

- **Case 2** (뒤 블록과 병합): `bp`는 현재 해제된 블록입니다. `rover == bp`라면 rover는 이미 현재 블록의 시작을 정확히 가리키고 있으므로, 병합 후에도 `rover = bp`로 재설정할 필요가 없습니다. `rover > bp`인 경우만 체크하면 충분합니다.

- **Case 3** (앞 블록과 병합): 이 코드에서 bp는 `bp = PREV_BLKP(bp)` 이후의 값입니다. 즉 병합된 블록의 새 시작점입니다. rover가 기존 bp(현재 블록 시작)를 가리키고 있었다면, 그 블록이 앞 블록에 흡수되어 사라지므로 반드시 `rover = bp(새 시작점)`으로 갱신해야 합니다. 따라서 `>= bp`가 필요합니다.

</details>

---

**Q15. coalesce case 1에 빈 if 블록 `{ }`이 있습니다. 왜 이 분기를 명시적으로 남겨뒀나요? 그냥 지워도 되지 않나요?**

```c
if (prev_alloc && next_alloc) { } // 아무것도 하지 않음, bp 그대로 반환
```

<details>
<summary>정답 보기</summary>

기능적으로는 지워도 동일하게 작동합니다. 이 블록을 남겨둔 이유는 **가독성과 명시성** 때문입니다. coalesce에는 4가지 case가 존재한다는 것을 코드 구조에서 명확히 드러내고, 교재(CSAPP)의 설명과 코드 구조를 일치시키기 위함입니다. 협업 시 "case 1이 의도적으로 처리 없음"임을 주석과 함께 표현하면 리뷰어가 누락인지 의도인지 혼동하지 않습니다.

단, 실제 프로덕션 코드에서는 `if (!(prev_alloc && next_alloc))` 으로 반전하거나 빈 블록을 제거하는 것이 일반적입니다.

</details>

---

**Q16. coalesce에서 `prev_alloc`을 읽을 때 왜 `FTRP(PREV_BLKP(bp))`처럼 두 번 이동하나요? 그냥 `(char*)bp - DSIZE`에서 읽으면 같은 값 아닌가요?**

```c
size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
```

<details>
<summary>정답 보기</summary>

`FTRP(PREV_BLKP(bp))`와 `(char*)bp - DSIZE`는 **동일한 주소**를 가리킵니다.

```
FTRP(PREV_BLKP(bp))
  = (char*)PREV_BLKP(bp) + GET_SIZE(HDRP(PREV_BLKP(bp))) - DSIZE
  = (bp - prev_size) + prev_size - DSIZE
  = bp - DSIZE
```

따라서 `GET_ALLOC((char*)bp - DSIZE)`로 써도 동일합니다. 매크로 표현식이 더 긴 이유는 **의도를 명확히 표현**하기 위함입니다. "이전 블록의 풋터에서 alloc 비트를 읽는다"는 의미를 PREV_BLKP + FTRP 조합으로 표현하면 코드가 자기 설명적(self-documenting)이 됩니다.

</details>

---

## 7. mm_realloc 코드 선택 이유 및 잠재적 버그

**Q17. realloc 제자리 확장 ①에서 병합 후 남은 공간이 `asize`보다 훨씬 클 때도 분할을 하지 않습니다. 이것이 문제가 될 수 있나요?**

```c
if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) && old_size + next_size >= asize) {
    size_t total = old_size + next_size;
    PUT(HDRP(ptr), PACK(total, 1)); // total 전체를 할당
    PUT(FTRP(ptr), PACK(total, 1));
    return ptr;
}
```

<details>
<summary>정답 보기</summary>

네, **메모리 활용률(utilization) 손실**이 발생합니다. 예를 들어:

- `old_size = 32`, `next_size = 512`, `asize = 48`인 경우
- `total = 544`를 통째로 할당 → 496바이트가 내부 단편화

분할 로직 `(total - asize) >= 2*DSIZE`를 추가하면 남은 공간을 다시 가용 블록으로 만들 수 있습니다. 이는 realloc 이후 다른 할당 요청이 많을 때 특히 중요합니다.

다만 분할을 추가하면 코드 복잡도가 올라가고 rover 갱신도 필요해지므로, 단순성과 성능 사이의 트레이드오프입니다.

</details>

---

**Q18. `copy_size = old_size - WSIZE`로 계산합니다. 이게 맞는 값인가요? 실제 payload 크기와 비교하면 어떤가요?**

```c
size_t copy_size = old_size - WSIZE; // payload 크기 (헤더 제외)
```

<details>
<summary>정답 보기</summary>

**잠재적 버그**입니다.

`old_size = GET_SIZE(HDRP(ptr))`는 헤더(4) + payload + 풋터(4) 를 합한 블록 전체 크기입니다.

- 실제 payload 크기 = `old_size - DSIZE` (헤더 4 + 풋터 4 = 8 제거)
- 코드의 `copy_size = old_size - WSIZE` = payload + 풋터 4바이트 포함

즉 **풋터 4바이트를 payload로 오인하고 복사**합니다. 다행히 `if (size < copy_size) copy_size = size` 조건으로 새로운 size가 작을 경우 제한되고, 메모리는 여전히 유효한 블록 범위 내이므로 실제 crash는 잘 발생하지 않습니다. 그러나 엄밀히는 `old_size - DSIZE`가 올바른 값입니다.

</details>

---

**Q19. realloc 제자리 확장 ②에서 `next_size == 0` 으로 에필로그를 감지합니다. 에필로그 헤더 값이 `PACK(0,1) = 1`인데, 왜 `GET_SIZE`가 0을 반환하나요?**

```c
if (next_size == 0) { // 에필로그 감지
```

<details>
<summary>정답 보기</summary>

`GET_SIZE(p) = GET(p) & ~0x7`

에필로그 헤더 = `PACK(0, 1) = 0 | 1 = 0x00000001`

```
GET_SIZE(에필로그 헤더) = 0x00000001 & 0xFFFFFFF8 = 0x00000000 = 0
```

하위 3비트(alloc 플래그)를 마스킹으로 제거하면 크기 부분이 0이 됩니다. 이 방식 덕분에 에필로그를 별도 변수 없이 크기 0으로 구분할 수 있습니다.

</details>

---

**Q20. realloc fallback에서 `mm_malloc(size)`를 호출하고 곧바로 `mm_free(ptr)`를 호출합니다. 만약 `mm_malloc`이 `ptr`의 바로 뒤 공간을 새 블록으로 사용하게 된다면, `memcpy` 도중 데이터가 덮어써질 위험이 없나요?**

```c
void *new_bp = mm_malloc(size);
memcpy(new_bp, ptr, copy_size);
mm_free(ptr);
```

<details>
<summary>정답 보기</summary>

**없습니다.** fallback은 제자리 확장 ①②가 모두 실패한 경우에만 실행됩니다. 즉 다음 블록이 할당 상태이거나 힙 끝이 아닌 상태입니다.

`mm_malloc`은 가용 블록을 찾아 새 블록을 배정하는데, `ptr`은 아직 `mm_free`되지 않은 할당 블록이므로 `mm_malloc`이 `ptr` 영역을 선택할 수 없습니다. `memcpy` 완료 후 `mm_free(ptr)`를 호출하므로 데이터 덮어쓰기 위험은 없습니다.

다만 만약 `mm_free(ptr)`를 `memcpy` **전에** 호출했다면, `ptr` 영역이 다음 `mm_malloc`에 의해 재사용될 수 있어 데이터 손실이 발생할 수 있습니다. 현재 코드는 순서가 올바릅니다.

</details>

---

## 8. extend_heap 코드 선택 이유

**Q21. `extend_heap`에서 에필로그를 설정하는 코드 `PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1))`은 어떤 원리로 에필로그를 정확한 위치에 쓰나요?**

```c
PUT(HDRP(bp), PACK(size, 0));      // 새 블록 헤더
PUT(FTRP(bp), PACK(size, 0));      // 새 블록 풋터
PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 새 에필로그
```

<details>
<summary>정답 보기</summary>

`NEXT_BLKP(bp) = (char*)bp + GET_SIZE(HDRP(bp)) = bp + size`

→ 새 블록 직후 바이트 주소

`HDRP(NEXT_BLKP(bp)) = NEXT_BLKP(bp) - WSIZE = bp + size - 4`

→ 새 블록 풋터 바로 다음 위치 = 새 에필로그 헤더 위치

`mem_sbrk(size)` 호출로 힙이 정확히 `size`바이트 확장됐고, 그 끝에서 4바이트 앞(= 새 에필로그 헤더 자리)에 `PACK(0,1)`을 씁니다. 이전 에필로그 자리는 새 가용 블록의 헤더로 덮어써졌으므로, 이 한 줄이 에필로그를 새 힙 끝으로 정확히 이동시킵니다.

</details>

---

**Q22. `extend_heap`은 반드시 `coalesce`를 호출해서 반환합니다. 만약 coalesce를 호출하지 않고 `return bp`를 하면 어떤 상황에서 문제가 생기나요?**

<details>
<summary>정답 보기</summary>

힙 확장 직전에 힙의 마지막 블록이 가용 상태였다면 문제가 생깁니다.

예시:
```
... [가용 블록 32B] [에필로그]
```
여기서 `extend_heap`으로 1024바이트를 추가하면:
```
... [가용 블록 32B] [새 가용 블록 1024B] [새 에필로그]
```

`coalesce` 없이 1024B 블록만 반환하면 32B와 1024B가 분리된 두 개의 가용 블록으로 남습니다. 실제로는 합쳐 1056B인 연속 공간이지만, 1033B 요청이 오면 두 블록 모두 조건 미달로 실패하고 또다시 `extend_heap`이 불립니다. `coalesce`가 없으면 **false fragmentation**이 지속적으로 발생합니다.

</details>

---

## 9. 경계 조건 및 엣지 케이스

**Q23. `mm_malloc(0)`을 호출하면 어떻게 되나요? 표준 `malloc(0)`의 동작과 일치하나요?**

<details>
<summary>정답 보기</summary>

```c
if (size == 0) {
    return NULL;
}
```

현재 구현은 `NULL`을 반환합니다. C 표준(ISO C11)에 따르면 `malloc(0)`은 구현에 따라 `NULL`이나 유효하지만 free 가능한 포인터를 반환할 수 있습니다("implementation-defined"). 따라서 `NULL` 반환 자체는 표준을 위반하지 않습니다.

다만 일부 코드가 `malloc(0) != NULL`을 기대하고 동작하는 경우 호환성 문제가 생길 수 있습니다. mdriver 테스트에서는 `size == 0` 요청이 들어올 경우 `NULL` 반환을 허용합니다.

</details>

---

**Q24. `mm_free(NULL)`을 호출하면 어떻게 되나요? 현재 코드에서 처리가 되어 있나요?**

```c
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    ...
}
```

<details>
<summary>정답 보기</summary>

**처리되지 않습니다.** `bp == NULL`이면 `HDRP(bp) = (char*)NULL - 4 = 0xFFFFFFFC`(혹은 플랫폼에 따라 다름)로 잘못된 메모리를 읽어 **segmentation fault** 또는 undefined behavior가 발생합니다.

C 표준은 `free(NULL)`을 no-op으로 정의합니다. 따라서 아래 코드를 추가하는 것이 올바릅니다:

```c
void mm_free(void *bp) {
    if (bp == NULL) return; // 추가 필요
    ...
}
```

mdriver 테스트에서는 보통 `NULL`을 `free`에 넘기지 않지만, 실제 사용 환경에서는 위험한 버그가 됩니다.

</details>

---

**Q25. 현재 `place` 함수에서 `rover`가 분할되어 사라지는 블록을 가리키고 있을 경우 rover를 갱신하지 않습니다. 이게 문제가 되지 않나요?**

<details>
<summary>정답 보기</summary>

`place`가 호출되기 직전에 `find_fit`이 `rover = bp`로 설정합니다. `place`는 bp의 앞부분을 할당 블록으로 만들고 뒤에 가용 블록을 남깁니다. 이 시점의 rover는 이미 할당된 블록 bp를 가리킵니다.

다음 `find_fit` 루프 1은 rover(= 할당된 블록)에서 시작합니다. 첫 번째 블록(`GET_ALLOC == 1`)을 건너뛰고 `NEXT_BLKP`로 분할된 가용 블록에 도달하게 됩니다. 동작은 정상이지만, `place`에서 `rover = NEXT_BLKP(bp)`(분할된 가용 블록)로 갱신하면 다음 탐색의 첫 블록이 바로 가용 블록이어서 미세한 성능 개선이 가능합니다. 즉 현재 코드는 **버그는 아니지만 최적화 여지가 있습니다.**

</details>

