import sys
import requests
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout,
    QPushButton, QLineEdit, QLabel, QCheckBox, QHBoxLayout
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
        
        # Pierwsza linia: Adres API i checkboxy autoryzacji
        api_layout = QHBoxLayout()
        
        self.api_input = QLineEdit()
        self.api_input.setText("http://192.168.0.57:5001")
        api_layout.addWidget(QLabel("Adres API:"))
        api_layout.addWidget(self.api_input)
        
        self.use_auth_checkbox = QCheckBox("Użyj Basic Auth")
        self.use_auth_checkbox.setChecked(True)
        api_layout.addWidget(self.use_auth_checkbox)
        
        layout.addLayout(api_layout)

        auth_layout = QHBoxLayout()
        
        self.user_input = QLineEdit()
        self.user_input.setText("studentDebil")
        auth_layout.addWidget(QLabel("Użytkownik:"))
        auth_layout.addWidget(self.user_input)
        
        self.pass_input = QLineEdit()
        self.pass_input.setText("student")
        self.pass_input.setEchoMode(QLineEdit.Password)  # Ukrywanie wpisywanego hasła (maskowanie)
        auth_layout.addWidget(QLabel("Hasło:"))
        auth_layout.addWidget(self.pass_input)
        
        layout.addLayout(auth_layout)

        # Trzecia linia: Status i przyciski akcji
        button_layout = QHBoxLayout()

        self.status_label = QLabel("Status: Rozłączony")
        layout.addWidget(self.status_label)

        self.connect_button = QPushButton("Połącz")
        self.connect_button.clicked.connect(self.update_charts)
        button_layout.addWidget(self.connect_button)

        self.pomiar = QPushButton("Pomiar")
        self.pomiar.clicked.connect(self.Pomiar)
        button_layout.addWidget(self.pomiar)

        self.clear = QPushButton("wyczyść wykresy")
        self.clear.clicked.connect(self.clear_chart)
        button_layout.addWidget(self.clear)

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
            self.timer.start(1000)
        else:
            print("STOP")
            self.timer.stop()

    def Pomiar(self):
        self.update_charts()

    def get_sensor_data(self, sensor_name):
        base_url = self.api_input.text()
        url = (
            f"{base_url}/measurements/history"
            f"?sensor={sensor_name}&limit=50"
        )

        # Konfiguracja uwierzytelniania w requests
        auth_credentials = None
        if self.use_auth_checkbox.isChecked():
            # Przekazanie krotki (user, password) powoduje, że biblioteka requests automatycznie
            # koduje dane do Base64 i wysyła nagłówek 'Authorization: Basic <credentials>'
            auth_credentials = (self.user_input.text(), self.pass_input.text())

        try:
            response = requests.get(url, auth=auth_credentials, timeout=3)
            
            # Rzucenie wyjątku HTTPError dla kodów statusu 4xx i 5xx
            response.raise_for_status()

            if response.status_code == 200:
                self.status_label.setText("Status: Połączono")
                data = response.json()
                data.reverse()

                values = [x["value"] for x in data]
                times = list(range(len(values)))
                return times, values

        except requests.exceptions.ConnectionError:
            self.status_label.setText("Błąd połączenia - zły adres API")
        except requests.exceptions.Timeout:
            self.status_label.setText("Timeout - serwer nie odpowiada")
        except requests.exceptions.HTTPError as e:
            if e.response.status_code == 401:
                self.status_label.setText("Status: 401 Unauthorized - Błędne poświadczenia lub brak logowania")
            else:
                self.status_label.setText(f"Błąd HTTP: {e}")
        except requests.exceptions.RequestException as e:
            self.status_label.setText(f"Inny błąd requests: {e}")
        except ValueError:
            self.status_label.setText("Błędny JSON z serwera")

        return [], []
    
    def clear_chart(self):
        self.ax_temp.clear()
        self.ax_pressure.clear()
        self.ax_altitude.clear()
        self.canvas.draw_idle()

    def update_charts(self):
        t_x, t_y = self.get_sensor_data("temperature")
        p_x, p_y = self.get_sensor_data("pressure")
        a_x, a_y = self.get_sensor_data("Altitude")

        if not t_y and not p_y and not a_y:
            print("Brak danych lub brak autoryzacji")
            self.clear_chart()
            self.canvas.draw_idle()
            return

        self.clear_chart()

        self.ax_temp.plot(t_x, t_y)
        self.ax_temp.set_title("Temperatura")

        self.ax_pressure.plot(p_x, p_y)
        self.ax_pressure.set_title("Ciśnienie")

        self.ax_altitude.plot(a_x, a_y)
        self.ax_altitude.set_title("Wysokość")

        self.figure.tight_layout()
        self.canvas.draw_idle()

app = QApplication(sys.argv)
window = SensorApp()
window.show()
sys.exit(app.exec_())