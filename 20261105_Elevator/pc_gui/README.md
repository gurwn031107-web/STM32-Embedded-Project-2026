# STM32 Elevator PC GUI

## 실행 (Windows)
- **간단히**: `run_gui.bat` 더블클릭 (처음 한 번은 라이브러리 자동 설치)
- 직접 실행:
  ```
  py -m pip install pyside6 pyserial
  py elevator_gui.py
  ```
  `python` 대신 `py`를 쓰는 이유: `python`이 Microsoft Store 별칭으로 연결돼 있으면 "Python"만 출력하고 끝남
- Python이 없으면 python.org에서 3.10 이상 설치 (첫 화면 "Add python.exe to PATH" 체크)
- 포트: Nucleo(STLink Virtual COM Port) 선택 → 115200 → 연결. 보드 없이 테스트는 `DEMO (가상 보드)`
- "액세스가 거부되었습니다" → 같은 COM 포트를 연 다른 프로그램(Tera Term, CubeIDE 터미널, 다른 GUI 창)을 닫기
- 단축키: `1` `2` `3` 층 호출, `Space` 비상 정지, `H` 원점 복귀

## UART 프로토콜 (115200 8N1, 한 줄 = 명령 1개, `\n` 종료 / `\r` 무시)

### PC → 보드
| 명령 | 동작 |
|---|---|
| `CALL 1` / `CALL 2` / `CALL 3` | 층 호출 (버튼과 동일 처리) |
| `STOP` | 이동·원점 복귀 중이면 비상 정지 (ERR_STOP) |
| `HOME` | IDLE / ERROR 상태에서 원점 복귀 시작 |

### 보드 → PC
기존 printf 로그는 그대로 두고, 상태줄 1종만 추가:
```
$ST,<state>,<cur>,<target>,<dir>,<pos>,<door>,<s1s2s3>,<err>
예) $ST,MOVING,2,3,U,1731,0,010,NONE
```
| 필드 | 값 |
|---|---|
| state | HOMING / IDLE / MOVING / DOOR / ERROR |
| cur, target | 0~3 (0 = 모름) |
| dir | U / D / S (moveCmd) |
| pos | position (1F = 0) |
| door | 0 닫힘, 1 도착 알림(DING/DONG/WAIT), 2 열림 |
| s1s2s3 | 층 센서 1F·2F·3F, 1 = 가려짐 |
| err | NONE / TIMEOUT / SENSOR / STOP / NOHOME (errName과 동일) |

전송 주기 권장: 값이 바뀌면 100 ms 간격으로, 안 바뀌어도 500 ms마다 1번 (GUI는 2초간 수신이 없으면 "수신 없음" 표시).

## 펌웨어 쪽에 추가할 것 (ap_main.c)
1. **수신**: `HAL_UART_Receive_IT(&huart2, &rxByte, 1)`로 1바이트씩 받고, `HAL_UART_RxCpltCallback`에서 줄 버퍼에 쌓은 뒤 다시 `Receive_IT` 호출. `\n`이 오면 "줄 완성" 플래그만 세움.
2. **파싱은 ap_main()에서**: 콜백(인터럽트) 안에서 printf나 상태 변경을 하지 않음.
3. **버튼 처리 함수화**: 지금 버튼 루프 안의 `if (state == ...)` 분기를 `handleCall(req, now)`로 빼서 버튼과 `CALL n`이 같은 함수를 쓰게 함.
4. **상태줄 출력**: LCD 갱신 블록 옆에 `$ST` printf 추가.
