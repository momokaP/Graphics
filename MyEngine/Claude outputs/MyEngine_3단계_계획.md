# MyEngine 3단계 — Scene / Assets / Editor (라이트)

기간: 약 1주
목표: **여러 물체를 각자 다른 위치·머티리얼로 배치하고, 카메라로 둘러보며, 에디터 UI로 고칠 수 있다.**

JSON 저장/로드는 7단계에서 한다. 3단계에서는 그걸 하기 쉬운 데이터 구조만 만든다.

---

## 현재 상태 (출발점)

씬 관련 데이터가 전부 `SandboxApp`에 흩어져 있다.

| 항목 | 지금 | 문제 |
| --- | --- | --- |
| 카메라 | `m_viewRot` 하나 (Y축 회전만) | 이동·상하 회전 불가 |
| 물체 | `m_modelTranslation/Rotation/Scaling` 한 세트 | 모든 메시가 같은 위치 |
| 머티리얼 | `RenderScene::material` 하나 (패스당 1회 설정) | 물체별 머티리얼 불가 |
| 에셋 | `SandboxApp::Mesh` 구조체, `m_meshes` | 앱이 GPU 리소스를 직접 소유 |
| 추출 | `SandboxApp::BuildRenderScene` | 앱이 렌더러 입력 형식을 알아야 함 |

---

## 단계 구성

| 단계 | 내용 | 새로 생기는 것 | 기간 |
| --- | --- | --- | --- |
| 3-1 | 플라이 카메라: WASD + 우클릭 마우스 회전 | `Engine/Scene/Camera.h/.cpp` | 0.5일 |
| 3-2 | Transform + Entity + Scene: 물체 여러 개 (박스, 구, 모델) | `Engine/Scene/Transform.h`, `Scene.h/.cpp` | 1일 |
| 3-3 | AssetManager: 메시·텍스처를 핸들로 관리, 경로 캐시 | `Engine/Assets/AssetManager.h/.cpp` | 1일 |
| 3-4 | 물체별 머티리얼: Forward가 Draw마다 `RP_Material` 설정 | `Material` 구조체, `DrawItem` 확장 | 0.5일 |
| 3-5 | 추출 분리: Scene + Assets → RenderScene을 엔진 함수로 | `SceneRenderer` (또는 `ExtractRenderScene`) | 0.5일 |
| 3-6 | 에디터 UI: 아웃라이너, 인스펙터, 통계 패널 | `Editor/EditorUI.h/.cpp` | 1~1.5일 |
| 3-7 | (선택) 피킹 또는 기즈모: 클릭으로 선택, ImGuizmo | | 0.5일 |

3-5가 끝나면 `SandboxApp`은 "씬을 만들고, 카메라 입력을 넘기고, `Render`를 부르는" 수십 줄로 줄어든다.

---

## 설계 원칙

- **ECS는 쓰지 않는다.** `std::vector<Entity>`에 Transform·메시 핸들·머티리얼 핸들을 넣는 단순한 구조. 1주 안에 끝낼 수 있고, 6A(애니메이션)·6B(물리)에서 컴포넌트가 필요해지면 그때 확장한다. ("왜 ECS를 안 썼나" → 규모 대비 비용)
- **`RenderScene`은 여전히 스냅샷.** Scene과 AssetManager는 렌더러를 모르고, 추출 단계(3-5)만 둘을 잇는다.
- **입력 장치와 로직 분리.** `Camera`는 `CameraInput`만 받고 ImGui/Win32를 모른다.
- **핸들, 포인터 아님.** Scene은 메시·텍스처를 정수 핸들로 가리킨다. 포인터는 추출 시점에만 풀어서 `DrawItem`에 넣는다. (멀티스레드 렌더링 대비, 주제 5 질문 1)
- **리소스 해제는 지연 해제로.** 에셋을 지울 때 GPU가 아직 쓰고 있을 수 있다. (1-E의 #921, #1001)

### 1~2단계에서 계속 지킬 규칙
1. 패스는 공유 리소스를 직접 전환하지 않는다. 전환은 `RecordFrame`이 한다.
2. 패스 사이에 주고받는 텍스처는 `RenderTargets`를 통해 넘긴다.
3. 셰이더의 텍스처 샘플링은 헬퍼 함수로 감싼다.
4. 머티리얼 텍스처 목록은 한 곳에 모아 둔다.

---

## 3-1. 플라이 카메라

**목표:** `m_viewRot`을 위치·yaw·pitch를 가진 `Camera` 클래스로 교체. 기본 위치는 기존과 같은 (0, 0, −2), +Z를 봄.

**핵심**
- 왼손 좌표계: `XMMatrixLookToLH`, `XMMatrixPerspectiveFovLH` (SimpleMath `CreateLookAt`은 오른손 좌표계)
- 오른쪽 방향 = `UnitY.Cross(forward)` (왼손)
- pitch는 ±89도로 제한
- 입력은 ImGui `io`에서 읽고, `WantCaptureMouse`/`WantCaptureKeyboard`가 참이면 무시
- `eyeWorld`는 `view.Invert()` 대신 `m_camera.Position()`

**수정 파일 (예측):** `Camera.h`, `Camera.cpp`(신규), `SandboxApp.h`, `SandboxApp.cpp`, `MyEngine.vcxproj`, `.filters`

**점검**
- 망가뜨리기: `View()`를 SimpleMath `Matrix::CreateLookAt`(오른손)으로 바꾸면?
- 질문 1: pitch를 ±89도로 제한하는 이유는?
- 질문 2: `eyeWorld`를 `view.Invert()`로 구하던 것과 `Position()`으로 바꾼 것의 차이는?

---

## 3-2. Transform + Entity + Scene

**목표:** 물체 여러 개(박스, 구, 모델)를 각자 다른 위치에 배치.

**핵심**
- `Transform { Vector3 position; Vector3 rotationEuler(또는 Quaternion); Vector3 scale; Matrix World() const; }`
- `Entity { std::string name; Transform transform; MeshHandle mesh; MaterialHandle material; bool visible; }`
- `Scene { std::vector<Entity> entities; Camera camera; 조명 목록; }`
- 회전 순서(스케일 → 회전 → 이동)와 행벡터 규약(v × M) 유지
- 조명도 GUI 변수에서 Scene으로 이동

**점검**
- 망가뜨리기: `World()`의 곱셈 순서를 이동 → 회전 → 스케일로 바꾸면?
- 질문 1: 오일러 각과 쿼터니언의 장단점은? (짐벌 락)
- 질문 2: 비균일 스케일을 쓰는 물체에서 노멀 행렬이 왜 중요한가? (4-E와 연결)

---

## 3-3. AssetManager

**목표:** 메시·텍스처의 소유를 앱에서 엔진으로 옮기고, 핸들로 참조.

**핵심**
- `MeshHandle`, `TextureHandle` = 정수 인덱스 (+ 선택: 세대 번호로 오래된 핸들 감지)
- `LoadTexture(path, srgb)`: 같은 경로는 캐시에서 재사용
- `CreateMesh(MeshData)`, `LoadModel(path)`: 모델 파일 하나 → 메시 여러 개
- 업로드는 `Renderer::UploadResources`로 묶고, SRV 생성은 업로드 뒤
- 해제는 `DeferredRelease` 경유 (리소스 + 디스크립터 칸)

**점검**
- 망가뜨리기: 캐시를 끄고 같은 텍스처를 10번 로드하면 SRV 힙과 메모리는?
- 질문 1: 포인터 대신 핸들을 쓰는 이유 두 가지는?
- 질문 2: 에셋을 지울 때 왜 바로 Release하면 안 되는가?

---

## 3-4. 물체별 머티리얼

**목표:** `RenderScene::material` 하나 → 물체마다 다른 머티리얼.

**핵심**
- `Material { hlsl::MaterialConstants params; TextureHandle albedo; }`
- `DrawItem`에 `MaterialConstants`(또는 포인터)와 앨버도 추가
- `ForwardPass`가 Draw마다 `RP_Material` 설정 (`Push` per draw)
- 업로드 사용량 증가 확인 (Draw당 Object 256B + Material 256B)

**점검**
- 망가뜨리기: `RP_Material` 설정을 루프 밖에 그대로 두면?
- 질문 1: 같은 머티리얼을 쓰는 물체가 많을 때 업로드를 줄이는 방법은? (정렬, 캐시, StructuredBuffer)
- 질문 2: 머티리얼 상수를 루트 CBV로 두는 것과 테이블로 두는 것의 차이는?

---

## 3-5. 추출 분리

**목표:** `SandboxApp::BuildRenderScene`을 엔진 쪽 함수로.

**핵심**
- `void ExtractRenderScene(const Scene&, const AssetManager&, RenderScene& out)`
- 핸들 → 포인터 변환은 여기서만
- `visible == false` 건너뛰기 (나중에 프러스텀 컬링 자리)
- `SandboxApp`은 씬 구성 + 입력 + `Render` 호출만

**점검**
- 망가뜨리기: 추출에서 `draws.clear()`를 빼면? (5-C와 비교)
- 질문 1: 추출 단계를 따로 두는 이유는? (언리얼 SceneProxy와 비교)
- 질문 2: 프러스텀 컬링은 이 구조에서 어디에 들어가야 하는가?

---

## 3-6. 에디터 UI

**목표:** 씬을 GUI로 보고 고칠 수 있게.

**핵심**
- 아웃라이너: 엔티티 목록, 선택, 추가/삭제
- 인스펙터: 선택한 엔티티의 Transform, 머티리얼, 메시
- 통계: FPS, CPU wait, Upload, Draw 수
- 카메라: 속도, FOV, 위치
- ImGui ID 충돌 주의 (`PushID(index)` / `##` 접미사)
- `SandboxApp::UpdateGUI`의 잡다한 슬라이더 정리

**점검**
- 망가뜨리기: 아웃라이너 루프에서 `PushID`를 빼면? (1-6, 와이어프레임 때의 ID 충돌)
- 질문 1: 엔티티를 삭제할 때 선택 인덱스·핸들은 어떻게 처리하나?
- 질문 2: 에디터 코드와 엔진 코드를 분리하는 이유는?

---

## 3-7. (선택) 피킹 또는 기즈모

- 피킹: 화면 좌표 → 카메라 레이 → 엔티티 AABB 교차
- 기즈모: ImGuizmo로 이동·회전·스케일 핸들

---

## 점검과 복습 일정

| 시점 | 내용 |
| --- | --- |
| 3-1 ~ 3-7 각 단계 끝 | **30분 점검**: 망가뜨리기 1개, 5줄 메모, 질문 2개 |
| 3단계 끝 | 별도 복습 없음 (원하면 반나절 미니 복습) |
| 4단계 각 단계 끝 | 30분 점검 |
| **4단계 끝** | **1~2일 체크포인트**: 3·4단계 통합 복습 + 1·2단계 Day 5 종합 시험 |
