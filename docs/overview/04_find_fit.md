# 🔍 find_fit — Next-fit 가용 블록 탐색

> 요청 크기(asize)를 수용할 수 있는 가용 블록을 찾는 함수.
> **Next-fit** 전략: 마지막으로 찾은 위치(rover)에서 탐색 재개.

---

## 코드

```c
static void *find_fit(size_t asize)
{
    void *bp;

    // 루프 1: rover부터 힙 끝까지
    for (bp = rover; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            rover = bp;  // 다음 탐색 시작점 갱신
            return bp;
        }
    }

    // 루프 2: 힙 처음부터 rover까지 (wrap-around)
    for (bp = heap_listp; bp < (void *)rover; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            rover = bp;
            return bp;
        }
    }

    return NULL;  // 적합한 블록 없음
}
```

---

## 탐색 전략 비교

| 전략 | 설명 | 장점 | 단점 |
|------|------|------|------|
| **First-fit** | 힙 처음부터 탐색 | 구현 단순 | 앞부분에 작은 조각 누적 |
| **Best-fit** | 가장 딱 맞는 블록 탐색 | 단편화 최소 | O(n) 전체 탐색 |
| **Next-fit** | 마지막 위치부터 탐색 | 속도+분산의 균형 | rover 관리 필요 |

---

## Next-fit 동작 시각화

### 정상 탐색 (rover 이후에서 발견)

```
힙:
heap_listp                rover           발견
│                         │               │
▼                         ▼               ▼
[A:16][F:32][A:24][A:16][F:48][A:8][F:64][F:128][에필로그]

asize = 100 요청
→ 루프1: rover(F:48)부터 시작
→ F:48 → 48 < 100 ✗
→ A:8  → 할당됨 ✗
→ F:64 → 64 < 100 ✗
→ F:128 → 128 >= 100 ✓  rover = 여기  return bp
```

### Wrap-around 탐색 (rover 이후에 없을 때)

```
힙:
heap_listp    발견        rover
│             │           │
▼             ▼           ▼
[A:16][F:256][A:24][A:16][F:48][A:8][에필로그]

asize = 200 요청
→ 루프1: rover(F:48)부터 → F:48 < 200 ✗ → 끝까지 없음
→ 루프2: heap_listp부터 rover까지
→ A:16 → 할당됨 ✗
→ F:256 → 256 >= 200 ✓  rover = 여기  return bp
```

### 둘 다 실패

```
→ NULL 반환
→ mm_malloc에서 extend_heap 호출
```

---

## rover 갱신의 의미

```
[연속 malloc 시나리오]

1. malloc(100) → rover = 블록A 위치에서 탐색 → 블록B 발견 → rover = 블록B
2. malloc(50)  → 블록B부터 탐색 → 블록C 발견 → rover = 블록C
3. malloc(200) → 블록C부터 탐색 → ...

효과: 탐색이 힙 전체를 균등하게 사용하게 됨
     (First-fit처럼 앞부분만 조각나는 문제 완화)
```

---

## coalesce와의 rover 연동

```c
// coalesce에서 rover가 병합되는 범위 안에 있으면 리셋
if (rover >= (char *)bp && rover < (char *)NEXT_BLKP(bp))
    rover = bp;
```

```
병합 전:
[가용:32][rover→][가용:64] → 병합 후 [가용:96]

rover가 가리키던 주소는 더 이상 블록 경계가 아님!
→ rover를 병합된 블록 시작으로 리셋 ✓
```

---

## 주석 처리된 first-fit 버전 (참고)

```c
// first-fit: 앞에서부터 처음 맞는 블록 반환
static void *find_fit(size_t asize) {
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= asize))
            return bp;
    }
    return NULL;
}
```

현재 코드는 이 first-fit을 **next-fit으로 업그레이드**한 버전입니다.
