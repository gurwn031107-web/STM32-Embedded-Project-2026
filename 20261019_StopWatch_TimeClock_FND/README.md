# STM32 StopWatch & TimeClock (FND)

> STM32F411RE에서 **타이머 인터럽트 + 상태 머신**으로 구현한 시계/스톱워치. 4-digit FND에 표시하고 버튼으로 모드를 전환합니다.

| 항목 | 내용 |
|---|---|
| 기간 | 2026.10 |
| 형태 | 마이크로컨트롤러 프로그래밍 수업 실습 |
| 보드 | NUCLEO-F411RE (STM32F411RET6) |
| 개발 환경 | STM32CubeIDE, STM32CubeMX, C (HAL) |
| 주변장치 | 4-digit FND, 버튼 ×3, LED ×4 |

> 이후 I2C LCD, DS3231 RTC, 스텝모터 바늘을 더한 확장 버전 → [stm32-integrated-clock-lcd-rtc-stepmotor](https://github.com/gurwn031107-web/stm32-integrated-clock-lcd-rtc-stepmotor)

## 동작

| 모드 | 표시 | 버튼 |
|---|---|---|
| **TimeClock** | 분·초 (DP 0.5초 점멸) | PC0 떼면 → StopWatch 모드 |
| **StopWatch** | 0.1초 단위 카운트 (최대 999.9초) | PC1: RUN/STOP, PC3: CLEAR (STOP 상태에서) |

- 모드 LED: PB15(시계) / PB14(스톱워치), 상태 LED: PB13(STOP) / PC4(RUN)

## 설계 포인트

- **타이머 분리**: `TIM3` 1ms → FND 다이나믹 스캔, `TIM4` 10ms → 시계 카운트 (0.01초 → 초 → 분 → 시 자리올림)
- **상태 머신**: 상위 모드(TimeClock / StopWatch) + 스톱워치 내부 상태(STOP / RUN / CLEAR)
- **계층 구조**: `ap/`(애플리케이션) ↔ `driver/`(button, led, fnd) 분리, 버튼은 PUSH / RELEASE 이벤트로 처리

## 폴더 구조

```
Core/Src/
├── main.c              # CubeMX 생성 (GPIO, TIM3, TIM4)
├── ap/
│   ├── ap_main.c       # 모드 상태 머신, 타이머 콜백
│   ├── StopWatch/
│   └── TimeClock/
└── driver/
    ├── button/
    ├── fnd/
    └── led/
```

## ▶ 빌드

1. STM32CubeIDE에서 `File → Import → Existing Projects into Workspace`
2. `.ioc` 파일을 열고 **Generate Code** (`Drivers/`는 저장소에서 제외)
3. Build → Debug/Run

> 수업에서 제공된 기본 구조(button/led/fnd 드라이버)를 바탕으로 작성했습니다.
