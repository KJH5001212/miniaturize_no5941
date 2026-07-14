# GLU 설치 (연결된 태블릿/폰에 바로)

앱 이름은 **GLU**. 아래는 **너의 컴퓨터**에서 실행한다 (이 저장소를 클론/풀 받은 뒤).
안드로이드 SDK 가 있는 환경이어야 컴파일된다 (안드로이드 스튜디오 설치 시 자동 포함).

## 가장 쉬운 방법 — 안드로이드 스튜디오

1. 안드로이드 스튜디오에서 **`android-app/` 폴더 열기** (File → Open)
2. 태블릿을 USB 로 연결하고 개발자 옵션 → **USB 디버깅 켜기** (태블릿에 뜨는 허용 팝업 수락)
3. 상단 기기 드롭다운에서 태블릿 선택 → **Run ▶**
   → 빌드 후 태블릿에 **GLU** 가 설치되고 실행됨

## CLI 한 줄 — adb 로 바로 설치

태블릿을 USB 로 연결(USB 디버깅 ON)한 상태에서:

```bash
cd android-app
./gradlew installDebug        # 빌드 + 연결된 기기에 GLU 설치
```

기기가 여러 대면 먼저 확인:

```bash
adb devices                    # 태블릿이 목록에 보이는지
```

설치만 따로 하려면 (이미 APK 를 뽑았을 때):

```bash
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## 처음 실행 시

- 블루투스 **스캔/연결 권한** 허용 팝업 → 수락
- 태블릿 블루투스 ON → 앱이 자동으로 **pstat** 기기를 검색 → 목록에서 탭하면 연결
- 연결되면 현재 전류·배터리·그래프가 실시간으로 갱신

## 안 될 때

- `SDK location not found` → 안드로이드 스튜디오로 한 번 열면 `local.properties` 가 자동 생성됨
  (또는 `ANDROID_HOME` 환경변수 설정)
- `adb: no devices` → USB 케이블/디버깅 승인 확인, `adb kill-server && adb start-server`
- 스캔에 기기가 안 뜸 → 태블릿 위치정보(안드로이드 11 이하)·블루투스 ON, pstat 전원 확인
