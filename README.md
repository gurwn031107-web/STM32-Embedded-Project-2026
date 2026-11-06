# STM32 Embedded Project 2026

마이크로컨트롤러 프로그래밍 수업에서 NUCLEO-F411RE(STM32F411RE)로 실습한 코드를 날짜순으로 정리한 저장소입니다.
LED 깜빡이기와 레지스터 직접 제어부터 타이머 인터럽트, I2C(LCD, RTC), 스텝모터, 센서(DHT11, HC-SR04, ADC), PWM 부저, UART 통신과 PC 프로그램까지 순서대로 진행했습니다.

> 대표 프로젝트: [STM32 통합 시계 (RTC, LCD, 스텝모터 바늘)](https://github.com/gurwn031107-web/stm32-integrated-clock-lcd-rtc-stepmotor)

## 구성

### 1. GPIO 기초

| 폴더 | 내용 |
|---|---|
| [`20261012_LedBlink`](20261012_LedBlink) | HAL로 LED 4개 한꺼번에 켜기, 끄기, 토글 |
| [`20261013_LedBlink`](20261013_LedBlink) | 버튼 3개(PC0 / PC1 / PC3)로 LED 켜기, 끄기, 토글 |
| [`20261013_C_Lang`](20261013_C_Lang) | `__io_putchar` 재정의로 UART2에 `printf` 출력, C 문법 실습 |
| [`20261015_baremetal_gpio`](20261015_baremetal_gpio) | HAL 없이 레지스터 직접 제어 (RCC, MODER, OTYPER, OSPEEDR, PUPDR), 버튼으로 LED 켜기 |

### 2. FND, 타이머 인터럽트

| 폴더 | 내용 |
|---|---|
| [`20261013_FND`](20261013_FND) | 7-세그먼트 핀(a~g)을 직접 구동해 숫자 표시 |
| [`20261014_Fnd_Upcount`](20261014_Fnd_Upcount) | FND 드라이버 분리, `HAL_GetTick()`으로 0~9999 업카운터 |
| [`20261014_Fnd_count`](20261014_Fnd_count) | 버튼으로 RUN / STOP / CLEAR 상태 전환하는 카운터 |
| [`20261016_UpCounter_StopWatch`](20261016_UpCounter_StopWatch) | **ap / driver 계층 구조 첫 적용**, 버튼, LED, FND 드라이버, 스톱워치 / 시계 모드 |
| [`20261019_StopWatch_TimeClock_FND`](20261019_StopWatch_TimeClock_FND) | 1ms(FND 스캔) / 10ms(시간 카운트) 타이머 인터럽트 분리, 모드 상태 머신 |

### 3. I2C, 스텝모터

| 폴더 | 내용 |
|---|---|
| [`20261020_StopWatch_TimeClock_I2C_LCD`](20261020_StopWatch_TimeClock_I2C_LCD) | I2C 통신(`HAL_I2C_Master_Transmit`, 주소 0x27)으로 LCD 제어 시작, Display 모듈 추가 |
| [`20261020_StopWatch_TimeClock_I2C_LCD2`](20261020_StopWatch_TimeClock_I2C_LCD2) | DS3231 RTC 드라이버 추가, 실제 시간, 날짜 읽기 |
| [`20261023_StepMotor`](20261023_StepMotor) | 28BYJ-48 + ULN2003 스텝모터 드라이버 추가 |
| [`20261023_StopWatch_TimeClock_I2C_LCD_DS3231_StepMotor`](20261023_StopWatch_TimeClock_I2C_LCD_DS3231_StepMotor) | 스텝모터 3개로 시, 분, 초 바늘 구동 (대표 프로젝트) |

### 4. 센서, PWM, ADC

| 폴더 | 내용 |
|---|---|
| [`20261027_DHT11`](20261027_DHT11) | µs 딜레이 구현, DHT11 1-Wire 온습도 읽기, `ap/service` 계층 추가 |
| [`20261028_DHT11_Temp_Humid`](20261028_DHT11_Temp_Humid) | DHT11 온습도 실습을 새 프로젝트로 다시 구성 |
| [`20261028_DHT11_HCSR04`](20261028_DHT11_HCSR04) | HC-SR04 초음파 거리 측정 (타이머 카운터로 Echo 펄스 폭 측정) + 온습도 LCD 표시 |
| [`20261029_DHT11_HCSR04_2`](20261029_DHT11_HCSR04_2) | TIM1 PWM 부저 드라이버, 버튼으로 주파수, duty 조절 |
| [`20261030_Buzzer`](20261030_Buzzer) | 음계(C4~B7) 사운드 서비스, 링 버퍼 큐(`que_buffer`)에 버튼 이벤트를 넣고 메인 루프에서 처리 |
| [`20261102_smartwatch`](20261102_smartwatch) | 시계, 온습도, 거리, 스톱워치 4가지 모드 스마트워치, 입력 이벤트 큐 |
| [`20261103_ADC`](20261103_ADC) | ADC 2채널 + DMA, 조도 센서(CDS) 값을 %와 mV로 변환 |
| [`20261103_joystick`](20261103_joystick) | 조이스틱 X/Y축 ADC, 데드존, 기울기별 속도로 카운터 증감 후 LCD 표시 |

### 5. UART 통신, 응용

| 폴더 | 내용 |
|---|---|
| [`20261104_uart_protocol`](20261104_uart_protocol) | 바이너리 패킷 프로토콜(LED 4개 제어, 온습도, 거리 요청/응답) + [`pc_gui`](20261104_uart_protocol/pc_gui) (PySide6 PC 제어 프로그램, 포트 자동 감지, 실시간 그래프) |
| [`20261105_Elevator`](20261105_Elevator) | 3층 엘리베이터: 원점 잡기, 층 센서 정지, 에러 감지 상태 머신, 도착 딩동 + 문 열림, UART 명령/상태 + [`pc_gui`](20261105_Elevator/pc_gui) |

## 공통 구조

10/16 이후 프로젝트는 **애플리케이션(ap)과 드라이버(driver)를 나누는 계층 구조**로 작성했습니다.

```
Core/Src/
├── main.c          # CubeMX 생성 코드 (주변장치 초기화) → ap_main() 호출
├── ap/             # 애플리케이션: 상태 머신, 모드, 화면 표시
│   └── service/    # 기능 단위 서비스 (TempHumid, Distance, sound ...)
├── driver/         # 하드웨어 제어: button, led, fnd, lcd, DS3231, StepMotor, dht11, HCSR04, buzzer ...
└── common/         # delay(µs), queue 등 공통 모듈
```

- 시간에 민감한 일(FND 스캔, 모터 스텝, 초음파 Echo)은 **타이머, 외부 인터럽트**, 느린 통신(I2C, UART 출력)은 **메인 루프**에서 처리
- 메인 루프와 인터럽트가 같이 쓰는 변수는 `volatile` + 짧은 임계구역으로 보호

## 20261105_Elevator

| 상태 | 동작 |
|---|---|
| `HOMING` | 전원 ON 시 아래로 내려가며 층 센서로 위치 인식 → 1층에서 정지 |
| `IDLE` | 층 버튼(PC0 / PC1 / PC3) 또는 PC 명령 대기 |
| `MOVING` | 목표 층 센서가 일정 스텝 동안 계속 가려져야 도착으로 인정 (잡음 무시) |
| `DOOR` | 딩동(TIM1 PWM) → 2초 뒤 문 열림(LED) → 3초 뒤 닫힘, 그동안 들어온 호출은 기억 |
| `ERROR` | 제한 스텝 초과(TIMEOUT), 너무 빠른 도착(센서 이상) 시 즉시 정지 → 버튼으로 원점부터 재시작 |

- LCD 1행: `FLOOR:2F > 3F UP` 같은 상태, 2행: 엘리베이터 안 온습도(DHT11, 정지 중에만 측정)
- PC → 보드: `CALL 1~3`, `STOP`, `HOME` / 보드 → PC: `$ST,<상태>,<현재층>,<목표층>,<방향>,<위치>,<문>,<센서>,<에러>`

## ▶ 빌드

1. STM32CubeIDE에서 `File → Import → Existing Projects into Workspace`로 원하는 폴더 열기
2. `.ioc` 파일을 열고 **Generate Code** (`Drivers/`(HAL, CMSIS)와 CubeMX가 자동 생성하는 시스템 파일은 저장소에서 제외)
3. Build → Debug/Run

PC 프로그램(`pc_gui`)은 각 폴더의 README를 참고하세요.
