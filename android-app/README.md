# Ampero — 안드로이드 앱 (WebView 하이브리드)

웨어러블 포텐쇼스탯(pstat) 전용 안드로이드 앱. 검증된 목업 UI(`../app/mockup.html`)를
그대로 앱 자산으로 싣고, **네이티브 BLE 계층**을 JS 브리지로 연결한 하이브리드 구조.

## 구조

```
app/src/main/
  assets/app.html                     ← UI 전체 (목업 v1.1 + 라이브 BLE 브리지)
  java/com/ampero/pstat/
    MainActivity.kt                    ← WebView 호스트 + 런타임 BT 권한
    NativeBridge.kt                    ← JS ↔ 네이티브 다리 (window.AmperoNative)
    ble/BleManager.kt                  ← BLE 연결/스캔/쓰기 (AD5941 앱에서 이식, 검증됨)
```

- **네이티브 → JS**: 모든 NUS notify 프레임을 UTF-8 문자열로 `window.AmperoIn("data",…)` 에 전달.
  연결/스캔/로그 이벤트도 같은 통로.
- **JS → 네이티브**: `AmperoNative.startScan/connect/send/…`. JS 가 pstat JSON 명령을 만들고
  네이티브는 그대로 NUS RX 로 씀 (기존 앱과 동일한 신뢰성 있는 쓰기 큐).
- **BLE 계층은 기기 UUID 를 몰라도** Nordic UART(NUS)를 자동 탐지하므로 pstat 에 그대로 붙음.

## 라이브 vs 데모

- **브라우저**에서 `app.html` 을 열면 `AmperoNative` 가 없어 **데모 데이터**로 동작 (미리보기).
- **앱**에서 실행하면 실행 즉시 연결 오버레이 → 기기 스캔·연결 → 라이브 전환:
  - 현재 전류(값·구간색·추세), 기기 배터리(vbat), 충전 상태(qi/chg), 동기화(fpend),
    **홈 그래프**가 수신 샘플로 실시간 갱신.
  - 홈 그래프의 x축은 **사이클+요약(agg) 모드 = 5분 간격** 가정으로 그림 (주 사용 모드).

### 다음 이터레이션 (온디바이스 검증 필요)

- 기록(일자별)·주간 패턴·오전/오후 시계의 **라이브 데이터 바인딩** — 현재는 데모 데이터 기준.
  실측 히스토리(로컬 캐시/서버 `GET /days`)와 연결 시 활성화.
- 서버 업로드(`POST /sync`)·계정 로그인(OAuth)·푸시 알림·홈 위젯.
- 연속(고속) 모드의 실시간 x축(현재는 5분 격자 가정).

## 빌드 (안드로이드 스튜디오 / SDK 필요)

이 저장소를 만든 환경엔 안드로이드 SDK 가 없어(프록시가 dl.google.com 차단) **여기서는 APK 를
컴파일하지 못한다.** 아래처럼 SDK 가 있는 환경에서 빌드:

```bash
# 안드로이드 스튜디오로 android-app/ 폴더 열기 → Run
# 또는 CLI (ANDROID_HOME 설정 + platform android-35, build-tools 설치 후):
cd android-app
./gradlew assembleDebug
# 결과: app/build/outputs/apk/debug/app-debug.apk
```

- AGP 8.7.3 · Kotlin 2.0.21 · minSdk 26 · targetSdk 35 (AD5941 앱과 동일 스택).
- 첫 실행 시 블루투스 스캔/연결 권한 허용 → pstat 기기 선택.

## UI 갱신

`../app/mockup.html` 을 수정한 뒤 아래로 자산을 재생성:

```bash
# app.html = mockup.html + BLE 브리지 (generate_asset.py 참조 — 저장소 히스토리)
```
목업과 앱 자산이 갈라지지 않도록, UI 변경은 목업에서 하고 브리지만 앱 자산에 유지한다.
