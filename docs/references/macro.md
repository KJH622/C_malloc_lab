# malloc lab 매크로 정리

`malloc-lab/mm.c`에 있는 매크로들을 보기 편하게 목록 느낌으로 정리한 문서입니다.

## 자주 나오는 단위

- `WSIZE`
  한 워드 크기입니다. 여기서는 `4`바이트입니다.
- `DSIZE`
  더블 워드 크기입니다. 여기서는 `8`바이트입니다.
- `ALIGNMENT`
  블록 정렬 단위입니다. 여기서는 `8`바이트 정렬을 맞춥니다.

## 각 매크로를 감으로 이해하기

- `ALIGN(size)`
  `size`를 8의 배수로 올려 맞춥니다.
  쉽게 말하면: "8바이트 경계에 맞게 크기를 올린다"
  예: `ALIGN(13)` -> `16`

- `MAX(x, y)`
  둘 중 더 큰 값을 고릅니다.
  쉽게 말하면: "최소 기준보다 작지 않게 큰 값 사용"

- `PACK(size, prev_alloc, alloc)`
  블록 크기와 할당 비트들을 하나의 값으로 합칩니다.
  쉽게 말하면: "크기 + 이전 블록 상태 + 현재 블록 상태를 한 칸에 포장"
  예: `PACK(16, 1, 0)` -> "크기 16, 이전 블록 allocated, 현재 블록 free"

- `GET(p)`
  주소 `p`에 저장된 4바이트 값을 읽습니다.
  쉽게 말하면: "헤더나 풋터에 적힌 값을 꺼내기"

- `PUT(p, val)`
  주소 `p`에 `val`을 기록합니다.
  쉽게 말하면: "헤더나 풋터에 값 써 넣기"

- `GET_SIZE(p)`
  헤더/풋터 값에서 블록 크기만 꺼냅니다.
  쉽게 말하면: "상태 비트는 빼고 크기만 보기"

- `GET_ALLOC(p)`
  현재 블록이 할당 상태인지 확인합니다.
  쉽게 말하면: "이 블록 지금 사용 중이야?"
  반환: `1`이면 allocated, `0`이면 free

- `GET_PREV_ALLOC(p)`
  이전 블록이 할당 상태인지 확인합니다.
  쉽게 말하면: "바로 앞 블록이 사용 중인지 확인"
  반환: `1`이면 이전 블록 allocated, `0`이면 free

- `HDRP(bp)`
  `bp`가 가리키는 블록의 헤더 주소입니다.
  쉽게 말하면: "payload 포인터에서 4바이트 뒤로 가면 헤더"

- `FTRP(bp)`
  `bp`가 가리키는 블록의 풋터 주소입니다.
  쉽게 말하면: "블록 끝부분의 footer 위치 찾기"
  주의: 이 구현은 allocated 블록에는 footer를 생략할 수 있으므로, `FTRP(bp)`는 주로 free 블록에서 의미 있게 사용됩니다.

- `NEXT_BLKP(bp)`
  현재 블록의 다음 블록 payload 주소입니다.
  쉽게 말하면: "현재 블록 크기만큼 앞으로 점프하면 다음 블록"

- `PREV_BLKP(bp)`
  현재 블록의 이전 블록 payload 주소입니다.
  쉽게 말하면: "바로 앞 블록의 크기를 이용해 이전 블록 시작점 찾기"
  주의: 이전 블록의 크기를 알아야 하므로, 앞 블록이 free일 때 특히 유용합니다.

## free list 관련 매크로

- `PRED_FIELD(bp)`
  free 블록 payload 안에서 predecessor 포인터가 저장되는 위치입니다.
  쉽게 말하면: "이 free 블록의 이전 노드 주소 칸"

- `SUCC_FIELD(bp)`
  free 블록 payload 안에서 successor 포인터가 저장되는 위치입니다.
  쉽게 말하면: "이 free 블록의 다음 노드 주소 칸"

- `PTR_TO_OFF(ptr)`
  실제 포인터를 힙 시작점 기준 offset 값으로 바꿉니다.
  쉽게 말하면: "주소를 거리 값으로 바꿔 저장"

- `OFF_TO_PTR(off)`
  offset 값을 다시 실제 포인터로 바꿉니다.
  쉽게 말하면: "거리 값을 다시 진짜 주소로 복원"

- `SET_PRED(bp, ptr)`
  현재 free 블록의 predecessor를 `ptr`로 설정합니다.

- `SET_SUCC(bp, ptr)`
  현재 free 블록의 successor를 `ptr`로 설정합니다.

- `PRED(bp)`
  현재 free 블록의 predecessor 포인터를 읽습니다.

- `SUCC(bp)`
  현재 free 블록의 successor 포인터를 읽습니다.

## 정말 짧게 외우기

- `HDRP(bp)`
  `bp`가 가리키는 블록의 헤더 주소
- `FTRP(bp)`
  `bp`가 가리키는 블록의 풋터 주소
- `NEXT_BLKP(bp)`
  현재 블록 다음 블록의 payload 주소
- `PREV_BLKP(bp)`
  현재 블록 이전 블록의 payload 주소
- `PACK(size, prev_alloc, alloc)`
  "크기 size, 이전 블록 상태, 현재 블록 상태"를 헤더 형식으로 포장
- `GET_SIZE(p)`
  헤더 값에서 블록 크기만 추출
- `GET_ALLOC(p)`
  현재 블록이 할당 상태인지 추출
- `GET_PREV_ALLOC(p)`
  이전 블록이 할당 상태인지 추출

## 메모리 그림으로 보기

```text
[ Header | Payload ... | Footer ]
          ^
          bp
```

- `HDRP(bp)`
  `bp` 바로 앞의 header
- `FTRP(bp)`
  블록 끝쪽의 footer
- `NEXT_BLKP(bp)`
  현재 블록 크기만큼 이동한 다음 블록의 `bp`

## 한 줄 감각

- 헤더에서 크기와 상태 비트 읽기
- 현재 블록 기준으로 앞뒤 블록 위치 계산하기
- free list 연결 관계 읽고 쓰기
