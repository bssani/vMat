# V-Mat 사용법

월드에 배치된 mesh를 선택하고 material을 누르면, 그 mesh의 **원본 StaticMesh 에셋** material이 바뀐다.
다른 플러그인이나 material library 없이 이 플러그인만으로 쓴다.

## 열기

- 레벨 에디터 위쪽 툴바, Play 버튼 오른쪽에 있는 **V-Mat** 버튼
- 또는 메뉴 **Tools → V-Mat**

창 맨 위에 탭이 두 개 있다.

- **Palette**: preset에 넣어둔 material을 선택한 mesh에 입힌다. slot이 여러 개면 slot 전부에 같은 걸 넣는다.
- **Multi-slot**: 레벨에서 slot이 여러 개인 mesh를 찾아서 slot마다 따로 넣는다.

## Palette 탭

### Preset (material 모음집)

맨 위 드롭다운에서 preset을 고른다. 드롭다운 오른쪽 아이콘 버튼은 순서대로 이렇다.

- 연필: 이름 바꾸기. 입력하고 Enter, 취소는 Esc
- +: 빈 preset 만들기
- 겹친 네모: 지금 preset 복사
- 휴지통: 지금 preset 삭제 (확인창 뜸)

바꾸는 즉시 저장된다. 저장 버튼은 따로 없다. 에디터를 껐다 켜면 마지막에 쓰던 preset이 다시 열린다.

### preset 저장 위치와 다른 프로젝트로 옮기기

preset은 **플러그인 폴더 안** `Config/VMatPresets.json`에 저장된다.

- 플러그인 폴더(`Plugins/VMat`)를 다른 프로젝트에 복사하면 preset도 같이 간다. git으로 받는 사람도 같은 preset을 받는다.
- material은 `/Game/...` 경로로 저장된다. 다른 프로젝트에도 같은 경로에 material이 있으면 그대로 쓰인다. 없으면 타일에 **Missing**이 뜬다.
- 마지막에 쓰던 preset은 사람마다 달라서 json에 안 들어간다. 프로젝트의 `Saved/Config/.../EditorPerProjectUserSettings.ini`에 따로 저장된다.
- json을 직접 고치다 깨지면 V-Mat이 알림을 띄우고, 고칠 때까지 preset을 저장하지 않는다. 깨진 파일을 빈 내용으로 덮어쓰지 않으려고 그렇다.

예전 버전은 프로젝트 `Config/DefaultEditor.ini`에 저장했다. 플러그인에 json이 아직 없으면, 처음 열 때 그 ini의 preset을 json으로 한 번 옮긴다. ini는 건드리지 않는다.

### Material 추가 / 삭제

- 콘텐츠 브라우저에서 material을 **끌어다 가운데 영역에 놓으면** 들어간다. 여러 개를 한 번에 놓아도 되고, material instance도 된다.
- 또는 콘텐츠 브라우저에서 고르고 **Add selected**를 누른다.
- 이미 있는 건 또 안 들어간다.
- 타일에 마우스를 올리면 오른쪽 위에 **X**가 뜬다. 누르면 preset에서 빠진다. material 에셋 자체는 안 지워진다.
- 타일을 우클릭하면 메뉴가 뜬다. 입히기 / 콘텐츠 브라우저에서 보기 / preset에서 빼기

material 에셋을 지우거나 옮겨서 못 찾으면 타일에 **Missing**이 뜨고 눌리지 않는다. preset에서는 안 지워진다. X는 항상 보이니까 정리하면 된다.

### Material 입히기

1. 레벨에서 mesh actor를 하나 또는 여러 개 선택한다.
2. 아래 요약에서 몇 개가 바뀔지 확인한다. 선택한 actor 이름은 **Selected actors**를 펼치면 보인다.
3. material 타일을 누른다.

누르면 이렇게 된다.

- 선택한 actor의 원본 mesh 에셋 material이 바뀌고, 바뀐 mesh는 바로 저장된다.
- 레벨은 자동 저장 안 된다. 평소처럼 Ctrl+S로 저장한다.
- **Ctrl+Z 한 번**이면 누른 게 통째로 되돌아간다. 이미 저장된 mesh는 되돌린 다음 다시 저장해야 한다.
- 이미 같은 material이면 아무것도 안 바뀐다.
- mesh 수백 개를 한 번에 바꾸면 몇 초 걸릴 수 있다. 오래 걸리면 진행창이 뜬다.

### 주의

- **같은 mesh를 쓰는 다른 actor도 같이 바뀐다.** 원본을 바꾸는 거라서 그렇다. 볼트 하나만 선택해도 같은 볼트 mesh 쓰는 건 다 바뀐다. 알림에 레벨에서 같이 바뀐 actor 수가 나온다.
- **slot이 여러 개인 mesh는 slot 전부에 같은 material이 들어간다.** slot이 5개면 5개 다 바뀐다. 선택에 이런 mesh가 있으면 요약 줄에 몇 개인지 나온다. 옆의 **Edit per slot**을 누르면 Multi-slot 탭에서 그 mesh들이 골라진 채로 열리고, 거기서 slot별로 넣으면 된다.
- 이런 건 **건너뛴다**. 건너뛴 건 아래 요약과 알림에 나온다.
  - **/Game 밖에 있는 mesh**: 엔진 기본 도형(Cube 같은 거)이나 다른 플러그인 에셋은 안 건드린다.
  - **mesh가 없는 actor**: Light, PlayerStart 같은 거
- 예전 툴처럼 actor(component)에 따로 material이 덮여 있으면 그건 지운다. 남아 있으면 원본을 바꿔도 월드에 안 보이기 때문이다.

## Multi-slot 탭

지금 열린 레벨에서 material slot이 2개 이상인 mesh를 찾고, slot마다 따로 material을 넣는다. /Game 안에 있는 mesh만 나온다.

화면은 이렇게 나뉜다. 사이 경계를 끌면 크기가 바뀐다.

1. **mesh 목록** (왼쪽 위): slot 여러 개인 mesh
2. **Slots** (왼쪽 아래): 위에서 고른 mesh의 slot
3. **미리보기** (오른쪽): 고른 mesh를 3D로 보여준다.
4. **material 줄** (맨 아래): 지금 preset의 material. Palette 탭과 같은 preset을 쓰고, 오른쪽 드롭다운으로 바꿀 수 있다.

### 미리보기

- mesh를 고르면 오른쪽에 그 mesh가 나온다. 여러 개 고르면 마지막에 누른 mesh가 나온다.
- Slots에서 slot을 고르면 고른 slot만 원래 material로 보이고 나머지는 어두운 회색이 된다. slot 이름만 봐서는 어느 부분인지 모를 때 쓰면 된다.
- material을 넣으면 미리보기에도 바로 반영된다.
- 왼쪽 버튼으로 끌면 돌아가고, 휠로 확대/축소, F를 누르면 mesh를 다시 가운데로 가져온다.
- 오른쪽 아래 **Zoom** 칸에서 배율을 정한다. 100%면 mesh가 창에 딱 맞는다. 숫자를 쓰거나 좌우로 끌면 되고, 휠로 바꿔도 이 숫자가 같이 바뀐다. 옆 버튼은 100%로 되돌린다.
- 다음 mesh도 같은 배율로 열린다. 배율은 개인 설정에 저장돼서 에디터를 껐다 켜도 남는다.
- 휠과 이동은 부품 크기에 맞춰 움직인다. 몇 cm짜리 부품도 한 칸에 지나쳐버리지 않는다.
- mesh를 안 고르면 미리보기는 그리지 않는다.

### slot에 material 넣기

1. 위 목록에서 mesh를 고른다.
2. 가운데 **Slots**에서 바꿀 slot을 고른다. Ctrl/Shift로 여러 개 골라도 된다.
3. 아래 material을 누른다. 고른 slot에만 들어간다.

- slot을 안 고르면 아래 material은 흐리게 나오고 눌리지 않는다.
- 넣고 나면 slot 목록이 새로 그려지지만 고른 slot은 그대로 골라져 있다.
- Palette 탭처럼 바뀐 mesh는 바로 저장되고, Ctrl+Z 한 번이면 통째로 되돌아간다.
- 레벨에서 그 mesh를 쓰는 actor에 그 slot override가 남아 있으면 지운다. 남아 있으면 바꿔도 안 보이기 때문이다.
- mesh 수백 개를 한 번에 바꾸면 몇 초 걸릴 수 있다. 오래 걸리면 진행창이 뜬다.

### mesh 여러 개를 한 번에

mesh를 하나만 고르면 slot이 번호대로 나온다. 여러 개 고르면 slot을 묶어서 보여준다. 묶는 기준은 Slots 오른쪽에서 고른다.

- **By material**: 지금 같은 material이 들어있는 slot끼리 묶는다. 예를 들어 CAD에서 들어온 빨간 도장 material이 들어있는 slot을 전부 한 줄로 보고 한 번에 바꾼다.
- **By slot name**: slot 이름이 같은 것끼리 묶는다. Datasmith CAD import는 CAD material ID를 slot 이름으로 쓰기 때문에, 원래 같은 CAD material이던 slot이 이미 다른 material로 바꿨어도 계속 한 줄로 모인다.

mesh 목록에서 Ctrl+A로 전부 고르고 By material로 보면, 레벨 전체에서 그 material이 들어있는 slot을 한 번에 바꿀 수 있다.

### 목록 다루기

- 탭을 처음 열 때 한 번 훑는다. 그 뒤로 레벨을 바꿨으면 **Scan level**을 눌러 다시 훑는다.
- 훑은 결과 위에 몇 개를 몇 ms 걸려 훑었는지 나온다. 레벨에 올라온 mesh는 이미 메모리에 있어서 에셋을 새로 불러오지 않는다. mesh 몇천 개짜리 레벨도 금방 끝난다.
- 검색칸에 mesh 이름이나 폴더 일부를 쓰면 걸러진다.
- 열 제목(Mesh / Slots / Actors / Folder)을 누르면 그 기준으로 정렬된다.
- 줄에 마우스를 올리면 slot마다 지금 들어있는 material이 보인다.
- 줄을 **더블클릭**하면 그 mesh를 쓰는 actor가 레벨에서 전부 선택되고 카메라가 그쪽으로 간다. Ctrl/Shift로 여러 줄 골라서 **Select in level**을 눌러도 된다.
- 폴더 아이콘은 콘텐츠 브라우저에서 보기, 연필 아이콘은 Static Mesh Editor 열기다. 에디터는 한 번에 10개까지만 연다.

World Partition 레벨이면 지금 로드된 actor만 훑는다.
