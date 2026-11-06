# 🕰️ STM32 통합 시계 — FND · I2C LCD · DS3231 RTC · 스텝모터 바늘

> STM32F411RE로 만든 **시계 + 스톱워치**. RTC 시간을 FND·LCD에 표시하고, 스텝모터 3개로 **실제 시·분·초 바늘**을 움직입니다.

| 항목 | 내용 |
|---|---|
| 기간 | 2026.10 |
| 형태 | 마이크로컨트롤러 프로그래밍 수업 실습 (개인) |
| 보드 | NUCLEO-F411RE (STM32F411RET6) |
| 개발 환경 | STM32CubeIDE, STM32CubeMX, C (HAL) |
| 주변장치 | 4-digit FND, I2C LCD 16×2 (0x27), DS3231 RTC (0x68), 28BYJ-48 + ULN2003 ×3, 버튼 ×3, LED ×4 |

<br>

## 📌 동작

| 모드 | 스텝모터 바늘 | FND | LCD 1행 | LCD 2행 |
|---|---|---|---|---|
| **TimeClock** | RTC 시·분·초 | 분·초 | 현재 시각 | 날짜·요일 |
| **StopWatch** | 스톱워치 시·분·초 | 분 일의 자리·초·0.1초 | 현재 시각 | 스톱워치 시·분·초·0.01초 |

- 시침은 12시간에 한 바퀴, 분침·시침은 초/분까지 반영해 중간 위치를 가리킴
- 모드가 바뀌면 바늘이 **짧은 방향**으로 새 목표까지 이동, 모터 3개가 같은 타이머 틱에서 함께 이동
- 스톱워치는 RUN 상태면 시계 모드에서도 계속 증가 (모드 전환 = 표시 대상 선택)
- STOP 후 CLEAR → 숫자는 즉시 0, 바늘은 12시로 복귀

<br>

## 🧱 설계 포인트

**타이머 인터럽트 분리**
- `TIM3` 1ms: FND 스캔 + 스텝모터 (2ms마다 모터당 최대 1스텝)
- `TIM4` 10ms: 스톱워치 0.01초 카운트
- I2C 통신(LCD·RTC), 버튼 디바운스, 문자열 포맷은 메인 루프에서만 → 모터 펄스가 LCD 전송 시간에 묶이지 않음

**바늘 목표 위치 계산** (`N = 4096` 스텝/회전)
```
초침 = 초 × N / 60
분침 = (분 × 60 + 초) × N / 3600
시침 = ((시 % 12) × 3600 + 분 × 60 + 초) × N / 43200

distance = (target + N - position) % N
  → 0이면 정지(코일 유지), N/2 이하면 정방향, 초과면 역방향 1스텝
```

**계층 구조 (ap / driver)**
- `ap/` : TimeClock, StopWatch, Display, ClockNeedle (애플리케이션)
- `driver/` : button, led, fnd, lcd, DS3231, StepMotor (하드웨어 제어)
- main ↔ ISR 공유 변수는 `volatile` + 짧은 임계구역으로 스냅샷 복사

<br>

## 🔌 핀 배치

| 바늘 | IN1 | IN2 | IN3 | IN4 |
|---|---|---|---|---|
| 시 | PC10 | PC11 | PC12 | PD2 |
| 분 | PC9 | PC8 | PC6 | PC5 |
| 초 | PA0 | PA1 | PA4 | PB0 |

| 기능 | 핀 |
|---|---|
| I2C1 SCL / SDA | PB8 / PB9 |
| 모드 전환 / 실행·정지 / 초기화 버튼 | PC0 / PC1 / PC3 |
| 시계 모드 LED / 스톱워치 모드 LED | PB15 / PB14 |
| 스톱워치 STOP LED / RUN LED | PB13 / PC4 |

> 원점 센서가 없으므로 전원 투입 전 세 바늘을 12시 방향에 맞춰야 합니다.

<br>

## 📁 폴더 구조

```
Core/Src/
├── main.c                # CubeMX 생성 (GPIO, TIM3, TIM4, I2C1 초기화)
├── ap/
│   ├── ap_main.c         # 모드 선택, 초기화 순서, 타이머 콜백
│   ├── ClockNeedle.c     # 시간 → 바늘 목표 스텝 변환, 모터 이동 ISR
│   ├── Display/          # FND, LCD, LED 표시
│   ├── StopWatch/
│   └── TimeClock/        # DS3231 읽기
└── driver/
    ├── button/  ├── led/  ├── fnd/
    ├── lcd/     ├── DS3231/  └── StepMotor/
docs/통합_코드_가이드.md   # 상세 설계 문서
```

<br>

## ▶️ 빌드

1. STM32CubeIDE에서 `File → Import → Existing Projects into Workspace`로 폴더 열기
2. `.ioc` 파일을 열고 **Generate Code** (저장소에는 용량 때문에 `Drivers/`(HAL·CMSIS)를 넣지 않았습니다)
3. Build → Debug/Run

자세한 설명은 [`docs/통합_코드_가이드.md`](docs/통합_코드_가이드.md)에 있습니다.
