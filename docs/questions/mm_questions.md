# mm.c 발표 예상 질문 리스트

> **구현 방식 요약**: Implicit Free List + Next-fit + Boundary Tag (Header/Footer) Coalescing

---

## 1. 전체 설계 관련

**Q1. 왜 Explicit Free List나 Segregated Free List 대신 Implicit Free List를 선택했나요?**

<details>
<summary>정답 보기</summary>

Implicit Free List는 별도의 포인터 필드 없이 헤더/풋터만으로 힙 전체를 순회할 수 있어 구현이 가장 단순합니다. Explicit Free List는 가용 블록에 prev/next 포인터를 추가해 탐색 속도를 높이지만 구현 복잡도가 올라가고, Segregated Free List는 크기별로 리스트를 분리해 단편화를 줄이지만 더 복잡합니다. malloc-lab의 기본 구현 단계에서는 Implicit List로 정확성을 먼저 확보하는 것이 합리적인 선택입니다.

</details>

---

**Q2. Next-fit을 선택한 이유가 뭔가요? First-fit이나 Best-fit과 비교해서 장단점이 있나요?**

<details>
<summary>정답 보기</summary>

| 방식 | 탐색 시작점 | 속도 | 단편화 |
|------|------------|------|--------|
| First-fit | 항상 힙 앞 | 보통 | 앞쪽에 단편화 집중 |
| Next-fit | 마지막 탐색 위치(`rover`) | 빠름 | 단편화 분산 |
| Best-fit | 힙 전체 | 느림 | 단편화 최소 |

Next-fit은 `rover` 덕분에 이미 탐색한 구간을 반복하지 않아 평균 탐색 비용이 줄어듭니다. 다만 메모리 활용률(utilization)은 Best-fit보다 낮을 수 있습니다.

</details>

---

**Q3. `rover`가 무엇이고, 어떤 문제를 해결하기 위해 도입했나요?**

<details>
<summary>정답 보기</summary>

`rover`는 마지막으로 가용 블록을 찾았던 위치를 기억하는 정적 포인터입니다. First-fit은 매번 힙 앞에서 탐색하기 때문에 앞쪽 블록들이 반복적으로 스캔되어 단편화가 앞에 몰립니다. `rover`를 이용하면 다음 탐색이 이전 탐색이 끝난 지점부터 시작되어 탐색 비용을 줄이고 단편화를 힙 전체에 고르게 분산시킵니다.

</details>

---

## 2. 헤더/풋터 (Boundary Tag) 관련

**Q4. 헤더와 풋터에 왜 size와 alloc 비트를 같이 저장하나요?**

<details>
<summary>정답 보기</summary>

`PACK(size, alloc) = size | alloc` 으로 두 정보를 하나의 4바이트 워드에 합칩니다. 8바이트 정렬을 사용하면 블록 크기는 항상 8의 배수여서 하위 3비트가 항상 0입니다. 이 중 최하위 1비트를 alloc 플래그로 재활용하면 별도 공간 없이 헤더/풋터 하나에 크기와 할당 여부를 모두 저장할 수 있습니다.

</details>

---

**Q5. 헤더와 풋터를 둘 다 두는 이유가 뭔가요? 헤더만 있으면 안 되나요?**

<details>
<summary>정답 보기</summary>

풋터(Boundary Tag)가 있어야 `PREV_BLKP(bp)`로 이전 블록을 O(1)에 찾을 수 있습니다. 풋터 없이 헤더만 있으면 이전 블록의 시작을 알 방법이 없어서, coalesce의 **Case 3(이전 블록과 병합)** 과 **Case 4(앞뒤 모두 병합)** 를 효율적으로 처리할 수 없습니다. 결국 즉각적인 양방향 병합(coalescing)을 O(1)에 가능하게 하는 것이 풋터의 핵심 존재 이유입니다.

</details>

---

**Q6. `GET_SIZE(p) = GET(p) & ~0x7` 에서 왜 `~0x7` 로 마스킹하나요?**

<details>
<summary>정답 보기</summary>

`~0x7 = 0xFFFFFFF8` 로, 하위 3비트를 0으로 만드는 마스크입니다. 8바이트 정렬에서 블록 크기는 항상 8의 배수이므로 하위 3비트는 크기 정보가 아닌 플래그(alloc 비트 등)로 사용됩니다. `& ~0x7` 로 이 플래그 비트를 제거하면 순수한 크기 값만 추출할 수 있습니다.

</details>

---

**Q7. WSIZE는 4, DSIZE는 8인데, 최소 블록 크기를 왜 16바이트(2*DSIZE)로 설정했나요?**

<details>
<summary>정답 보기</summary>

블록 하나의 최소 구성은 **헤더(4B) + payload + 풋터(4B)** 이고, payload는 8바이트 정렬 요건을 만족해야 하므로 최소 8B입니다. 따라서 4 + 8 + 4 = 16바이트가 최솟값입니다. `place`에서 분할 기준도 `(csize - asize) >= 2*DSIZE(16)` 으로 이 최소 크기를 보장합니다.

</details>

---

## 3. mm_init 관련

**Q8. `heap_listp`를 초기화할 때 왜 4*WSIZE(16바이트)를 할당하고, 각 워드에 무엇을 넣나요?**

<details>
<summary>정답 보기</summary>

16바이트를 할당해 4개의 워드를 다음과 같이 초기화합니다:

```
[0      ][PACK(8,1)][PACK(8,1)][PACK(0,1)]
 정렬패딩  프롤로그헤더  프롤로그풋터   에필로그헤더
```

- **정렬 패딩(0)**: 이후 payload가 8바이트 경계에 맞도록 앞에 4바이트 여백을 둡니다.
- **프롤로그 헤더/풋터**: 크기 8, alloc=1인 특수 블록. 힙 앞 경계를 나타냅니다.
- **에필로그 헤더**: 크기 0, alloc=1. 힙 순회 시 종료 조건(`GET_SIZE == 0`)으로 사용됩니다.

`heap_listp`는 프롤로그 풋터를 가리키도록 `+= 2*WSIZE` 합니다.

</details>

---

**Q9. 프롤로그 블록과 에필로그 블록이 왜 필요한가요?**

<details>
<summary>정답 보기</summary>

- **프롤로그 블록**: alloc=1로 설정된 더미 블록으로, `coalesce`에서 `PREV_BLKP`로 힙 앞 경계를 넘어가려 할 때 "이미 할당된 블록"으로 인식되어 병합을 막는 경계 보호 역할을 합니다.
- **에필로그 블록**: `GET_SIZE == 0` 조건으로 힙 순회(`find_fit`, `extend_heap`)의 종료 지점을 알립니다. 힙이 확장되면 에필로그는 새 가용 블록 뒤로 밀려납니다.

</details>

---

**Q10. `extend_heap(CHUNKSIZE/WSIZE)`를 init에서 호출하는 이유가 뭔가요?**

<details>
<summary>정답 보기</summary>

초기 힙에는 가용 블록이 전혀 없기 때문에, 첫 `mm_malloc` 호출 시 바로 `extend_heap`이 호출될 수밖에 없습니다. init 시점에 미리 CHUNKSIZE(1KB)만큼의 가용 블록을 생성해 두면 초반 할당 요청들을 힙 확장 없이 처리할 수 있고, 자주 발생하는 소규모 시스템 콜(sbrk)을 줄일 수 있습니다.

</details>

---

## 4. extend_heap 관련

**Q11. `words % 2 ? (words+1)*WSIZE : words*WSIZE` 계산은 왜 하나요?**

<details>
<summary>정답 보기</summary>

힙은 항상 8바이트(더블워드) 정렬을 유지해야 합니다. 1워드(WSIZE)는 4바이트이므로 홀수 개의 워드를 요청하면 총 크기가 4의 홀수 배가 되어 8바이트 정렬이 깨집니다. 따라서 홀수 워드 요청이 들어오면 1을 더해 짝수로 맞춰 항상 8바이트의 배수 크기로 힙을 확장합니다.

</details>

---

**Q12. `extend_heap` 마지막에 `coalesce(bp)`를 호출하는 이유가 뭔가요?**

<details>
<summary>정답 보기</summary>

힙 확장 직전에 힙의 마지막 블록이 가용 블록이었을 수 있습니다. 에필로그 바로 앞 블록이 가용 상태라면 새로 확장된 블록과 합칠 수 있는데, `coalesce`를 호출하지 않으면 두 인접한 가용 블록이 분리된 채로 남게 됩니다(false fragmentation). `coalesce(bp)`로 즉시 병합해 큰 가용 블록을 확보합니다.

</details>

---

## 5. find_fit (Next-fit) 관련

**Q13. Next-fit에서 `rover`를 두 번 루프로 나눠서 탐색하는 이유가 뭔가요?**

<details>
<summary>정답 보기</summary>

Next-fit은 `rover`부터 탐색을 시작하므로 `rover` 앞쪽에 있는 가용 블록을 놓칠 수 있습니다. 이를 보완하기 위해:

- **루프 1**: `rover` → 힙 끝까지 탐색
- **루프 2**: 루프 1에서 못 찾으면 힙 시작(`heap_listp`) → `rover` 직전까지 탐색

이 순환 탐색으로 힙 전체를 한 바퀴 빠짐없이 스캔합니다. 두 루프 모두 실패하면 `NULL`을 반환해 `mm_malloc`이 `extend_heap`을 호출하도록 합니다.

</details>

---

**Q14. `rover`를 업데이트하는 조건이 무엇인가요?**

<details>
<summary>정답 보기</summary>

`find_fit`에서 조건을 만족하는 가용 블록을 찾았을 때 `rover = bp`로 갱신합니다. 또한 `coalesce`에서 병합이 발생할 경우 `rover`가 병합된 블록 범위 안에 있으면 병합된 블록의 시작 주소(`bp`)로 재조정합니다. 이는 `rover`가 사라진 블록(병합으로 흡수된 블록)을 가리키는 dangling pointer 상황을 방지합니다.

</details>

---

## 6. place (분할) 관련

**Q15. 블록을 분할할 때 기준이 `(csize - asize) >= 2*DSIZE`인 이유가 뭔가요?**

<details>
<summary>정답 보기</summary>

분할 후 남는 조각이 유효한 가용 블록이 되려면 최소 블록 크기(16바이트)를 충족해야 합니다. 최소 블록은 헤더(4) + 최소 payload(8) + 풋터(4) = 16바이트 = `2*DSIZE`입니다. 남는 공간이 이보다 작으면 가용 블록으로 쓸 수 없으므로 분할하지 않고 블록 전체를 할당합니다(내부 단편화 허용).

</details>

---

**Q16. 분할 후 남은 가용 블록에 `rover`를 따로 업데이트하지 않는 이유가 뭔가요?**

<details>
<summary>정답 보기</summary>

`rover`는 `find_fit`에서 탐색 성공 시 `rover = bp`로 이미 갱신됩니다. `place`는 `find_fit`이 반환한 블록에 대해 할당과 분할만 수행하는 함수이므로, 분할된 나머지 가용 블록의 위치를 굳이 `rover`에 반영할 필요가 없습니다. 다음 `find_fit` 호출 때 자연스럽게 해당 위치부터 탐색이 시작됩니다.

</details>

---

## 7. mm_malloc 관련

**Q17. `asize`를 계산하는 공식 `DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE)`는 어떻게 동작하나요?**

<details>
<summary>정답 보기</summary>

단계별로 분해하면:

1. `size + DSIZE`: 요청 크기에 헤더(4B) + 풋터(4B) = 8B(DSIZE) 오버헤드를 더합니다.
2. `+ (DSIZE - 1)`: `+ 7`을 더해 정수 나눗셈의 올림(ceiling)을 구현합니다.
3. `/ DSIZE`: 8로 나눠 8바이트 단위 개수를 구합니다.
4. `* DSIZE`: 다시 8을 곱해 8의 배수 바이트 크기로 만듭니다.

예: `size=9` → `(9+8+7)/8*8 = 24/8*8 = 24` → asize=24

</details>

---

**Q18. 가용 블록을 못 찾았을 때 `MAX(asize, CHUNKSIZE)`만큼 힙을 늘리는 이유가 뭔가요?**

<details>
<summary>정답 보기</summary>

`sbrk` 시스템 콜은 비용이 크기 때문에 너무 자주 호출하면 성능이 떨어집니다. `asize`가 CHUNKSIZE(1KB)보다 작은 경우에도 최소 CHUNKSIZE만큼 한번에 확장해두면, 이후 소규모 할당 요청들을 추가적인 `sbrk` 없이 처리할 수 있어 처리량(throughput)이 향상됩니다. `asize`가 CHUNKSIZE보다 크면 요청 크기에 맞게 더 크게 확장합니다.

</details>

---

## 8. mm_free / coalesce 관련

**Q19. coalesce의 4가지 경우(case)를 설명해주세요.**

<details>
<summary>정답 보기</summary>

| Case | 이전 블록 | 다음 블록 | 처리 |
|------|----------|----------|------|
| 1 | 할당됨 | 할당됨 | 병합 없음, bp 그대로 반환 |
| 2 | 할당됨 | 가용 | bp의 헤더와 다음 블록 풋터를 합산 크기로 갱신 |
| 3 | 가용 | 할당됨 | 이전 블록 헤더와 bp의 풋터를 합산 크기로 갱신, bp를 이전 블록으로 이동 |
| 4 | 가용 | 가용 | 이전 블록 헤더와 다음 블록 풋터를 합산 크기로 갱신, bp를 이전 블록으로 이동 |

각 case에서 `rover`가 병합 범위 안에 있으면 새 블록 시작 주소로 재조정합니다.

</details>

---

**Q20. `coalesce` 내에서 `rover`를 재조정하는 이유가 뭔가요?**

<details>
<summary>정답 보기</summary>

병합으로 인해 기존 블록들이 하나의 큰 블록으로 합쳐지면, `rover`가 가리키던 블록이 더 이상 독립된 블록으로 존재하지 않을 수 있습니다. `rover`가 그 범위 안을 가리키고 있다면 `find_fit`에서 잘못된 위치를 읽게 됩니다. 따라서 `rover >= bp && rover < NEXT_BLKP(bp)` 조건으로 확인 후, 병합된 새 블록의 시작 주소로 재설정해 올바른 탐색 기준점을 유지합니다.

</details>

---

**Q21. `prev_alloc`을 읽을 때 이전 블록의 **풋터**에서 읽는 이유가 뭔가요?**

<details>
<summary>정답 보기</summary>

Implicit Free List에서는 이전 블록의 시작 주소를 직접 저장하지 않습니다. 대신 `bp - DSIZE`에 위치한 이전 블록의 풋터에서 크기를 읽고(`PREV_BLKP`), 그 풋터에서 alloc 비트를 확인합니다(`FTRP(PREV_BLKP(bp))`). 풋터가 이전 블록의 끝에 위치해 현재 블록에서 O(1)으로 접근 가능하기 때문에 이 방식이 동작합니다.

</details>

---

## 9. mm_realloc 관련

**Q22. realloc에서 "제자리 확장"을 시도하는 이유가 뭔가요?**

<details>
<summary>정답 보기</summary>

기본 fallback(새 블록 할당 + memcpy + 기존 블록 해제)은 메모리를 두 배로 사용하는 순간이 발생하고, memcpy 비용도 큽니다. 반면 제자리 확장은:

- **① 다음 블록이 가용**: 인접 가용 블록을 흡수해 메모리 이동 없이 크기를 늘립니다.
- **② 힙 끝 블록**: sbrk로 필요한 만큼만 힙을 늘려 불필요한 메모리 낭비를 줄입니다.

두 방식 모두 기존 데이터를 이동시키지 않아 memcpy 비용이 없고 메모리 활용률도 높습니다.

</details>

---

**Q23. 제자리 확장 ② (에필로그 직전, `next_size == 0`)에서 split을 하지 않는 이유가 뭔가요?**

<details>
<summary>정답 보기</summary>

에필로그 바로 앞에 있다는 것은 힙의 끝에 위치한 블록이라는 의미입니다. `sbrk`로 필요한 만큼(`asize - old_size`)만 힙을 늘리면 블록이 정확히 `asize`가 되므로 남는 공간이 없습니다. 남는 공간이 없으니 분할할 필요가 없고, 힙 확장 후 에필로그만 새 위치에 다시 세워주면 됩니다.

</details>

---

**Q24. fallback에서 `copy_size = old_size - WSIZE`로 계산하는 이유가 뭔가요?**

<details>
<summary>정답 보기</summary>

`GET_SIZE(HDRP(ptr))`로 얻은 `old_size`는 헤더(4B = WSIZE)를 포함한 블록 전체 크기입니다. memcpy로 복사해야 할 대상은 실제 데이터가 있는 payload 영역이므로 헤더 크기를 빼야 합니다. 즉 `copy_size = old_size - WSIZE`가 실제 payload 크기입니다. (풋터는 payload 뒤에 있으므로 payload 크기 계산에서 제외됩니다.)

</details>

---

## 10. 성능 및 개선 가능성

**Q25. 현재 구현의 메모리 활용률(utilization)과 처리량(throughput) 병목은 어디인가요?**

<details>
<summary>정답 보기</summary>

- **처리량 병목**: Implicit List는 `find_fit`이 O(n) 탐색입니다. 힙에 블록이 많아질수록 탐색 시간이 선형으로 증가합니다.
- **활용률 병목**: 블록마다 헤더(4B) + 풋터(4B) = 8바이트 오버헤드가 발생합니다. 작은 크기(예: 1바이트)를 할당해도 최소 16바이트 블록이 필요하므로 내부 단편화가 큽니다. 또한 Next-fit은 Best-fit에 비해 외부 단편화도 다소 많습니다.

</details>

---

**Q26. 더 개선한다면 어떤 방향으로 바꾸겠어요?**

<details>
<summary>정답 보기</summary>

- **Explicit Free List**: 가용 블록에 prev/next 포인터를 추가해 가용 블록만 연결 → `find_fit`이 O(free blocks)으로 단축
- **Segregated Free List**: 크기 구간별로 별도 리스트 관리 → 탐색 범위를 크게 줄이고 단편화 개선
- **풋터 최적화(Footer Elimination)**: 할당된 블록에는 풋터를 제거하고 다음 블록 헤더에 "이전 블록 alloc" 비트를 저장 → 블록당 4바이트 절약
- **Best-fit 전환**: 탐색 비용은 늘지만 단편화를 줄여 활용률 향상 가능

</details>
