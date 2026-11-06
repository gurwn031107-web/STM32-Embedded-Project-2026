"""
fake_mcu.py - virtual STM32 elevator board for testing the GUI without hardware.

Behaves like ap_main.c (HOMING / IDLE / MOVING / DOOR / ERROR) and speaks the
same UART protocol as the real board:
  PC -> board : CALL n, STOP, HOME            (one command per line)
  board -> PC : normal log lines + "$ST,..." status line

It exposes the small part of the pyserial API the GUI uses:
  write(bytes), read(n), in_waiting, is_open, close()
"""

import threading
import time

FLOOR_POS = {1: 0, 2: 1800, 3: 3600}   # sensor centre position of each floor (steps)
SENSOR_HALF = 120                       # sensor is blocked within +-SENSOR_HALF steps
STEPS_PER_SEC = 500                     # STEP_INTERVAL_MS = 2
STOP_CONFIRM_STEPS = 60
MAX_MOVE_STEPS = 15000
START_POS = 2500                        # start between 2F and 3F -> HOMING on power up

DING_MS, DONG_MS = 300, 600
DOOR_OPEN_DELAY_MS, DOOR_OPEN_MS = 2000, 3000


class FakeMcu:
    def __init__(self):
        self.is_open = True
        self._rx = bytearray()          # board -> PC
        self._cmd = bytearray()         # PC -> board (unparsed)
        self._lock = threading.Lock()

        self.real_pos = START_POS       # physical car position
        self.position = 0               # firmware counter (unknown until HOME OK)
        self.state = "HOMING"
        self.cur = 0
        self.target = 0
        self.dir = "S"
        self.err = "NONE"
        self.move_steps = 0
        self.start_floor = 0
        self.blk = [0, 0, 0]
        self.door_phase = "NONE"
        self.door_t0 = 0.0
        self.door_t1 = 0.0
        self.pending = 0
        self._prev_sens = None
        self._last_st = ""
        self._last_st_t = 0.0
        self._last_pos_print = 0.0

        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    # ---------- pyserial-like API ----------
    @property
    def in_waiting(self):
        with self._lock:
            return len(self._rx)

    def read(self, n=1):
        with self._lock:
            data = bytes(self._rx[:n])
            del self._rx[:n]
            return data

    def write(self, data):
        with self._lock:
            self._cmd += data
        return len(data)

    def close(self):
        self.is_open = False

    # ---------- board side ----------
    def _print(self, text):
        with self._lock:
            self._rx += (text + "\r\n").encode()

    def _sensor(self, i):
        return abs(self.real_pos - FLOOR_POS[i + 1]) <= SENSOR_HALF

    def _blocked_floor(self):
        for i in range(3):
            if self._sensor(i):
                return i + 1
        return 0

    def _start_move(self, target, direction, state):
        self.start_floor = self.cur
        self.target = target
        self.move_steps = 0
        self.blk = [0, 0, 0]
        self.state = state
        self.dir = direction

    def _start_homing(self):
        self._print("HOMING: go down to 1F")
        self.err = "NONE"
        self._start_move(1, "D", "HOMING")

    def _start_arrival(self, now):
        self.state = "DOOR"
        self.door_phase = "DING"
        self.door_t0 = now
        self.door_t1 = now
        self.pending = 0
        self._print(f"DING-DONG at {self.cur}F")

    def _enter_error(self, err):
        self.dir = "S"
        self.state = "ERROR"
        self.err = err
        if err == "STOP":
            self._print(f"EMERGENCY STOP  pos={self.position} -> press any button to re-home")
        elif err == "TIMEOUT":
            self._print(f"ERROR(TIMEOUT): {self.target}F sensor not found -> STOP")

    def _handle_call(self, req, now, from_pc):
        self._print(f"CALL {req}F" + (" (PC)" if from_pc else ""))
        if self.state in ("HOMING", "MOVING"):
            self._print("BUSY -> ignored (use STOP)")
        elif self.state == "ERROR":
            self._start_homing()
        elif self.state == "DOOR":
            if req != self.cur:
                self.pending = req
                self._print(f"queued {req}F (door busy)")
        elif self.state == "IDLE":
            if req == self.cur and self._sensor(req - 1):
                self._print(f"already at {req}F -> open door")
                self._start_arrival(now)
            else:
                self._start_move(req, "U" if req > self.cur else "D", "MOVING")
                self._print(f"MOVE {self.cur}F -> {req}F")

    def _handle_line(self, line, now):
        parts = line.strip().upper().split()
        if not parts:
            return
        if parts[0] == "CALL" and len(parts) == 2 and parts[1] in ("1", "2", "3"):
            self._handle_call(int(parts[1]), now, True)
        elif parts[0] == "STOP":
            if self.state in ("HOMING", "MOVING"):
                self._enter_error("STOP")
            else:
                self._print("STOP: not moving")
        elif parts[0] == "HOME":
            if self.state in ("IDLE", "ERROR"):
                self._start_homing()
            else:
                self._print("BUSY -> ignored")
        else:
            self._print(f"? unknown command: {line.strip()}")

    def _step(self):
        self.real_pos += 1 if self.dir == "U" else -1
        self.position += 1 if self.dir == "U" else -1
        self.move_steps += 1
        for i in range(3):
            self.blk[i] = self.blk[i] + 1 if self._sensor(i) else 0
            if self.blk[i] == STOP_CONFIRM_STEPS:
                f = i + 1
                self.cur = f
                if self.state == "HOMING" and f != 1:
                    self._print(f"  pass {f}F")
                if f == self.target:
                    self.dir = "S"
                    return True
        return False

    def _status_line(self):
        sens = "".join("1" if self._sensor(i) else "0" for i in range(3))
        door = {"NONE": 0, "DING": 1, "DONG": 1, "WAIT": 1, "OPEN": 2}[self.door_phase] \
            if self.state == "DOOR" else 0
        return (f"$ST,{self.state},{self.cur},{self.target},{self.dir},"
                f"{self.position},{door},{sens},{self.err}")

    def _run(self):
        now = time.monotonic()
        self._print("")
        self._print("[Elevator] UART control + status ($ST)")
        self._print("START between floors")
        self._start_homing()
        last = now
        acc = 0.0
        while self.is_open:
            time.sleep(0.01)
            now = time.monotonic()
            dt, last = now - last, now

            # commands from PC
            with self._lock:
                buf = bytes(self._cmd)
                self._cmd.clear()
            self._pending_text = getattr(self, "_pending_text", "") + buf.decode(errors="replace")
            while "\n" in self._pending_text:
                line, self._pending_text = self._pending_text.split("\n", 1)
                self._handle_line(line.replace("\r", ""), now)

            # motor
            if self.dir != "S":
                acc += dt * STEPS_PER_SEC
                while acc >= 1 and self.dir != "S":
                    acc -= 1
                    if self._step():
                        if self.state == "HOMING":
                            self.position = 0
                            self.cur = 1
                            self.state = "IDLE"
                            self._print("HOME OK (1F)  pos=0")
                        else:
                            self._print(f"ARRIVED {self.cur}F  pos={self.position}  steps={self.move_steps}")
                            self._start_arrival(now)
                if self.dir != "S" and self.move_steps > MAX_MOVE_STEPS:
                    self._enter_error("TIMEOUT")
                if self.dir != "S" and now - self._last_pos_print >= 0.2:
                    self._last_pos_print = now
                    self._print(f"{'UP  ' if self.dir == 'U' else 'DOWN'}  pos={self.position}")
            else:
                acc = 0.0

            # door sequence
            if self.state == "DOOR":
                ms0 = (now - self.door_t0) * 1000
                ms1 = (now - self.door_t1) * 1000
                if self.door_phase == "DING" and ms1 >= DING_MS:
                    self.door_phase, self.door_t1 = "DONG", now
                elif self.door_phase == "DONG" and ms1 >= DONG_MS:
                    self.door_phase = "WAIT"
                elif self.door_phase == "WAIT" and ms0 >= DOOR_OPEN_DELAY_MS:
                    self.door_phase, self.door_t1 = "OPEN", now
                    self._print(f"DOOR OPEN  ({self.cur}F)")
                elif self.door_phase == "OPEN" and ms1 >= DOOR_OPEN_MS:
                    self.door_phase = "NONE"
                    self.state = "IDLE"
                    self._print(f"DOOR CLOSE ({self.cur}F)")
                    if self.pending and self.pending != self.cur:
                        req, self.pending = self.pending, 0
                        self._start_move(req, "U" if req > self.cur else "D", "MOVING")
                        self._print(f"MOVE {self.cur}F -> {req}F (queued)")
                    self.pending = 0

            # sensor change log
            sens = tuple(self._sensor(i) for i in range(3))
            if sens != self._prev_sens:
                if self._prev_sens is not None:
                    for i in range(3):
                        if sens[i] != self._prev_sens[i]:
                            self._print(f"  [{i + 1}F SENSOR {'BLOCKED' if sens[i] else 'open'}]  pos={self.position}")
                self._prev_sens = sens

            # status line: every 100 ms when changed, at least every 500 ms
            st = self._status_line()
            if (st != self._last_st and now - self._last_st_t >= 0.1) or now - self._last_st_t >= 0.5:
                self._print(st)
                self._last_st, self._last_st_t = st, now
