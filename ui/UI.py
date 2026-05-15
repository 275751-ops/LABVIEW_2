import sys
import requests
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout,
    QPushButton, QLineEdit, QLabel,QCheckBox, QHBoxLayout
)

from PyQt5.QtCore import QTimer

from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure


class SensorApp(QWidget):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("ESP Sensor Monitor")
        self.resize(1000, 800)

        layout = QVBoxLayout()
        button_layout = QHBoxLayout()

        # Pole adresu API
        self.api_input = QLineEdit()
        self.api_input.setText("http://127.0.0.1:5001")
        layout.addWidget(QLabel("Adres API:"))
        layout.addWidget(self.api_input)

        # Przyciski i chkboxy
        self.connect_button = QPushButton("Połącz")
        self.connect_button.clicked.connect(self.update_charts)
        button_layout.addWidget(self.connect_button)

        self.pomiar = QPushButton("Pomiar")
        self.pomiar.clicked.connect(self.obsluga_klikniecia)
        button_layout.addWidget(self.pomiar)

        self.ciagly_pomiar = QCheckBox("Ciągły pomiar")
        self.ciagly_pomiar.setChecked(False)
        self.ciagly_pomiar.stateChanged.connect(self.toggle_continuous_measurement)
        button_layout.addWidget(self.ciagly_pomiar)

        button_layout.addStretch()

        layout.addLayout(button_layout)
        
        # Wykresy
        self.figure = Figure(figsize=(8, 8))
        self.canvas = FigureCanvas(self.figure)

        self.ax_temp = self.figure.add_subplot(311)
        self.ax_pressure = self.figure.add_subplot(312)
        self.ax_altitude = self.figure.add_subplot(313)

        layout.addWidget(self.canvas)

        self.setLayout(layout)

        # Timer odświeżania
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_charts)

    def toggle_continuous_measurement(self):

        if self.ciagly_pomiar.isChecked():

            print("START")
            self.timer.start(2000)

        else:

            print("STOP")
            self.timer.stop()

    # def start_fetching(self):
    #     self.timer.start(2000)  # co 2 sekundy
    #     self.update_charts()
    def obsluga_klikniecia(self):
        self.update_charts()

    def get_sensor_data(self, sensor_name):
        base_url = self.api_input.text()

        url = (
            f"{base_url}/measurements/history"
            f"?sensor={sensor_name}&limit=50"
        )

        try:
            response = requests.get(url, timeout=3)

            if response.status_code == 200:
                data = response.json()

                data.reverse()

                values = [x["value"] for x in data]
                times = list(range(len(values)))

                return times, values

        except Exception as e:
            print(e)

        return [], []

    def update_charts(self):

        # Temperatura
        t_x, t_y = self.get_sensor_data("temperature")

        # Ciśnienie
        p_x, p_y = self.get_sensor_data("pressure")

        # Wysokość
        a_x, a_y = self.get_sensor_data("Altitude")

        # Czyszczenie
        self.ax_temp.clear()
        self.ax_pressure.clear()
        self.ax_altitude.clear()

        # Rysowanie
        self.ax_temp.plot(t_x, t_y)
        self.ax_temp.set_title("Temperatura")

        self.ax_pressure.plot(p_x, p_y)
        self.ax_pressure.set_title("Ciśnienie")

        self.ax_altitude.plot(a_x, a_y)
        self.ax_altitude.set_title("Wysokość")

        self.figure.tight_layout()

        self.canvas.draw()


app = QApplication(sys.argv)

window = SensorApp()
window.show()

sys.exit(app.exec_())