import sys
from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import QApplication, QMainWindow, QTextEdit
import zmq
import daoLogging_pb2 as daoLogging

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self.text_edit = QTextEdit(self)
        self.text_edit.setReadOnly(True)
        self.text_edit.setLineWrapMode(QTextEdit.NoWrap)
        self.text_edit.setTextInteractionFlags(Qt.TextSelectableByMouse)
        self.setCentralWidget(self.text_edit)

        self.setWindowTitle("Proxy Messages")
        self.resize(800, 600)

    def append_message(self, serialized_message):
        message = daoLogging.LogMessage()
        message.ParseFromString(serialized_message)
        self.text_edit.append(f'[{message.time_stamp}] {message.component_name} - {message.log_message}')
        print(f'[{message.time_stamp}] {message.component_name} - {message.log_message}')


if __name__ == "__main__":
    app = QApplication(sys.argv)
    main_window = MainWindow()
    main_window.show()
    context = zmq.Context()
    socket = context.socket(zmq.SUB)
    socket.connect("tcp://localhost:5558")
    socket.setsockopt_string(zmq.SUBSCRIBE, "")
    while True:
        serialized_message = socket.recv()
        main_window.append_message(serialized_message)
    sys.exit(app.exec_())
