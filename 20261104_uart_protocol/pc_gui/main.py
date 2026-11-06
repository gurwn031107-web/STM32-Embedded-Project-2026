"""
Nucleo-F411 UART 제어 프로그램 시작점.

    python main.py          실제 보드 (포트 자동 감지)
    python main.py --sim    보드 없이 가상 보드로 실행
"""
import argparse
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description="Nucleo-F411 UART 제어 프로그램")
    parser.add_argument("--sim", action="store_true", help="실제 보드 없이 가상 보드로 실행")
    args, qt_args = parser.parse_known_args()

    from PySide6.QtWidgets import QApplication
    from main_window import MainWindow

    app = QApplication([sys.argv[0], *qt_args])
    app.setStyle("Fusion")
    app.setPalette(app.style().standardPalette())   # 윈도우 다크 모드에서도 밝은 테마로 고정
    window = MainWindow(sim=args.sim)
    window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
